#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>

// Определяем структуру
#define FILENAME_SIZE 256
#define COMMAND_SIZE 4096
#define STATUS_SIZE 256

struct shared_data {
    char filename[FILENAME_SIZE];
    char command[COMMAND_SIZE];
    char status[STATUS_SIZE];
    int new_command;
    int division_by_zero;
    int parent_alive;
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        const char msg[] = "Error: usage: child <shm_name> <sem_name>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    const char *shm_name = argv[1];
    const char *sem_name = argv[2];

    // Открытие shared memory
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        const char msg[] = "Error: cannot open shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    // Отображение shared memory
    struct shared_data *shared = mmap(NULL, sizeof(struct shared_data), 
                                     PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        const char msg[] = "Error: cannot map shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Открытие семафора
    sem_t *semaphore = sem_open(sem_name, 0);
    if (semaphore == SEM_FAILED) {
        const char msg[] = "Error: cannot open semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        munmap(shared, sizeof(struct shared_data));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Получение имени файла
    const char *filename = shared->filename;
    
    // Открытие файла для результатов
    int output_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_file == -1) {
        const char msg[] = "Error: cannot open output file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        munmap(shared, sizeof(struct shared_data));
        close(shm_fd);
        sem_close(semaphore);
        exit(EXIT_FAILURE);
    }

    // Запись заголовка
    const char header[] = "Calculation Results:\n====================\n";
    write(output_file, header, sizeof(header) - 1);

    // Основной цикл обработки команд
    while (shared->parent_alive) {
        // Проверяем есть ли новая команда
        if (sem_wait(semaphore) == -1) {
            break;
        }
        
        int has_new_command = shared->new_command;
        char command[COMMAND_SIZE];
        
        if (has_new_command) {
            strncpy(command, shared->command, COMMAND_SIZE - 1);
            shared->new_command = 0;
        }
        
        sem_post(semaphore);
        
        if (!has_new_command) {
            usleep(100000);
            continue;
        }
        
        // Обработка команды
        float result = 0.0;
        int numbers_seen = 0;
        int division_by_zero = 0;
        int valid_input = 1;
        char status_msg[STATUS_SIZE];
        
        // Парсинг чисел из строки
        char *ptr = command;
        while (*ptr && valid_input) {
            while (*ptr && isspace((unsigned char)*ptr)) {
                ptr++;
            }
            if (!*ptr) {
                break;
            }

            int is_negative = 0;
            if (*ptr == '-') {
                is_negative = 1;
                ptr++;
            }

            float number = 0.0;
            int digits_found = 0;
            while (*ptr && isdigit((unsigned char)*ptr)) {
                number = number * 10.0 + (*ptr - '0');
                ptr++;
                digits_found = 1;
            }

            if (*ptr == '.') {
                ptr++;
                float fraction = 0.1;
                while (*ptr && isdigit((unsigned char)*ptr)) {
                    number += (*ptr - '0') * fraction;
                    fraction *= 0.1;
                    ptr++;
                    digits_found = 1;
                }
            }

            if (!digits_found) {
                while (*ptr && !isspace((unsigned char)*ptr)) ptr++;
                continue;
            }

            if (is_negative) {
                number = -number;
            }

            numbers_seen++;

            if (numbers_seen == 1) {
                result = number;
            } else {
                if (number == 0.0) {
                    division_by_zero = 1;
                    break;
                }
                result /= number;
            }

            while (*ptr && !isspace((unsigned char)*ptr)) {
                valid_input = 0;
                break;
            }
            if (!valid_input) {
                break;
            }
        }

        // Обработка результатов
        if (!valid_input) {
            snprintf(status_msg, sizeof(status_msg), "Error: invalid input format\n");
            
            char error_entry[128];
            int len = snprintf(error_entry, sizeof(error_entry), 
                             "Input: \"%s\" -> Error: invalid format\n", command);
            write(output_file, error_entry, len);
        } else if (division_by_zero) {
            snprintf(status_msg, sizeof(status_msg), "Error: division by zero\n");
            
            char error_entry[128];
            int len = snprintf(error_entry, sizeof(error_entry), 
                             "Input: \"%s\" -> Error: division by zero\n", command);
            write(output_file, error_entry, len);
        } else if (numbers_seen < 2) {
            snprintf(status_msg, sizeof(status_msg), "Error: not enough numbers (need at least 2)\n");
            
            char error_entry[128];
            int len = snprintf(error_entry, sizeof(error_entry), 
                             "Input: \"%s\" -> Error: not enough numbers\n", command);
            write(output_file, error_entry, len);
        } else {
            snprintf(status_msg, sizeof(status_msg), "Calculation completed successfully\n");
            
            char result_entry[128];
            int len = snprintf(result_entry, sizeof(result_entry), 
                             "Input: \"%s\" -> Result: %.6f\n", command, result);
            write(output_file, result_entry, len);
        }

        // Отправка статуса родителю
        if (sem_wait(semaphore) == -1) {
            break;
        }
        
        strncpy(shared->status, status_msg, STATUS_SIZE - 1);
        
        sem_post(semaphore);
        
        if (division_by_zero) {
            break;
        }
    }

    // Завершение
    const char footer[] = "\nEnd of calculations.\n";
    write(output_file, footer, sizeof(footer) - 1);
    
    close(output_file);
    munmap(shared, sizeof(struct shared_data));
    close(shm_fd);
    sem_close(semaphore);
    
    return 0;
}