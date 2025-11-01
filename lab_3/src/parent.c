#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>

// Определяем структуру прямо здесь, если нет отдельного .h файла
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

// Глобальные переменные для cleanup
static struct shared_data *shared = NULL;
static sem_t *semaphore = NULL;
static int shm_fd = -1;
static char shm_name[256];
static char sem_name[256];

void cleanup_resources() {
    if (shared) {
        shared->parent_alive = 0;
    }
    
    if (semaphore) {
        sem_close(semaphore);
    }
    
    if (shared) {
        munmap(shared, sizeof(struct shared_data));
    }
    
    if (shm_fd != -1) {
        close(shm_fd);
        shm_unlink(shm_name);
    }
    
    sem_unlink(sem_name);
}

void signal_handler(int sig) {
    cleanup_resources();
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    char filename[1024];
    
    // Запрос имени файла у пользователя
    {
        const char msg[] = "Enter output filename: ";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);

        ssize_t n = read(STDIN_FILENO, filename, sizeof(filename) - 1);
        if (n <= 0) {
            const char msg[] = "Error: cannot read filename\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        filename[n - 1] = '\0';
    }

    // Создание уникальных имен
    pid_t parent_pid = getpid();
    snprintf(shm_name, sizeof(shm_name), "/lab3_shm_%d", parent_pid);
    snprintf(sem_name, sizeof(sem_name), "/lab3_sem_%d", parent_pid);

    // Создание shared memory
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        const char msg[] = "Error: cannot create shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }

    // Установка размера
    if (ftruncate(shm_fd, sizeof(struct shared_data)) == -1) {
        const char msg[] = "Error: cannot set shared memory size\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    // Отображение shared memory
    shared = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, 
                  MAP_SHARED, shm_fd, 0);
    if (shared == MAP_FAILED) {
        const char msg[] = "Error: cannot map shared memory\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    // Инициализация shared memory
    memset(shared, 0, sizeof(struct shared_data));
    strncpy(shared->filename, filename, FILENAME_SIZE - 1);
    shared->new_command = 0;
    shared->division_by_zero = 0;
    shared->parent_alive = 1;

    // Создание семафора
    semaphore = sem_open(sem_name, O_CREAT, 0666, 1);
    if (semaphore == SEM_FAILED) {
        const char msg[] = "Error: cannot create semaphore\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        cleanup_resources();
        exit(EXIT_FAILURE);
    }

    // Создание дочернего процесса
    pid_t pid = fork();

    switch(pid) {
        case -1: {
            const char msg[] = "Error: cannot create child process\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            cleanup_resources();
            exit(EXIT_FAILURE);
        }
        case 0: {
            // Дочерний процесс
            execl("./child", "child", shm_name, sem_name, NULL);
            
            const char msg[] = "Error: cannot execute child process\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            exit(EXIT_FAILURE);
        }
        default: {
            // Родительский процесс
            const char prompt[] = "Enter numbers separated by spaces (or 'exit' to quit):\n";
            write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);

            char buffer[4096];
            int child_alive = 1;

            while (child_alive && shared->parent_alive) {
                const char input_prompt[] = "> ";
                write(STDOUT_FILENO, input_prompt, sizeof(input_prompt) - 1);

                ssize_t n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
                if (n <= 0) break;

                buffer[n - 1] = '\0';

                if (strcmp(buffer, "exit") == 0) {
                    break;
                }

                if (strlen(buffer) == 0) {
                    continue;
                }

                // Захватываем семафор для записи команды
                if (sem_wait(semaphore) == -1) {
                    const char msg[] = "Error: sem_wait failed\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    break;
                }

                // Записываем команду в shared memory
                strncpy(shared->command, buffer, COMMAND_SIZE - 1);
                shared->new_command = 1;
                memset(shared->status, 0, STATUS_SIZE);

                // Освобождаем семафор
                if (sem_post(semaphore) == -1) {
                    const char msg[] = "Error: sem_post failed\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    break;
                }

                // Ожидаем ответ от дочернего процесса
                int attempts = 0;
                const int max_attempts = 100;
                
                while (attempts < max_attempts) {
                    usleep(100000);
                    
                    if (sem_wait(semaphore) == -1) {
                        const char msg[] = "Error: sem_wait failed\n";
                        write(STDERR_FILENO, msg, sizeof(msg) - 1);
                        break;
                    }
                    
                    if (strlen(shared->status) > 0) {
                        write(STDERR_FILENO, shared->status, strlen(shared->status));
                        
                        if (strstr(shared->status, "division by zero") != NULL) {
                            const char error_msg[] = "Error: division by zero detected. Terminating...\n";
                            write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
                            child_alive = 0;
                            shared->division_by_zero = 1;
                        }
                        
                        memset(shared->status, 0, STATUS_SIZE);
                        sem_post(semaphore);
                        break;
                    }
                    
                    sem_post(semaphore);
                    attempts++;
                }
                
                if (attempts >= max_attempts) {
                    const char timeout_msg[] = "Error: child process timeout\n";
                    write(STDERR_FILENO, timeout_msg, sizeof(timeout_msg) - 1);
                }
            }

            shared->parent_alive = 0;

            int status;
            wait(&status);

            const char exit_msg[] = "Parent process terminated.\n";
            write(STDOUT_FILENO, exit_msg, sizeof(exit_msg) - 1);
            
            cleanup_resources();
        }
    }

    return 0;
}