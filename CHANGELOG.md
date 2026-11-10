# CHANGELOG — ASTRO-ROOF SAFE / Specula Carpathia

Najnowsze na górze. Każda zmiana firmware podnosi pole `build` w `src/main.cpp`.
Pole `fw` (SAFE-RECOVERY-20261105) zostaje bez zmian dla zgodności ze skryptami N.I.N.A.

## Niewydane
(puste)

## ALPACA-PWM-20261109-R2 — 10.11.2026 — Claude
Status: kod gotowy, **niewgrany**. Testy offline 40/40 (g++). Kompilacja PlatformIO **niezweryfikowana**, test na sprzęcie nie był robiony.
- `src/hardware_io.h`: `heaterPwmWriteChannel` ponawia zapis do 3 razy (przerwa 20 ms), gdy moduł jest online. Sweep OFF (moduł offline) robi 1 próbę. Odzyskany zapis daje wpis INFO HEATERS (najwyżej raz na 5 min), błąd WARN podaje liczbę prób.
- `src/main.cpp`: Alpaca ObservingConditions zwraca błąd 0x500 zamiast `Value:null` dla temperature, humidity, dewpoint, skytemperature, cloudcover, pressure. `skybrightness` bez zmian.
- `src/main.cpp`: nieznany powód resetu (np. po wgraniu przez USB) pokazuje kod liczbowy i ma poziom WARN zamiast CRITICAL.
- Testy: nowy `tests/check_pwm_retry.py`. Funkcje `alpacaSend`, `heaterControlTick`, `heaterManualSet`, `writeAll` zarejestrowane jako zamierzone zmiany w `check_policy_invariants.py` i `check_rc41_invariants.py` (zmiany z wersji ALPACA-PWM-20261109, wcześniej testy były czerwone).

## ALPACA-PWM-20261109 — 9.11.2026 — ChatGPT (Codex)
Status: wg logu z 10.11.2026 działa na ESP32 (potwierdź polem `build` w `/api/state`). W logu ~2 h przy otwartym dachu bez błędów PWM.
- PWM: wyłączanie OFF po jednym kanale na przebieg pętli, odświeżanie kanałów na zmianę (jeden zapis na przebieg), automat wraca po potwierdzeniu wszystkich czterech zapisów OFF.
- Modbus: rozpoznawanie ramki wyjątku (5 bajtów), szczegółowy log błędu PWM (kanał, rodzaj błędu, bajty, kod wyjątku).
- HTTP: nagłówek i ciało odpowiedzi jednym wysłaniem; `EthernetClient::stop()` czeka 20 ms zamiast domyślnej 1 s; budżet 250 ms na wysłanie, porzucenie wiszącego połączenia.
- Alpaca: `windspeed` i `windgust` zwracają błąd 0x500 zamiast `Value:null`, gdy wiatr jest niewiarygodny.
- Uwaga: w tym commicie `src/main.cpp` ma zmienione końcówki linii (CRLF→LF). Do przeglądu: `git diff --ignore-cr-at-eol fw/SAFE-RECOVERY-20261105 fw/ALPACA-PWM-20261109 -- src/main.cpp`.
- Uwaga: paczka ZIP nie zawierała `README.md`, `docs/` ani `RELEASE_MANIFEST.json`; w repozytorium zostały z wersji bazowej.
- Znany błąd tej wersji: 2 testy pilnujące hashy funkcji były czerwone (naprawione w R2).

## SAFE-RECOVERY-20261105 — 5.11.2026 — baza
Import projektu `DACH_PROSTE_NAZWY_NIEWGRANY`. Szczegóły: `README.md`, `docs/SAFE_RECOVERY_20261105.md`.
`RELEASE_MANIFEST.json` i raporty w `docs/` dotyczą tej wersji i nie były aktualizowane.
