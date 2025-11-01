#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Ограничение на максимальный размер окна
#define MAX_WINDOW_SIZE 25

typedef struct {
    const int *matrix;
    int *result;
    int rows;
    int cols;
    int window_size;
    int start_row;
    int end_row;
} ThreadArgs;

void generate_matrix(int *matrix, int rows, int cols);
void median_filter_seq(int *matrix, int rows, int cols, int window_size, int k);
void *median_filter_worker(void *arg);
int median_filter_par(int *matrix, int rows, int cols, int window_size, int k, int num_threads);
int parse_int(const char *str, int *out);
double get_time_ms(void);
int read_matrix_from_file(const char *filename, int **matrix, int *rows, int *cols);
int write_matrix_to_file(const char *filename, const int *matrix, int rows, int cols);

void sort_array(int *arr, int n) {
    for (int i = 0; i < n - 1; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (arr[i] > arr[j]) {
                int tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

int apply_median_filter(const int *matrix, int rows, int cols, int r, int c, int w) {
    int size = w * w;
    int *values = malloc(size * sizeof(int));
    if (!values) return matrix[r * cols + c];

    int count = 0;
    int half = w / 2;

    for (int dr = -half; dr <= half; ++dr) {
        for (int dc = -half; dc <= half; ++dc) {
            int nr = r + dr;
            int nc = c + dc;
            if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                values[count++] = matrix[nr * cols + nc];
            }
        }
    }

    sort_array(values, count);
    int result = values[count / 2];
    free(values);
    return result;
}

void median_filter_seq(int *matrix, int rows, int cols, int window_size, int k) {
    int *temp = malloc((size_t)rows * (size_t)cols * sizeof(int));
    if (!temp) return;

    memcpy(temp, matrix, (size_t)rows * (size_t)cols * sizeof(int));

    for (int iter = 0; iter < k; ++iter) {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                matrix[r * cols + c] = apply_median_filter(temp, rows, cols, r, c, window_size);
            }
        }
        memcpy(temp, matrix, (size_t)rows * (size_t)cols * sizeof(int));
    }
    free(temp);
}

void *median_filter_worker(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    for (int r = args->start_row; r < args->end_row; ++r) {
        for (int c = 0; c < args->cols; ++c) {
            args->result[r * args->cols + c] =
                apply_median_filter(args->matrix, args->rows, args->cols, r, c, args->window_size);
        }
    }
    return NULL;
}

int median_filter_par(int *matrix, int rows, int cols, int window_size, int k, int num_threads) {
    int *temp = malloc((size_t)rows * (size_t)cols * sizeof(int));
    int *result = malloc((size_t)rows * (size_t)cols * sizeof(int));
    pthread_t *threads = malloc((size_t)num_threads * sizeof(pthread_t));
    ThreadArgs *targs = malloc((size_t)num_threads * sizeof(ThreadArgs));

    if (!temp || !result || !threads || !targs) {
        free(temp); free(result); free(threads); free(targs);
        return -1;
    }

    memcpy(temp, matrix, (size_t)rows * (size_t)cols * sizeof(int));
    for (int iter = 0; iter < k; ++iter) {
        int rows_per_thread = rows / num_threads;
        int extra_rows = rows % num_threads;

        for (int t = 0; t < num_threads; ++t) {
            targs[t] = (ThreadArgs){
                .matrix = temp,
                .result = result,
                .rows = rows,
                .cols = cols,
                .window_size = window_size,
                .start_row = t * rows_per_thread,
                .end_row = (t + 1) * rows_per_thread
            };
            if (t == num_threads - 1) {
                targs[t].end_row += extra_rows;
            }
            int ret = pthread_create(&threads[t], NULL, median_filter_worker, &targs[t]);
            if (ret != 0) {
                free(temp); free(result); free(threads); free(targs);
                return ret;
            }
        }

        for (int t = 0; t < num_threads; ++t) {
            pthread_join(threads[t], NULL);
        }
        memcpy(temp, result, (size_t)rows * (size_t)cols * sizeof(int));
    }
    memcpy(matrix, temp, (size_t)rows * (size_t)cols * sizeof(int));

    free(temp);
    free(result);
    free(threads);
    free(targs);
    return 0;
}

void generate_matrix(int *matrix, int rows, int cols) {
    for (int i = 0; i < rows * cols; ++i) {
        matrix[i] = rand() % 100;
    }
}

int parse_int(const char *str, int *out) {
    if (!str || !*str) return -1;

    *out = 0;
    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    }
    while (*str) {
        if (*str < '0' || *str > '9') return -1;
        *out = *out * 10 + (*str - '0');
        str++;
    }
    *out *= sign;
    return 0;
}

double get_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

// Чтение матрицы из файла
int read_matrix_from_file(const char *filename, int **matrix, int *rows, int *cols) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        const char msg[] = "Error: Cannot open input file\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        return -1;
    }

    // Read dimensions
    if (fscanf(file, "%d %d", rows, cols) != 2) {
        const char msg[] = "Error: Invalid file format (dimensions)\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        fclose(file);
        return -1;
    }

    if (*rows <= 0 || *cols <= 0) {
        const char msg[] = "Error: Matrix dimensions must be positive\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        fclose(file);
        return -1;
    }

    // Allocate memory
    *matrix = malloc((size_t)(*rows) * (size_t)(*cols) * sizeof(int));
    if (!*matrix) {
        const char msg[] = "Error: Memory allocation failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        fclose(file);
        return -1;
    }

    // Read matrix data
    int idx = 0;
    while (idx < (*rows) * (*cols) && fscanf(file, "%d", &(*matrix)[idx]) == 1) {
        idx++;
    }

    fclose(file);

    if (idx != (*rows) * (*cols)) {
        const char msg[] = "Error: Incomplete matrix data\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        free(*matrix);
        *matrix = NULL;
        return -1;
    }

    return 0;
}


int write_matrix_to_file(const char *filename, const int *matrix, int rows, int cols) {
    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == -1) {
        const char msg[] = "Error: Cannot create output file '";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        write(STDERR_FILENO, filename, strlen(filename));
        const char end[] = "'\n";
        write(STDERR_FILENO, end, sizeof(end) - 1);
        return -1;
    }

    // Формируем первую строку: "rows cols\n"
    char header[64];
    int len = snprintf(header, sizeof(header), "%d %d\n", rows, cols);
    if (len < 0 || len >= (int)sizeof(header)) {
        const char msg[] = "Error: Header too long\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        return -1;
    }
    if (write(file, header, len) != len) {
        const char msg[] = "Error: Failed to write header\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        close(file);
        return -1;
    }

    // Записываем элементы матрицы построчно
    for (int i = 0; i < rows; ++i) {
        char line[1024] = {0};
        int pos = 0;
        for (int j = 0; j < cols; ++j) {
            int val = matrix[i * cols + j];
            char num[16];
            int nlen = snprintf(num, sizeof(num), "%d", val);
            if (nlen < 0 || nlen >= (int)sizeof(num)) {
                const char msg[] = "Error: Number too long\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                close(file);
                return -1;
            }
            // Добавляем пробел, если не последний элемент
            if (j < cols - 1) {
                num[nlen++] = ' ';
            }
            memcpy(line + pos, num, nlen);
            pos += nlen;
        }
        line[pos++] = '\n';
        if (write(file, line, pos) != pos) {
            const char msg[] = "Error: Failed to write row\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            close(file);
            return -1;
        }
    }

    close(file);
    return 0;
}

int main(int argc, char **argv) {
    int rows = 20;
    int cols = 20;
    int window_size = 3;
    int k = 1;
    int num_threads = 1;
    char *input_file = NULL;
    char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            if (parse_int(argv[i + 1], &num_threads) != 0 || num_threads <= 0) {
                const char msg[] = "Error: Invalid number of threads\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            if (parse_int(argv[i + 1], &k) != 0 || k <= 0) {
                const char msg[] = "Error: Invalid iterations count\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
                if (parse_int(argv[i + 1], &window_size) != 0 || 
                    window_size <= 0 || window_size % 2 == 0 ||
                    window_size > MAX_WINDOW_SIZE) {
                    const char msg[] = "Error: Window size must be positive odd integer ≤ 25\n";
                    write(STDERR_FILENO, msg, sizeof(msg) - 1);
                    return 1;
                }
                i++;
            } else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc) {
                input_file = argv[i + 1];
                i++;
            } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                output_file = argv[i + 1];
                i++;
            } else {
                const char msg[] = "Usage: ./program [-t threads] [-k iters] [-w window] [-i input] [-o output]\n";
                write(STDERR_FILENO, msg, sizeof(msg) - 1);
                return 1;
            }
        }

    int *matrix = NULL;

    // Чтение матрицы из файла или генерация случайной
    if (input_file) {
        if (read_matrix_from_file(input_file, &matrix, &rows, &cols) != 0) {
            return 1;
        }
    } else {
        matrix = malloc((size_t)rows * (size_t)cols * sizeof(int));
        if (!matrix) {
            const char msg[] = "Error: Memory allocation failed for matrix\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return 1;
        }
        generate_matrix(matrix, rows, cols);
    }

    double start_time = get_time_ms();

    // Применение фильтра
    int result = median_filter_par(matrix, rows, cols, window_size, k, num_threads);
    if (result != 0) {
        const char msg[] = "Error: Median filter failed\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        free(matrix);
        return 1;
    }

    double end_time = get_time_ms();
    double elapsed = end_time - start_time;

    // Запись результата в файл
    if (output_file) {
        if (write_matrix_to_file(output_file, matrix, rows, cols) != 0) {
            free(matrix);
            return 1;
        }
    }

    // Вывод метрик через write
    char time_msg[128];
    int len = snprintf(time_msg, sizeof(time_msg),
                     "Processing time: %.2f ms\n", elapsed);
    if (len > 0 && len < (int)sizeof(time_msg)) {
        write(STDOUT_FILENO, time_msg, len);
    }

    char params_msg[256];
    len = snprintf(params_msg, sizeof(params_msg),
                 "Parameters: rows=%d cols=%d window=%d k=%d threads=%d\n",
                 rows, cols, window_size, k, num_threads);
    if (len > 0 && len < (int)sizeof(params_msg)) {
        write(STDOUT_FILENO, params_msg, len);
    }

    free(matrix);
    return 0;
}
