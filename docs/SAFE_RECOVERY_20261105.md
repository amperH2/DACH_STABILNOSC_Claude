# SAFE-RECOVERY-20261105 — Specula Carpathia

Naprawiony problem: chwilowa utrata PARK podczas ruchu zatrzymywała dach i wymagała restartu, nawet gdy sygnały wróciły. Nowy osobny typ błędu PARK_LOST pozwala na świadome potwierdzenie w panelu po inspekcji, bez restartu. Moment wykrycia i STOP nie zostały opóźnione filtrem.

Panel wymaga autoryzowanego MANUAL, hasła i potwierdzenia inspekcji nieruchomego napędu. Sterownik wymaga ponownego stabilnego PARK RA/DEC przez dotychczasowe 5 s, poprawnej geometrii, braku aktywnych blokad, potwierdzonego STOP i jego zwolnienia, braku impulsów oraz świeżego odczytu OFF trzech kanałów napędu. Po odczycie Modbus ponownie sprawdza PARK, blokady, rodzaj błędu, stan dachu i brak nowego STOP w trakcie potwierdzania.

Potwierdzenie samo nie wysyła ruchu. Nie wznawia otwierania i nie przełącza MANUAL na AUTO. Po odblokowaniu istniejąca ochrona pogodowa może zażądać CLOSE — panel ostrzega o tym przed potwierdzeniem. Polecenie CLOSE nadal przechodzi wszystkie istniejące zabezpieczenia. Przed potwierdzeniem trzeba rzeczywiście sprawdzić dach i napęd; echo STOP nie jest pomiarem fizycznego bezruchu.

Bez zmian: ręczne potwierdzenie COMMUNICATION; brak automatycznego kasowania błędów. STUCK, TIMEOUT, sprzeczne krańcówki i STOP_UNCONFIRMED nie są odblokowywane nową ścieżką. Brak PARK albo nieudany odczyt też odrzucają potwierdzenie. Nie zmieniono wiatru, chmur, GPIO, DI/DO, Modbus, czujników ani geometrii. Nie dodano niekalibrowanej ochrony DENSE_CLOUD. Pozostaje zwykła ochrona zachmurzenia.

Wersja: jedna stała FW_VERSION z etykietą SAFE-RECOVERY-20261105 w stanie fw, logu startowym, stopce logu i informacji sterowników Alpaca. Nazwy Specula Carpathia i identyfikatory Alpaca zachowane.

Weryfikacja: 38/38 zestawów offline PASS; wszystkie trzy profile kompilacji SUCCESS. Nowe testy wykonują produkcyjne funkcje potwierdzenia: trzy pozycje dachu, 25 odmów, odbicia PARK, przepełnienie millis, brak skonfigurowanych wejść oraz utrata PARK/nowy STOP/nowy błąd w trakcie odczytu Modbus. Testy WWW weryfikują bramki uprawnień i pokazywanie przycisku. Układ przy 320/390/1440 px sprawdzony w przeglądarce z zablokowaną siecią. Nie wykonywano prób ruchu, przełączania przekaźników ani potwierdzania błędu na rzeczywistym napędzie. To nadal wymaga kontrolowanej inspekcji i próby stanowiskowej.

Zmienione źródła firmware: src/main.cpp oraz src/web_ui.h. Pozostałe zmiany: testy, dokumentacja, manifest. Raport porównania krytycznych funkcji i konfiguracji: SAFE_RECOVERY_SCOPE.txt. Różnice do przeglądu: SAFE_RECOVERY_REVIEW.diff.

Wynik wgrania i odczytu po restarcie, jeśli wykonano: SAFE_RECOVERY_UPLOAD.json. Używaj całego nowego folderu projektu. Poprzednia wersja z 4 listopada pozostała kopią bezpieczeństwa. Po wgraniu odśwież stronę Ctrl+F5.
