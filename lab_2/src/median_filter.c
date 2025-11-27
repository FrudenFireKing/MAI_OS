///usr/bin/cc -o /tmp/median_filter -pthread $0 && exec /tmp/median_filter "$@"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

// Структура для матрицы
typedef struct {
    int **data;
    int width;
    int height;
} matrix_t;

// Аргументы для потоков
typedef struct {
    int thread_id;
    int total_threads;
    matrix_t *src_matrix;
    matrix_t *dst_matrix;
    int window_size;
    int k_iters;
    pthread_barrier_t *barrier;
} thread_args_t;

// Глобальные переменные для параметров
static int num_threads = 1;
static int k_iters = 1;
static int window_size = 3;
static char *input_file = NULL;
static char *output_file = NULL;

// Функция для создания матрицы
matrix_t* create_matrix(int width, int height) {
    matrix_t *matrix = malloc(sizeof(matrix_t));
    matrix->width = width;
    matrix->height = height;
    
    matrix->data = malloc(height * sizeof(int*));
    for (int i = 0; i < height; i++) {
        matrix->data[i] = malloc(width * sizeof(int));
    }
    
    return matrix;
}

// Функция для освобождения матрицы
void free_matrix(matrix_t *matrix) {
    if (!matrix) return;
    
    for (int i = 0; i < matrix->height; i++) {
        free(matrix->data[i]);
    }
    free(matrix->data);
    free(matrix);
}

// Функция для копирования матрицы
matrix_t* copy_matrix(const matrix_t *src) {
    matrix_t *dst = create_matrix(src->width, src->height);
    
    for (int y = 0; y < src->height; y++) {
        for (int x = 0; x < src->width; x++) {
            dst->data[y][x] = src->data[y][x];
        }
    }
    
    return dst;
}

// Функция для чтения матрицы из файла
matrix_t* read_matrix(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "Error: Cannot open file %s\n", filename);
        return NULL;
    }
    
    int width, height;
    if (fscanf(file, "%d %d", &height, &width) != 2) {
        fprintf(stderr, "Error: Invalid file format\n");
        fclose(file);
        return NULL;
    }
    
    matrix_t *matrix = create_matrix(width, height);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (fscanf(file, "%d", &matrix->data[y][x]) != 1) {
                fprintf(stderr, "Error: Invalid data at position (%d, %d)\n", y, x);
                fclose(file);
                free_matrix(matrix);
                return NULL;
            }
        }
    }
    
    fclose(file);
    return matrix;
}

// Функция для записи матрицы в файл
bool write_matrix(const char *filename, const matrix_t *matrix) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        fprintf(stderr, "Error: Cannot create file %s\n", filename);
        return false;
    }
    
    fprintf(file, "%d %d\n", matrix->height, matrix->width);
    
    for (int y = 0; y < matrix->height; y++) {
        for (int x = 0; x < matrix->width; x++) {
            fprintf(file, "%d", matrix->data[y][x]);
            if (x < matrix->width - 1) fprintf(file, " ");
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return true;
}

// Функция для применения медианного фильтра к одной точке
int apply_median_filter(const matrix_t *matrix, int x, int y, int window_size) {
    int radius = window_size / 2;
    int window[window_size * window_size];
    int count = 0;
    
    // Собираем значения из окна
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            int ny = y + dy;
            int nx = x + dx;
            
            if (ny >= 0 && ny < matrix->height && nx >= 0 && nx < matrix->width) {
                window[count++] = matrix->data[ny][nx];
            }
        }
    }
    
    // Сортировка пузырьком (простая реализация)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (window[j] > window[j + 1]) {
                int temp = window[j];
                window[j] = window[j + 1];
                window[j + 1] = temp;
            }
        }
    }
    
    // Возвращаем медиану
    return window[count / 2];
}

// Функция для одной итерации медианного фильтра (без граничных пикселей)
void apply_median_filter_iteration(const matrix_t *src, matrix_t *dst, int window_size, 
                                  int start_row, int end_row) {
    int radius = window_size / 2;
    
    for (int y = start_row; y < end_row; y++) {
        for (int x = radius; x < src->width - radius; x++) {
            dst->data[y][x] = apply_median_filter(src, x, y, window_size);
        }
    }
}

// ПОСЛЕДОВАТЕЛЬНАЯ ВЕРСИЯ
matrix_t* median_filter_sequential(matrix_t *input, int window_size, int k_iters) {
    matrix_t *current = copy_matrix(input);
    matrix_t *next = create_matrix(input->width, input->height);
    
    int radius = window_size / 2;
    
    for (int iter = 0; iter < k_iters; iter++) {
        apply_median_filter_iteration(current, next, window_size, radius, current->height - radius);
        
        // Меняем местами текущую и следующую матрицу
        matrix_t *temp = current;
        current = next;
        next = temp;
    }
    
    free_matrix(next);
    return current;
}

// Функция рабочего потока для параллельной версии
static void* worker_thread(void *_args) {
    thread_args_t *args = (thread_args_t*)_args;
    int radius = args->window_size / 2;
    
    // Определяем свою полосу строк
    int rows_per_thread = args->src_matrix->height / args->total_threads;
    int start_row = args->thread_id * rows_per_thread;
    int end_row = (args->thread_id == args->total_threads - 1) ? 
                  args->src_matrix->height : start_row + rows_per_thread;
    
    // Учитываем радиус окна
    start_row = (start_row < radius) ? radius : start_row;
    end_row = (end_row > args->src_matrix->height - radius) ? 
              args->src_matrix->height - radius : end_row;
    
    matrix_t *current = args->src_matrix;
    matrix_t *next = args->dst_matrix;
    
    for (int iter = 0; iter < args->k_iters; iter++) {
        // Обрабатываем свою полосу
        apply_median_filter_iteration(current, next, args->window_size, start_row, end_row);
        
        // Синхронизация в барьере
        pthread_barrier_wait(args->barrier);
        
        // Главный поток меняет матрицы, все ждут
        pthread_barrier_wait(args->barrier);
    }
    
    return NULL;
}

// ПАРАЛЛЕЛЬНАЯ ВЕРСИЯ
matrix_t* median_filter_parallel(matrix_t *input, int window_size, int k_iters, int num_threads) {
    matrix_t *current = copy_matrix(input);
    matrix_t *next = create_matrix(input->width, input->height);
    
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    thread_args_t *args = malloc(num_threads * sizeof(thread_args_t));
    pthread_barrier_t barrier;
    
    // Инициализируем барьер
    pthread_barrier_init(&barrier, NULL, num_threads);
    
    // Создаем рабочие потоки
    for (int i = 0; i < num_threads; i++) {
        args[i] = (thread_args_t){
            .thread_id = i,
            .total_threads = num_threads,
            .src_matrix = current,
            .dst_matrix = next,
            .window_size = window_size,
            .k_iters = k_iters,
            .barrier = &barrier
        };
        
        pthread_create(&threads[i], NULL, worker_thread, &args[i]);
    }
    
    // Главный поток управляет итерациями
    for (int iter = 0; iter < k_iters; iter++) {
        // Ждем завершения обработки всеми потоками
        pthread_barrier_wait(&barrier);
        
        // Меняем местами матрицы
        matrix_t *temp = current;
        current = next;
        next = temp;
        
        // Обновляем указатели в аргументах для следующей итерации
        for (int i = 0; i < num_threads; i++) {
            args[i].src_matrix = current;
            args[i].dst_matrix = next;
        }
        
        // Разрешаем потокам начать следующую итерацию
        pthread_barrier_wait(&barrier);
    }
    
    // Ожидаем завершения всех потоков
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_barrier_destroy(&barrier);
    free(args);
    free(threads);
    free_matrix(next);
    
    return current;
}

// Функция для замера времени в миллисекундах
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// Функция для вывода справки
void print_usage(const char *program_name) {
    printf("Usage: %s -t <threads> -k <iterations> -w <window_size> -i <input> -o <output>\n", program_name);
    printf("Options:\n");
    printf("  -t <threads>     Number of threads (default: 1)\n");
    printf("  -k <iterations>  Number of filter iterations (default: 1)\n");
    printf("  -w <window_size> Filter window size (default: 3)\n");
    printf("  -i <input>       Input file with matrix\n");
    printf("  -o <output>      Output file for result\n");
}

// Функция для парсинга аргументов командной строки
bool parse_arguments(int argc, char **argv) {
    int opt;
    while ((opt = getopt(argc, argv, "t:k:w:i:o:")) != -1) {
        switch (opt) {
            case 't':
                num_threads = atoi(optarg);
                if (num_threads <= 0) {
                    fprintf(stderr, "Error: Number of threads must be positive\n");
                    return false;
                }
                break;
            case 'k':
                k_iters = atoi(optarg);
                if (k_iters <= 0) {
                    fprintf(stderr, "Error: Number of iterations must be positive\n");
                    return false;
                }
                break;
            case 'w':
                window_size = atoi(optarg);
                if (window_size <= 0 || window_size % 2 == 0) {
                    fprintf(stderr, "Error: Window size must be positive and odd\n");
                    return false;
                }
                break;
            case 'i':
                input_file = optarg;
                break;
            case 'o':
                output_file = optarg;
                break;
            default:
                return false;
        }
    }
    
    if (!input_file || !output_file) {
        fprintf(stderr, "Error: Input and output files are required\n");
        return false;
    }
    
    return true;
}

int main(int argc, char **argv) {
    if (!parse_arguments(argc, argv)) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Читаем входную матрицу
    matrix_t *input = read_matrix(input_file);
    if (!input) {
        return 1;
    }
    
    printf("Matrix: %dx%d, Threads: %d, Iterations: %d, Window: %dx%d\n",
           input->height, input->width, num_threads, k_iters, window_size, window_size);
    
    matrix_t *result;
    long long start_time, end_time;
    
    if (num_threads == 1) {
        // Последовательная версия
        printf("Running sequential version...\n");
        start_time = get_time_ms();
        result = median_filter_sequential(input, window_size, k_iters);
        end_time = get_time_ms();
    } else {
        // Параллельная версия
        printf("Running parallel version with %d threads...\n", num_threads);
        start_time = get_time_ms();
        result = median_filter_parallel(input, window_size, k_iters, num_threads);
        end_time = get_time_ms();
    }
    
    long long execution_time = end_time - start_time;
    printf("Execution time: %lld ms\n", execution_time);
    
    // Записываем результат
    if (!write_matrix(output_file, result)) {
        free_matrix(input);
        free_matrix(result);
        return 1;
    }
    
    printf("Result written to %s\n", output_file);
    
    // Очистка памяти
    free_matrix(input);
    free_matrix(result);
    
    return 0;
}