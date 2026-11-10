# Reguły pracy dla agentów AI (Codex, Claude Code i inne)

Ten plik jest identyczny z `CLAUDE.md`. Test `tests/check_agent_rules.py` pilnuje, żeby tak zostało.
Projekt: ASTRO-ROOF SAFE, firmware ESP32-S3 sterujący dachem obserwatorium „Specula Carpathia”.
To układ bezpieczeństwa: błąd w firmware może uszkodzić sprzęt. Stabilność jest ważniejsza niż nowe funkcje.

## 0. Zanim cokolwiek zmienisz
1. Przeczytaj `docs/STAN.md` (co jest wgrane, co jest otwarte) i ostatnie wpisy `CHANGELOG.md`.
2. Uruchom `git status` i `git log --oneline -n 10`. Jeśli drzewo robocze jest brudne albo nie wiesz, czyja to zmiana, zatrzymaj się i zapytaj użytkownika.
3. Pisać możesz tylko wtedy, gdy użytkownik wprost powiedział, że w tej sesji jesteś osobą piszącą. W przeciwnym razie pracujesz tylko do odczytu: analizujesz, recenzujesz, proponujesz diff w odpowiedzi, ale nie zapisujesz plików. Piszący jest zawsze jeden naraz.

## 1. Gałęzie
- `main` = ostatnia zaakceptowana wersja z przechodzącymi testami. Nigdy nie commituj ani nie scalaj bezpośrednio do `main`; scala użytkownik.
- Swoją pracę rób na gałęzi `agent/<claude|codex>/<krótki-temat>` utworzonej z aktualnego `main`.
- Tag `flashed/<data>-<build>` oznacza wersję faktycznie wgraną na ESP32. Nie zakładaj, że `main` jest wgrany.

## 2. Zakres zmian
- Rób najmniejszą zmianę, która rozwiązuje problem. Bez refaktoryzacji, formatowania, zmiany nazw plików ani „przy okazji”.
- NIE zmieniaj końcówek linii (CRLF/LF) w żadnym pliku i nie przepuszczaj plików przez formatter. Część testów liczy hashe bajtów plików, a masowa zmiana końcówek zamienia diff na tysiące linii. Po edycji sprawdź `git diff --stat`; jeśli plik, który ruszyłeś minimalnie, pokazuje setki zmienionych linii, cofnij i napraw końcówki.
- Funkcje przypięte hashami (`tests/rc51_policy_baseline.json`, `tests/rc4_safety_baseline.json`) to logika bezpieczeństwa. Nie zmieniaj ich bez wyraźnej zgody użytkownika. Jeśli zmiana jest zamierzona i zatwierdzona, dopisz funkcję do listy wyjątków w obu testach (`intentional` w `check_rc41_invariants.py`, lista pominięć w `check_policy_invariants.py`), podaj powód w komentarzu i we wpisie `CHANGELOG.md`. Nigdy nie „naprawiaj” czerwonego testu przez ślepe podmienianie hashy.
- Logiki dachu, wiatru, deszczu, PARK, zasilania montażu i przekaźników nie zmieniaj bez zgody. Przy wątpliwości zapytaj.
- Komentarze w kodzie pisz po polsku, bez znaków diakrytycznych, w stylu istniejących plików.

## 3. Bramka jakości (przed każdym commitem)
1. `python tests/run_all.py --compiler <kompilator>` musi dać 100% (obecnie 41 zestawów). Kompilator: `g++` albo `xtensa-esp32s3-elf-g++` z PlatformIO.
2. Jeśli masz PlatformIO: `pio run -e poe_main` musi się skompilować. Jeśli nie możesz uruchomić kompilacji, napisz to wprost w `CHANGELOG.md` („niezweryfikowane kompilacją”). Nie twierdź, że przechodzi.
3. Podnieś znacznik `build` w `src/main.cpp` (pole JSON `"build"`) i dopisz wpis do `CHANGELOG.md`: co, dlaczego, pliki, wynik testów, czego nie sprawdzono.
4. Commit na swojej gałęzi, komunikat: `<BUILD>: <krótki opis>`.

## 4. Czego nigdy nie robisz
- Nie dotykasz `secrets.ini` i nie commitujesz haseł, tokenów ani kluczy.
- Nie wgrywasz firmware na ESP32 i nie wysyłasz poleceń do urządzenia (dach, przekaźniki, PWM), chyba że użytkownik wprost o to poprosi w tej sesji. Wgrywa się po sesji obserwacyjnej, przy zamkniętym dachu i zaparkowanym montażu.
- Nie usuwasz historii git (`reset --hard`, `push --force`, `rebase` cudzych gałęzi).
- Nie obiecujesz, że coś zadziała na sprzęcie. Rozróżniaj: „przechodzą testy offline”, „kompiluje się”, „sprawdzone na urządzeniu”.

## 5. Przekazanie pracy drugiemu agentowi
Na koniec sesji zostaw: commit na swojej gałęzi, wpis w `CHANGELOG.md` (sekcja „Niewydane”), listę otwartych pytań. Kolejny agent zaczyna od punktu 0. Jeśli dwie gałęzie zmieniły te same funkcje, nie scalaj obu na ślepo; zgłoś to użytkownikowi i poproś jednego agenta o przeniesienie swojej zmiany na aktualny `main`.

## 6. Komunikacja
Odpowiadaj użytkownikowi po polsku, krótko i konkretnie. Podawaj fakty i oddzielaj je od przypuszczeń. Gdy coś jest niejasne albo kłóci się z tymi regułami, zapytaj.
