#include <math.h>
#include "../include/contract.h"

// Реализация №1 производной: (f(a + dx) - f(a)) / dx
float cos_derivative(float a, float dx) {
    return (cosf(a + dx) - cosf(a)) / dx;
}

// Реализация №1 числа pi: Ряд Лейбница
float pi(int k) {
    if (k <= 0) return 0.0f;
    
    float result = 0.0f;
    for (int i = 0; i < k; i++) {
        float term = (i % 2 == 0) ? 1.0f : -1.0f;
        term /= (2 * i + 1);
        result += term;
    }
    return result * 4;
}