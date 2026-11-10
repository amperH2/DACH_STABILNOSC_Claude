# Wynik — NETFIX1, 28.10.2026

## Co ustalono

Log wykazał reset przez Task Watchdog o 14:57:56. Zapis etapu 20 wskazuje okolice obsługi HTTP, ale nie jest backtrace i nie dowodzi dokładnego miejsca zawieszenia. Wpisy `NOTIFY ... result=200 accepted=1` oznaczają poprawne przyjęcie powiadomienia/heartbeat; same nie oznaczają resetów.

## Poprawki

- Ograniczono oczekiwanie biblioteki Ethernet na rejestry, miejsce w buforze, potwierdzenia TCP/UDP i wykonanie komendy układu. Dodano licznik przekroczeń czasu.
- Współdzielony dostęp do układu Ethernet chroni krótka blokada. Podczas oczekiwania na zdalne potwierdzenie blokada jest zwalniana.
- Ograniczono obsługę uszkodzonych lub niepełnych pakietów UDP/DNS i przetwarzanie opcji DHCP.
- HTTP przez Wi-Fi zapisuje bez blokującej pętli ponawiania. Dodano dokładniejsze etapy diagnostyczne obsługi HTTP.
- Skorygowano jednostki timeoutu publicznego API ESP_SSLClient: 2000/3000 sekund zastąpiono 2/3 sekundami. Limit handshake 4 sekundy pozostaje.
- Poprawiona biblioteka Ethernet jest częścią projektu, więc zmiany nie znikną po odtworzeniu `.pio`.

Watchdog pozostaje włączony. Nie zmieniono mapowania sprzętu, GPIO, Modbus, progów pogodowych, polityki parku, konfiguracji antykolizyjnej ani sterowania ruchem dachu.

## Sprawdzenie

Pełna kompilacja `poe_main`, `eth_8ro_spare`, `rs485_service`: SUCCESS. Wszystkie 33 zestawy testów offline: PASS. Testy obejmują m.in. zawieszone oczekiwanie Ethernet, uszkodzone UDP, przepełnienie millis i dotychczasowe zasady bezpieczeństwa. Pozostaje wcześniejsze ostrzeżenie ADC_ATTEN_DB_11 o przestarzałej nazwie stałej.

Nie wykonano testu na urządzeniu, rzeczywistych odpowiedzi NINA ani długotrwałej obserwacji stabilności. Poprawki usuwają znalezione mechanizmy blokowania; bez diagnostyki stosu nie można obiecać, że wyjaśniają każdą możliwą przyczynę resetu.

## Jak użyć folderu

1. Zachowaj obecny projekt DACH i działający firmware jako kopię.
2. Otwórz ten folder jako nowy projekt PlatformIO. Skopiuj do niego tylko własny `secrets.ini` z dotychczasowego DACH. Nie zmieniaj `hardware.ini` i nie kopiuj starego `.pio`.
3. Uruchom Build profilu `poe_main`. Pozostałe dwa profile służą innym wariantom sprzętu.
4. Przed ewentualnym wgraniem sprawdź zamknięty dach, stabilny PARK i zatrzymaną sekwencję NINA. Wgrywaj wyłącznie do sterownika dachu na COM3. Nie kasuj całej pamięci/NVS.
5. Po wgraniu sprawdź nazwę NETFIX1 w logu oraz stabilność działania. Próby ruchu wymagają osobnej kontroli fizycznej; nie wykonano ich w ramach tej poprawki.

Obecny projekt PlatformIO i firmware ESP32 pozostały niezmienione. Z powodu ograniczeń zapisu przygotowano osobny folder zgodnie z prośbą użytkownika.
