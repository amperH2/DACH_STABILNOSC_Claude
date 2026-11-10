# Stan projektu (aktualizuje człowiek albo agent na wyraźne polecenie)

Ostatnia aktualizacja: 10.11.2026

| Co | Wersja | Uwagi |
|---|---|---|
| Na ESP32 (wg logu z 10.11.2026) | ALPACA-PWM-20261109 | Potwierdź polem `build` w `/api/state`. Nie ma jeszcze tagu `flashed/...`. |
| `main` | ALPACA-PWM-20261109-R2 | Niewgrane. Przed wgraniem: `pio run -e poe_main` i test z zamkniętym dachem. |
| Bezpieczny powrót | tag `fw/ALPACA-PWM-20261109` | Wersja, która teraz działa w obserwatorium. |

## Otwarte tematy (nie rozwiązane w kodzie)
1. Jedno gniazdo nasłuchu W5500 i 2 sesje HTTP na interfejs: równoległe połączenia (NINA + panel WWW) mogą być odrzucane.
2. Anemometr: ramki ADC są regularnie odrzucane (ok. 0,35 V, duży rozrzut min/max). Sprawdzić kabel, masę, zasilanie. Osobno od NINA/PWM.
3. `skybrightness` w Alpace nadal zwraca `Value:null`, gdy brak pomiaru (BH1750 wyłączony).
4. Pojedynczy błąd PWM po 3 próbach nadal wyłącza grzałki i loguje CRITICAL (celowo, bezpieczny kierunek).
5. Kompilacja PlatformIO dla R2 niezweryfikowana.

## Czego nie ma w repozytorium
`secrets.ini` (hasła). Wzór: `secrets.ini.example`. Trzymaj kopię poza repo.
