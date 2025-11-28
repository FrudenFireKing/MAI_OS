#include <math.h>
#include "../include/contract.h"

// Реализация №2 производной: (f(a + dx) - f(a - dx)) / (2 * dx)
float cos_derivative(float a, float dx) {
    return (cosf(a + dx) - cosf(a - dx)) / (2 * dx);
}

// Реализация №2 числа pi: Формула Валлиса
float pi(int k) {
    if (k <= 0) return 0.0f;
    
    float result = 1.0f;
    for (int i = 1; i <= k; i++) {
        float numerator = 4.0f * i * i;
        float denominator = 4.0f * i * i - 1.0f;
        result *= numerator / denominator;
    }
    return result * 2;
}