# System sterowania: pętla PI/PID, tryby, testy akceptacyjne

## Cel

Zbudować zamknięty układ sterowania (PI lub PID) nad urządzeniem zewnętrznym (symulowanym), z trybami OPEN/CLOSED, ograniczeniami i bezpiecznymi stanami. Przeprowadzić testy akceptacyjne (metryki jakości + awarie).

## Zakres
- Pętla sterowania (tick np. 10 ms): u = PI/PID(set, meas); ograniczenia u_min/u_max, anti-windup.
- Tryby:
- OPEN: u ustawiane bezpośrednio przez SET SPEED/OUT.
- CLOSED: u z regulatora na podstawie SETPOINT.
- Bezpieczeństwo: watchdog pętli (wykrycie zatrzymania), SAFE po STOP lub błędach.
- FSM systemu: INIT → IDLE → RUN(OPEN/CLOSED) → FAULT/SAFE.

Kroki (bez odpowiedzi)
1. Kontrakt rejestrów/parametrów: SETPOINT, OUT(u), MODE, LIMITS, STATUS, ERR.
2. Regulator: PI lub PID (może być q15 lub float), saturacja + anti-windup; tick() wywoływany okresowo.
3. Symulator obiektu: 1-rzędowy (y[k+1] = y[k] + α(-y[k] + u[k])) lub inny prosty model; zaszum go lekko (opcjonalnie).
4. Tryby i przełączanie:
    - OPEN: OUT ustawiane komendą (z rampą antyszarpnięciową).
    - CLOSED: setpoint → regulator → OUT.
    - Bumpless transfer (opcjonalnie): łagodne przejście OPEN↔CLOSED.
5. Bezpieczeństwo:
    - Watchdog: jeśli tick() nie wykona się N× z rzędu → SAFE.
    - Limity: jeśli |y| przekroczy dopuszczalny zakres → FAULT/SAFE.
6. Testy akceptacyjne:
    - Funkcjonalne: MODE OPEN → SET OUT → MODE CLOSED → SETPOINT → STOP.
    - Jakościowe (tylko CLOSED): czas narastania, przeregulowanie, błąd ustalony (zdefiniuj tolerancję).
    - Awarie: zatrzymaj tick() (symulacja), podaj OUT na skraj wartości, przerwij ramki → system ma wejść w SAFE.
7. Telemetria: ticks, wd_resets, overshoot_max, u_sat_count, mode_changes.

## Artefakty do oddania
- control.md: opis PI/PID, parametry, granice, opis FSM, scenariusze testów.
- Kod: control.*, plant.*, integracja z protokołem z Ćw. 1.
- REPORT.md: tabela metryk (≥2 zestawy parametrów, ≥2 wartości α), logi z testów awaryjnych.