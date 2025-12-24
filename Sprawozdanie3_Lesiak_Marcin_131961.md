# Sprawozdanie z zajęć — **Systemy wbudowane w C: analiza, obliczenia, I/O, interfejs i sterowanie**


**Imię i nazwisko:**  Marcin Lesiak 131961
**Data zajęć:**   14.12.2025


## Ćwiczenie 1 — Interfejs sterujący urządzeniem (protokół + driver + telemetria)

> [!abstract] Cel ćwiczenia
> Zaprojektować i uruchomić nieblokujący interfejs sterujący z własnym protokołem ramek i obsługą błędów; zaimplementować driver (FSM odbioru) oraz telemetrię (statystyki RX/TX).

### Specyfikacja i Implementacja

> [!info] Szczegóły techniczne
> **Protokół:**
> * **Format ramki:** `STX (0xAA) | LEN | CMD | PAYLOAD | CRC`
> * **Kodowanie:** Binarne (Little Endian).
> * **Algorytm CRC:** XOR (suma modulo 2 wszystkich bajtów: `LEN ^ CMD ^ PAYLOAD`).
> 
> **Implementacja Drivera:**
> * **Warstwa transportowa:** Wykorzystano `ringbuf` z polityką `drop-new`.
> * **Maszyna stanów (FSM):** Zaimplementowano stany: `WAIT_STX` → `WAIT_LEN` → `WAIT_CMD` → `WAIT_DATA` → `WAIT_CRC`.
> * **Symulator:** Program testowy w `main.c` wstrzykuje ramki do bufora RX, symulując transmisję UART.

### Wyciąg z logów (własne)

> [!example] Scenariusze testowe (Logi z konsoli QEMU)
> Poniższe logi potwierdzają poprawną dekapsulację ramek, obsługę błędów oraz działanie telemetrii.
> 
> ```text
> === START SYSTEMU ===
> 
> --- TEST 1: SET SPEED (Ok) ---
> [TEST] PC wysyla: CMD=0x10 LEN=1
> [APP] Przyszla ramka: CMD=0x10 LEN=1
> [APP] Sukces: Predkosc ustawiona na 80
> [TEST] Odpowiedz urzadzenia (TX): AA 00 AA AA
> 
> --- TEST 2: Nieznana Komenda ---
> [TEST] PC wysyla: CMD=0x99 LEN=0
> [APP] Przyszla ramka: CMD=0x99 LEN=0
> [APP] Blad: Nieznana komenda
> [TEST] Odpowiedz urzadzenia (TX): AA 01 FF 01 FF
> 
> --- TEST 3: Zle CRC ---
> [SYSTEM] Ramka odrzucona prawidlowo (blad sumy kontrolnej)
> 
> --- TEST 4: GET STAT ---
> [TEST] PC wysyla: CMD=0x40 LEN=0
> [APP] Przyszla ramka: CMD=0x40 LEN=0
> [APP] Wyslano telemetrie (Dropped=0, CRC_Err=1)
> [TEST] Odpowiedz urzadzenia (TX): AA 14 40 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00 00 00 55
> ```

### Wnioski

> [!success] Podsumowanie
> 1. **Integralność danych:** Maszyna stanów (FSM) poprawnie ignoruje szum i przetwarza tylko ramki zgodne z protokołem (zaczynające się od `STX` i posiadające poprawne `CRC`).
> 2. **Odporność na błędy:** W Teście nr 3 celowo wprowadzono błędną sumę kontrolną. Driver poprawnie odrzucił tę ramkę, co zapobiegło wykonaniu błędnej instrukcji.
> 3. **Telemetria:** System poprawnie zliczył błąd CRC z Testu 3 (widoczne w logach Testu 4: `CRC_Err=1`). Pozwala to na zdalną diagnostykę jakości łącza.
> 4. **Dwukierunkowość:** Urządzenie poprawnie generuje odpowiedzi `ACK` (potwierdzenie) oraz `NACK` (błąd), co spełnia założenia niezawodnego interfejsu sterującego.

Marker autogradingu (opcjonalny): OK [IFACE]


## Ćwiczenie 2 — System sterowania (PI/PID, tryby OPEN/CLOSED, SAFE)

> [!abstract] Cel
> Zbudować zamknięty układ sterowania (PID) z obsługą trybów pracy (OPEN - sterowanie otwarte, CLOSED - pętla zamknięta) oraz mechanizmem bezpieczeństwa (Watchdog). Przeprowadzić testy funkcjonalne, jakościowe i awaryjne.

### Konfiguracja i Metryki

> [!info] Założenia techniczne
> * **Model rośliny:** $y[k+1] = y[k] + 0.1 \cdot (-y[k] + u[k])$ (inercja $\alpha=0.1$)
> * **Regulator PID:** $K_p=2.0, K_i=0.1, K_d=0.5$
> * **Saturacja:** $u \in [0.0, 100.0]$ (Anti-windup aktywny)
> * **Watchdog:** Próg zadziałania = 5 cykli pętli (symulacja 50ms przy tick=10ms)
> * **FSM:** `IDLE` → `RUN_OPEN` / `RUN_CLOSED` → `SAFE` (przy błędzie WD)

**Tabela wyników (Testy jakościowe):**

| Zestaw | Tryb | Kp | Ki | Kd | Czas narastania | Przeregulowanie | Błąd ustalony | Uwagi |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **#1** | **CLOSED** | 2.0 | 0.1 | 0.5 | > 20 ticków | 0% (Brak) | < 10% (w trakcie) | Układ stabilny, silnie tłumiony (brak oscylacji) |
| **#2** | **OPEN** | - | - | - | n/d | n/d | Zależny od obciążenia | Sterowanie mocą "na sztywno" (30%) |
| **#3** | **SAFE** | - | - | - | < 1 tick | - | - | Natychmiastowe zerowanie wyjścia (OUT=0) |

### Wyciąg z logów (Testy akceptacyjne - Własne)

> [!example] Scenariusz A i B: Tryb CLOSED i OPEN
> ```text
> --- TEST A: TRYB CLOSED (PID) ---
> [evt] MODE CHANGED -> CLOSED
> [tick] k=00 mode=CLOSED set=50.0 y=010.0 u=100.0 u_sat=1
> [tick] k=05 mode=CLOSED set=50.0 y=033.2 u=056.2 u_sat=0
> [tick] k=10 mode=CLOSED set=50.0 y=039.9 u=047.4 u_sat=0
> [tick] k=15 mode=CLOSED set=50.0 y=042.5 u=046.0 u_sat=0
> [tick] k=19 mode=CLOSED set=50.0 y=043.7 u=046.2 u_sat=0  <-- Stabilne dążenie do celu
> 
> --- TEST B: TRYB OPEN (Moc na sztywno) ---
> [evt] MODE CHANGED -> OPEN
> [tick] k=20 mode=OPEN set=30.0 y=042.3 u=030.0 u_sat=0   <-- Sztywna moc 30%
> [tick] k=25 mode=OPEN set=30.0 y=037.3 u=030.0 u_sat=0
> [tick] k=29 mode=OPEN set=30.0 y=034.8 u=030.0 u_sat=0   <-- Prędkość spada (fizyka)
> ```

> [!danger] Scenariusz C: Awarie (Watchdog)
> Symulacja zerwania komunikacji. Watchdog zlicza cykle bez nowej ramki danych.
> ```text
> --- TEST C: AWARIA (Watchdog) ---
> [tick] k=30 mode=CLOSED set=80.0 y=041.3 u=100.0 u_sat=1
> ...
> !! SYMULACJA ZERWANIA KOMUNIKACJI !!
> [tick] k=35 mode=CLOSED set=80.0 y=055.5 u=072.0 u_sat=0
> [tick] k=38 mode=CLOSED set=80.0 y=059.5 u=069.9 u_sat=0
> [wd] watchdog timeout -> SAFE (tick 39)                   <-- Wykrycie awarii!
> [tick] k=39 mode=SAFE set=80.0 y=053.6 u=000.0 u_sat=1    <-- Hard STOP (u=0)
> [tick] k=44 mode=SAFE set=80.0 y=031.6 u=000.0 u_sat=1    <-- Wyhamowywanie
> ```

### Wnioski i Podsumowanie

> [!success] Wnioski końcowe
> 1. **Jakość regulacji (CLOSED):** Dla przyjętych nastaw PID układ zachowuje się stabilnie. Nie występuje przeregulowanie (overshoot), co jest bezpieczne dla układów mechanicznych, choć czas dojścia do wartości zadanej jest wydłużony (układ przetłumiony).
> 2. **Tryb OPEN:** Pozwala na bezpośrednie sterowanie elementem wykonawczym (mocą), co jest przydatne przy testach sprzętowych lub awarii czujników. W testach widać, że stała moc nie gwarantuje stałej prędkości (zależy od dynamiki obiektu).
> 3. **Bezpieczeństwo (Watchdog):** Mechanizm poprawnie wykrył brak odświeżania komend (symulowane zerwanie łącza) i w cyklu 39 wymusił przejście do stanu `SAFE` z wyzerowaniem sygnału sterującego. Jest to kluczowy element bezpieczeństwa w systemach embedded.
> 
> **Co wdrożyłbym w wersji produkcyjnej:**
> 1. Sprzętowy Watchdog (IWDG) zamiast programowego licznika, aby zresetować MCU w razie zawieszenia CPU.
> 2. Płynne przełączanie trybów (bumpless transfer), aby inicjować człon całkujący (Ki) wartością bieżącą przy przejściu OPEN -> CLOSED.
> 3. Logowanie zdarzeń FAULT do pamięci nieulotnej (Flash/EEPROM) w celu późniejszej diagnostyki.