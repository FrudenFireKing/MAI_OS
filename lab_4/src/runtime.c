#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void print_usage() {
    printf("Доступные команды:\n");
    printf("0 - переключение между реализациями\n");
    printf("1 a dx - вычисление производной cos(x) в точке a с приращением dx\n");
    printf("2 k - вычисление числа pi с длиной ряда k\n");
}

int load_library(Library* lib, const char* path, const char* name) {
    lib->handle = dlopen(path, RTLD_LAZY);
    if (!lib->handle) {
        fprintf(stderr, "Ошибка загрузки библиотеки %s: %s\n", name, dlerror());
        return 0;
    }
    
    // Загружаем функции
    lib->cos_derivative = (cos_derivative_func)dlsym(lib->handle, "cos_derivative");
    lib->pi = (pi_func)dlsym(lib->handle, "pi");
    
    if (!lib->cos_derivative || !lib->pi) {
        fprintf(stderr, "Ошибка загрузки функций из библиотеки %s: %s\n", name, dlerror());
        dlclose(lib->handle);
        return 0;
    }
    
    lib->name = name;
    printf("Библиотека %s успешно загружена\n", name);
    return 1;
}

void unload_library(Library* lib) {
    if (lib->handle) {
        dlclose(lib->handle);
        lib->handle = NULL;
        printf("Библиотека %s выгружена\n", lib->name);
    }
}

int main() {
    printf("Программа с динамической загрузкой библиотек\n");
    
    Library lib1, lib2, *current_lib = NULL;
    int use_first_lib = 1;
    
    // Загружаем обе библиотеки
    if (!load_library(&lib1, "./lib/libmath_impl1.so", "Реализация 1 (Лейбниц/Прямая разность)")) {
        printf("Не удалось загрузить первую библиотеку\n");
        return 1;
    }
    
    if (!load_library(&lib2, "./lib/libmath_impl2.so", "Реализация 2 (Валлис/Центральная разность)")) {
        printf("Не удалось загрузить вторую библиотеку\n");
        unload_library(&lib1);
        return 1;
    }
    
    current_lib = &lib1;
    printf("Текущая библиотека: %s\n", current_lib->name);
    print_usage();
    
    char input[256];
    while (1) {
        printf("\nВведите команду: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Удаляем символ новой строки
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "0") == 0) {
            // Переключаем библиотеки
            if (use_first_lib) {
                current_lib = &lib2;
                use_first_lib = 0;
            } else {
                current_lib = &lib1;
                use_first_lib = 1;
            }
            printf("Переключено на библиотеку: %s\n", current_lib->name);
            continue;
        }
        
        // Обработка команды для производной
        if (input[0] == '1') {
            float a, dx;
            if (sscanf(input + 1, "%f %f", &a, &dx) == 2) {
                float result = current_lib->cos_derivative(a, dx);
                printf("Производная cos(x) в точке %.2f с dx=%.4f (%s): %.6f\n", 
                       a, dx, current_lib->name, result);
            } else {
                printf("Ошибка: неверные аргументы для команды 1. Используйте: 1 a dx\n");
            }
        }
        // Обработка команды для числа π
        else if (input[0] == '2') {
            int k;
            if (sscanf(input + 1, "%d", &k) == 1) {
                if (k > 0) {
                    float result = current_lib->pi(k);
                    printf("Число pi с длиной ряда %d (%s): %.8f\n", 
                           k, current_lib->name, result);
                } else {
                    printf("Ошибка: k должно быть положительным числом\n");
                }
            } else {
                printf("Ошибка: неверные аргументы для команды 2. Используйте: 2 k\n");
            }
        }
        else {
            printf("Неизвестная команда.\n");
            print_usage();
        }
    }
    
    // Выгружаем библиотеки
    unload_library(&lib1);
    unload_library(&lib2);
    
    return 0;
}