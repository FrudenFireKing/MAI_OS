#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/contract.h"

void print_usage() {
    printf("Доступные команды:\n");
    printf("0 - выход из программы\n");
    printf("1 a dx - вычисление производной cos(x) в точке a с приращением dx\n");
    printf("2 k - вычисление числа pi с длиной ряда k\n");
}

int main() {
    printf("Программа со статической линковкой библиотеки\n");
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
            printf("Выход из программы.\n");
            break;
        }
        
        // Обработка команды для производной
        if (input[0] == '1') {
            float a, dx;
            if (sscanf(input + 1, "%f %f", &a, &dx) == 2) {
                float result = cos_derivative(a, dx);
                printf("Производная cos(x) в точке %.2f с dx=%.4f: %.6f\n", a, dx, result);
            } else {
                printf("Ошибка: неверные аргументы для команды 1. Используйте: 1 a dx\n");
            }
        }
        // Обработка команды для числа π
        else if (input[0] == '2') {
            int k;
            if (sscanf(input + 1, "%d", &k) == 1) {
                if (k > 0) {
                    float result = pi(k);
                    printf("Число pi с длиной ряда %d: %.8f\n", k, result);
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
    
    return 0;
}