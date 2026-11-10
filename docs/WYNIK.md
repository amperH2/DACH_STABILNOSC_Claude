# NETFIX2 — 28.10.2026

Naprawia potwierdzony błąd NETFIX1: główna strona była ucinana po jednej sekundzie. Pomiar z działającego ESP32: Content-Length 137930 bajtów, dostarczono 128512 bajtów. Brak końca JavaScript uniemożliwiał start panelu. Jednocześnie /api/state dostarczył komplet 5823 bajtów w 0,093 s, dach CLOSED, bez błędu ruchu. Nie był to dowód zawieszenia całego sterownika.

Zmiana: wysyłanie w porcjach 1024 zamiast 256 bajtów; limit bez postępu 1 s oraz całkowity limit 10 s zamiast 1 s dla całej strony. Podczas wysyłania nadal wykonywana jest szybka obsługa zabezpieczeń i pilnych żądań HTTP. Pozostałe poprawki NETFIX1 zachowano. Watchdog pozostaje włączony, konfiguracja sprzętu i logika ruchu są bez zmian.

Weryfikacja: 3/3 profile skompilowane, 34/34 zestawy testów offline PASS. Nowy test używa rzeczywistej funkcji writeAll i sprawdza pełną stronę 138 kB przesyłaną dłużej niż sekundę, częściowe zapisy, zatrzymanego odbiorcę i przepełnienie millis. Są to testy symulacyjne, nie test już wgranego NETFIX2.

NETFIX2 nie został wgrany przez agenta. Nie wykonywano ruchu dachu ani przełączeń przekaźników. Dotychczasowy projekt PlatformIO pozostaje bez zmian.

## Użycie

Otwórz folder DACH_NETFIX2 jako projekt PlatformIO. Skopiuj własny secrets.ini z działającego projektu; nie kopiuj starego .pio. Zachowaj cały lib/Ethernet. Build: profil poe_main. Przed ewentualnym Upload na COM3 sprawdź zamknięty dach, stabilny PARK i zatrzymaną NINA. Nie kasuj całej pamięci/NVS. Po wgraniu odśwież stronę Ctrl+F5 i sprawdź nazwę NETFIX2 w logu oraz aktualizację danych. Nie wyłączaj watchdoga.

Samo odświeżanie NETFIX1 nie naprawia ucinania strony. Starszy opis poprawek sieciowych zachowano w WYNIK_NETFIX1_ARCHIWUM.md.
