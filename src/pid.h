#ifndef PID_H
#define PID_H

#include <stdint.h>

// Struktura regulatora
typedef struct {
    // Nastawy (Gains)
    float Kp;  // Człon proporcjonalny (reakcja na błąd teraz)
    float Ki;  // Człon całkujący (korekta błędów z przeszłości)
    float Kd;  // Człon różniczkujący (przewidywanie przyszłości)

    // Limity wyjścia (np. PWM 0-100%)
    float out_min;
    float out_max;

    // Pamięć regulatora (zmienne wewnętrzne)
    float integral_sum; // Suma błędów (dla członu I)
    float prev_error;   // Poprzedni błąd (dla członu D)
} PID_Context;

// Inicjalizacja nastaw
void pid_init(PID_Context *ctx, float kp, float ki, float kd, float min, float max);

// Główna funkcja licząca (wywoływana cyklicznie)
// setpoint = wartość zadana (np. 80 km/h)
// measurement = wartość mierzona (np. 75 km/h)
// Zwraca: sterowanie (np. jak mocno wcisnąć gaz 0-100)
float pid_update(PID_Context *ctx, float setpoint, float measurement);

// Reset (np. przy zatrzymaniu silnika)
void pid_reset(PID_Context *ctx);

#endif