#ifndef CONTRACT_H
#define CONTRACT_H

#ifdef __cplusplus
extern "C" {
#endif

// Контракт для расчета производной cos(x)
float cos_derivative(float a, float dx);

// Контракт для расчета числа ?
float pi(int k);

#ifdef __cplusplus
}
#endif

#endif