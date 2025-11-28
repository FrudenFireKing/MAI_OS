#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include "../include/contract.h"

// Определения типов функций
typedef float (*cos_derivative_func)(float, float);
typedef float (*pi_func)(int);

typedef struct {
    void* handle;
    cos_derivative_func cos_derivative;
    pi_func pi;
    const char* name;
} Library;

void write_string(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

void print_usage(void) {
    write_string("Доступные команды:\n");
    write_string("0 - переключение между реализациями\n");
    write_string("1 a dx - вычисление производной cos(x) в точке a с приращением dx\n");
    write_string("2 k - вычисление числа pi с длиной ряда k\n");
}

// Функции парсинга (такие же как в linktime.c)
float parse_float(const char* str) {
    float result = 0.0f;
    float factor = 1.0f;
    int sign = 1;
    int decimal = 0;
    
    while (*str == ' ') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str) {
        if (*str == '.') {
            decimal = 1;
        } else if (isdigit(*str)) {
            if (decimal) {
                factor /= 10.0f;
                result += (*str - '0') * factor;
            } else {
                result = result * 10.0f + (*str - '0');
            }
        } else if (*str != ' ') {
            break;
        }
        str++;
    }
    
    return result * sign;
}

int parse_int(const char* str) {
    int result = 0;
    int sign = 1;
    
    while (*str == ' ') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str && isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

int parse_args(const char* input, float* arg1, float* arg2) {
    const char* ptr = input;
    
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 0;
    
    *arg1 = parse_float(ptr);
    
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 1;
    
    *arg2 = parse_float(ptr);
    
    return 2;
}

int parse_single_arg(const char* input, int* arg) {
    const char* ptr = input;
    
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 0;
    
    *arg = parse_int(ptr);
    
    return 1;
}

int load_library(Library* lib, const char* path, const char* name) {
    lib->handle = dlopen(path, RTLD_LAZY);
    if (!lib->handle) {
        write_string("Ошибка загрузки библиотеки ");
        write_string(name);
        write_string(": ");
        write_string(dlerror());
        write_string("\n");
        return 0;
    }
    
    lib->cos_derivative = (cos_derivative_func)dlsym(lib->handle, "cos_derivative");
    lib->pi = (pi_func)dlsym(lib->handle, "pi");
    
    if (!lib->cos_derivative || !lib->pi) {
        write_string("Ошибка загрузки функций из библиотеки ");
        write_string(name);
        write_string(": ");
        write_string(dlerror());
        write_string("\n");
        dlclose(lib->handle);
        return 0;
    }
    
    lib->name = name;
    write_string("Библиотека ");
    write_string(name);
    write_string(" успешно загружена\n");
    return 1;
}

void unload_library(Library* lib) {
    if (lib->handle) {
        dlclose(lib->handle);
        lib->handle = NULL;
        write_string("Библиотека ");
        write_string(lib->name);
        write_string(" выгружена\n");
    }
}

int main(void) {
    write_string("Программа с динамической загрузкой библиотек\n");
    
    Library lib1, lib2, *current_lib = NULL;
    int use_first_lib = 1;
    
    if (!load_library(&lib1, "./libmath_impl1.so", "Реализация 1 (Лейбниц/Прямая разность)")) {
        write_string("Не удалось загрузить первую библиотеку\n");
        return 1;
    }
    
    if (!load_library(&lib2, "./libmath_impl2.so", "Реализация 2 (Валлис/Центральная разность)")) {
        write_string("Не удалось загрузить вторую библиотеку\n");
        unload_library(&lib1);
        return 1;
    }
    
    current_lib = &lib1;
    write_string("Текущая библиотека: ");
    write_string(current_lib->name);
    write_string("\n");
    print_usage();
    
    char input[256];
    while (1) {
        write_string("\nВведите команду: ");
        
        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
        if (bytes_read <= 0) {
            break;
        }
        input[bytes_read] = '\0';
        
        char* newline = strchr(input, '\n');
        if (newline) *newline = '\0';
        
        if (strcmp(input, "0") == 0) {
            if (use_first_lib) {
                current_lib = &lib2;
                use_first_lib = 0;
            } else {
                current_lib = &lib1;
                use_first_lib = 1;
            }
            write_string("Переключено на библиотеку: ");
            write_string(current_lib->name);
            write_string("\n");
            continue;
        }
        
        if (input[0] == '1') {
            float a, dx;
            int args_count = parse_args(input, &a, &dx);
            
            if (args_count == 2) {
                if (dx == 0.0f) {
                    write_string("Ошибка: dx не может быть равно 0\n");
                } else {
                    float result = current_lib->cos_derivative(a, dx);
                    
                    char buffer[256];
                    int len = snprintf(buffer, sizeof(buffer),
                                     "Производная cos(x) в точке %.2f с dx=%.4f (%s): %.6f\n",
                                     a, dx, current_lib->name, result);
                    write(STDOUT_FILENO, buffer, len);
                }
            } else {
                write_string("Ошибка: неверные аргументы для команды 1. Используйте: 1 a dx\n");
            }
        }
        else if (input[0] == '2') {
            int k;
            int args_count = parse_single_arg(input, &k);
            
            if (args_count == 1) {
                if (k > 0) {
                    float result = current_lib->pi(k);
                    
                    char buffer[256];
                    int len = snprintf(buffer, sizeof(buffer),
                                     "Число pi с длиной ряда %d (%s): %.8f\n",
                                     k, current_lib->name, result);
                    write(STDOUT_FILENO, buffer, len);
                } else {
                    write_string("Ошибка: k должно быть положительным числом\n");
                }
            } else {
                write_string("Ошибка: неверные аргументы для команды 2. Используйте: 2 k\n");
            }
        }
        else {
            write_string("Неизвестная команда.\n");
            print_usage();
        }
    }
    
    unload_library(&lib1);
    unload_library(&lib2);
    
    return 0;
}