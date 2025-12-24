#include <stdio.h>
#include <stdint.h>
#include <math.h> // do fabs
#include "ringbuf.h"
#include "protocol.h"
#include "pid.h"

// --- KONFIGURACJA ---
#define TICK_MS 10
#define ALPHA 0.1f          // Inercja rośliny (modelu)
#define WATCHDOG_LIMIT 5    // Po ilu cyklach bez odświeżenia wejść w SAFE

// --- FIX QEMU ---
extern void initialise_monitor_handles(void);

// --- GLOBALNE ---
rb_t rb_rx;
rb_t rb_tx;
PID_Context pid;

// Definicja trybów pracy FSM
typedef enum {
    MODE_INIT,
    MODE_IDLE,
    MODE_RUN_OPEN,   // Sterowanie otwarte (bez PID)
    MODE_RUN_CLOSED, // Sterowanie zamknięte (PID)
    MODE_SAFE        // Awaria
} SystemMode;

// Stan systemu
struct {
    SystemMode state;
    float setpoint;     // Cel (Prędkość w CLOSED, Moc % w OPEN)
    float plant_y;      // Wyjście obiektu (Aktualna prędkość)
    float control_u;    // Sterowanie (0-100%)
    int wd_counter;     // Licznik watchdoga
} sys = {MODE_INIT, 0.0f, 0.0f, 0.0f, 0};

// --- SYMULACJA FIZYKI (ROŚLINA) ---
// Model: y[k+1] = y[k] + alpha * (-y[k] + u[k])
void plant_update(float u) {
    sys.plant_y = sys.plant_y + ALPHA * (-sys.plant_y + u);
}

// --- FUNKCJE POMOCNICZE ---
void log_tick(int tick) {
    // Format wymagany w sprawozdaniu: [tick] k=... mode=... set=... y=... u=...
    const char* mode_str;
    switch(sys.state) {
        case MODE_RUN_OPEN: mode_str = "OPEN"; break;
        case MODE_RUN_CLOSED: mode_str = "CLOSED"; break;
        case MODE_SAFE: mode_str = "SAFE"; break;
        default: mode_str = "IDLE";
    }

    // Wykrycie nasycenia (saturacji) dla raportu
    int u_sat = (sys.control_u >= 100.0f || sys.control_u <= 0.0f) ? 1 : 0;

    printf("[tick] k=%02d mode=%s set=%04.1f y=%05.1f u=%05.1f u_sat=%d\n", 
           tick, mode_str, sys.setpoint, sys.plant_y, sys.control_u, u_sat);
}

// Symulacja komendy z PC
void sim_cmd(uint8_t cmd, uint8_t val) {
    // W normalnym programie to idzie przez UART, tu ustawiamy bezpośrednio dla symulacji
    if (cmd == CMD_SET_SPEED) {
        sys.setpoint = (float)val;
        sys.wd_counter = 0; // Reset watchdoga ( przyszła ramka )
    }
    else if (cmd == CMD_MODE) {
        // 0=OPEN, 1=CLOSED
        if (val == 0) sys.state = MODE_RUN_OPEN;
        else sys.state = MODE_RUN_CLOSED;
        printf("[evt] MODE CHANGED -> %s\n", val ? "CLOSED" : "OPEN");
    }
    else if (cmd == CMD_STOP) {
        sys.state = MODE_SAFE;
        sys.setpoint = 0;
        printf("[evt] STOP CMD RECEIVED -> SAFE\n");
    }
}

// --- MAIN ---
int main(void) {
    initialise_monitor_handles();
    setbuf(stdout, NULL);

    // Init
    rb_init(&rb_rx);
    rb_init(&rb_tx);
    proto_init();
    pid_init(&pid, 2.0f, 0.1f, 0.5f, 0.0f, 100.0f); // Kp, Ki, Kd
    
    sys.state = MODE_IDLE;
    printf("=== SYSTEM START (Exercise 2 Simulation) ===\n");
    printf("Parametry: Alpha=%.2f, Kp=%.1f Ki=%.1f Kd=%.1f\n", ALPHA, pid.Kp, pid.Ki, pid.Kd);

    // --- SCENARIUSZ A: TEST FUNKCJONALNY (CLOSED LOOP) ---
    printf("\n--- TEST A: TRYB CLOSED (PID) ---\n");
    sim_cmd(CMD_MODE, 1); // Switch to CLOSED
    sim_cmd(CMD_SET_SPEED, 50); // Set Target 50 km/h

    for (int k = 0; k < 20; k++) {
        // 1. Logika Watchdoga
        sys.wd_counter++;
        if (sys.wd_counter > WATCHDOG_LIMIT) {
            sys.state = MODE_SAFE;
            printf("[wd] watchdog timeout -> SAFE\n");
        } else {
            // Symulujemy, że ramki przychodzą (reset WD)
            sys.wd_counter = 0; 
        }

        // 2. Obliczenie sterowania
        if (sys.state == MODE_RUN_CLOSED) {
            sys.control_u = pid_update(&pid, sys.setpoint, sys.plant_y);
        } else if (sys.state == MODE_RUN_OPEN) {
            sys.control_u = sys.setpoint; // W Open loop setpoint to % mocy
        } else { // SAFE / IDLE
            sys.control_u = 0.0f;
        }

        // 3. Fizyka
        plant_update(sys.control_u);

        // 4. Log
        log_tick(k);
    }

    // --- SCENARIUSZ B: TRYB OPEN (STEROWANIE MOCĄ) ---
    printf("\n--- TEST B: TRYB OPEN (Moc na sztywno) ---\n");
    sim_cmd(CMD_MODE, 0); // Switch to OPEN
    sim_cmd(CMD_SET_SPEED, 30); // Ustaw moc na 30% (nie prędkość!)
    
    // Reset pid dla czystości testu
    pid_reset(&pid);

    for (int k = 20; k < 30; k++) {
        // W OPEN setpoint traktujemy jako zadaną moc wyjściową
        sys.control_u = sys.setpoint; 
        
        plant_update(sys.control_u);
        log_tick(k);
    }

    // --- SCENARIUSZ C: AWARIA (WATCHDOG) ---
    printf("\n--- TEST C: AWARIA (Watchdog) ---\n");
    // Wracamy do Closed, żeby zobaczyć jak spadnie sterowanie
    sim_cmd(CMD_MODE, 1);
    sim_cmd(CMD_SET_SPEED, 80);
    
    // Symulujemy kilka kroków normalnych
    for (int k = 30; k < 35; k++) {
        sys.wd_counter = 0; // Komunikacja OK
        sys.control_u = pid_update(&pid, sys.setpoint, sys.plant_y);
        plant_update(sys.control_u);
        log_tick(k);
    }

    printf("!! SYMULACJA ZERWANIA KOMUNIKACJI !!\n");
    // Teraz NIE resetujemy licznika WD -> Powinien wejść w SAFE
    for (int k = 35; k < 45; k++) {
        sys.wd_counter++; // Licznik tyka...

        if (sys.wd_counter >= WATCHDOG_LIMIT && sys.state != MODE_SAFE) {
            sys.state = MODE_SAFE;
            printf("[wd] watchdog timeout -> SAFE (tick %d)\n", k);
        }

        if (sys.state == MODE_SAFE) {
            sys.control_u = 0.0f; // STOP AWARYJNY
        } else {
             sys.control_u = pid_update(&pid, sys.setpoint, sys.plant_y);
        }

        plant_update(sys.control_u);
        log_tick(k);
    }

    return 0;
}