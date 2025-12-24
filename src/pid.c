#include "pid.h"

void pid_init(PID_Context *ctx, float kp, float ki, float kd, float min, float max) {
    ctx->Kp = kp;
    ctx->Ki = ki;
    ctx->Kd = kd;
    ctx->out_min = min;
    ctx->out_max = max;
    pid_reset(ctx);
}

void pid_reset(PID_Context *ctx) {
    ctx->integral_sum = 0.0f;
    ctx->prev_error = 0.0f;
}

float pid_update(PID_Context *ctx, float setpoint, float measurement) {
    // 1. Oblicz uchyb (Błąd = Cel - Rzeczywistość)
    float error = setpoint - measurement;

    // 2. Człon Proporcjonalny (P)
    float p_term = ctx->Kp * error;

    // 3. Człon Całkujący (I) z zabezpieczeniem (Anti-windup)
    ctx->integral_sum += error;
    float i_term = ctx->Ki * ctx->integral_sum;
    
    // Proste zabezpieczenie przed nasyceniem całki
    // (żeby nie urosła do nieskończoności gdy silnik nie wyrabia)
    if (i_term > ctx->out_max) {
        ctx->integral_sum = ctx->out_max / ctx->Ki; 
        i_term = ctx->out_max;
    } else if (i_term < ctx->out_min) {
        ctx->integral_sum = ctx->out_min / ctx->Ki;
        i_term = ctx->out_min;
    }

    // 4. Człon Różniczkujący (D)
    float d_term = ctx->Kd * (error - ctx->prev_error);
    ctx->prev_error = error;

    // 5. Suma sterowania
    float output = p_term + i_term + d_term;

    // 6. Ograniczenie wyjścia (Saturacja do zakresu np. 0-100%)
    if (output > ctx->out_max) output = ctx->out_max;
    if (output < ctx->out_min) output = ctx->out_min;

    return output;
}