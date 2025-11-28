#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../include/contract.h"

void write_string(const char* str) {
    write(STDOUT_FILENO, str, strlen(str));
}

void write_float(float value) {
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%.6f", value);
    write(STDOUT_FILENO, buffer, len);
}

void write_int(int value) {
    char buffer[32];
    int len = snprintf(buffer, sizeof(buffer), "%d", value);
    write(STDOUT_FILENO, buffer, len);
}

void print_usage(void) {
    write_string("Доступные команды:\n");
    write_string("0 - выход из программы\n");
    write_string("1 a dx - вычисление производной cos(x) в точке a с приращением dx\n");
    write_string("2 k - вычисление числа pi с длиной ряда k\n");
}

// Функция для парсинга float из строки
float parse_float(const char* str) {
    float result = 0.0f;
    float factor = 1.0f;
    int sign = 1;
    int decimal = 0;
    
    // Пропускаем пробелы
    while (*str == ' ') str++;
    
    // Обрабатываем знак
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Парсим целую и дробную части
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

// Функция для парсинга int из строки
int parse_int(const char* str) {
    int result = 0;
    int sign = 1;
    
    // Пропускаем пробелы
    while (*str == ' ') str++;
    
    // Обрабатываем знак
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Парсим число
    while (*str && isdigit(*str)) {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// Функция для извлечения аргументов из строки
int parse_args(const char* input, float* arg1, float* arg2) {
    const char* ptr = input;
    
    // Пропускаем команду
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 0;
    
    // Парсим первый аргумент
    *arg1 = parse_float(ptr);
    
    // Ищем второй аргумент
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 1;
    
    // Парсим второй аргумент
    *arg2 = parse_float(ptr);
    
    return 2;
}

int parse_single_arg(const char* input, int* arg) {
    const char* ptr = input;
    
    // Пропускаем команду
    while (*ptr && !isspace(*ptr)) ptr++;
    while (*ptr && isspace(*ptr)) ptr++;
    
    if (!*ptr) return 0;
    
    // Парсим аргумент
    *arg = parse_int(ptr);
    
    return 1;
}

int main(void) {
    write_string("Программа со статической линковкой библиотеки\n");
    print_usage();
    
    char input[256];
    while (1) {
        write_string("\nВведите команду: ");
        
        int bytes_read = read(STDIN_FILENO, input, sizeof(input) - 1);
        if (bytes_read <= 0) {
            break;
        }
        input[bytes_read] = '\0';
        
        // Удаляем символ новой строки
        char* newline = strchr(input, '\n');
        if (newline) *newline = '\0';
        
        if (strcmp(input, "0") == 0) {
            write_string("Выход из программы.\n");
            break;
        }
        
        // Обработка команды для производной
        if (input[0] == '1') {
            float a, dx;
            int args_count = parse_args(input, &a, &dx);
            
            if (args_count == 2) {
                if (dx == 0.0f) {
                    write_string("Ошибка: dx не может быть равно 0\n");
                } else {
                    float result = cos_derivative(a, dx);
                    
                    char buffer[256];
                    int len = snprintf(buffer, sizeof(buffer), 
                                     "Производная cos(x) в точке %.2f с dx=%.4f: %.6f\n", 
                                     a, dx, result);
                    write(STDOUT_FILENO, buffer, len);
                }
            } else {
                write_string("Ошибка: неверные аргументы для команды 1. Используйте: 1 a dx\n");
            }
        }
        // Обработка команды для числа π
        else if (input[0] == '2') {
            int k;
            int args_count = parse_single_arg(input, &k);
            
            if (args_count == 1) {
                if (k > 0) {
                    float result = pi(k);
                    
                    char buffer[256];
                    int len = snprintf(buffer, sizeof(buffer), 
                                     "Число pi с длиной ряда %d: %.8f\n", k, result);
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
    
    return 0;
}