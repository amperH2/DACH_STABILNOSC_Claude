# Jak pracować z Codexem i Claude na jednym projekcie

Cel: jeden stabilny, uporządkowany kod, bez dwóch agentów nadpisujących sobie zmiany.

## Zasady w skrócie
1. **Jeden piszący naraz.** Zanim zaczniesz sesję, powiedz agentowi: „Ty piszesz” albo „Tylko recenzujesz, nic nie zapisuj”. Drugi agent w tym czasie nie dotyka kodu.
2. **Każda praca na osobnej gałęzi** `agent/claude/<temat>` lub `agent/codex/<temat>`, zawsze z aktualnego `main`.
3. **`main` scalasz ty**, po obejrzeniu diffu i po 100% testów.
4. **Zasady są w `AGENTS.md` i `CLAUDE.md`** (identyczne). Codex i Claude Code czytają je same, ale możesz też wkleić agentowi: „Przeczytaj AGENTS.md i stosuj się do niego”.

## Jednorazowa konfiguracja
1. Zainstaluj git. Rozpakuj repozytorium (katalog z `.git`) w miejscu projektu.
2. `git log --oneline --decorate` pokaże historię: baza, wersja ChatGPT, R2, reguły.
3. Skopiuj swój `secrets.ini` obok `platformio.ini` (git go ignoruje).

## Praca z agentem działającym na twoim komputerze (Codex / Claude Code)
1. `git switch main` i `git status` (ma być czysto).
2. Agent: `git switch -c agent/claude/temat`, zmiany, testy, commit, wpis w `CHANGELOG.md`.
3. Ty: `git diff main..agent/claude/temat` (jeśli plik ma zmienione końcówki linii: dodaj `--ignore-cr-at-eol`).
4. Zgoda: `git switch main`, `git merge --no-ff agent/claude/temat`.

## Praca przez czat (agent oddaje ZIP)
1. `git switch main`, `git status` (czysto).
2. `python workflow/import_zip.py ~/Pobrane/paczka.zip --branch agent/chatgpt/temat`
   Skrypt tworzy gałąź, kopiuje pliki z ZIP-a (pomija `secrets.ini`, `.pio`, `.git`), niczego nie commituje. Pliki, których nie ma w ZIP-ie, zostają bez zmian.
3. Obejrzyj `git diff --stat`, uruchom testy, dopiero potem `git add -A && git commit`.

## Testy (bramka przed scaleniem)
`python tests/run_all.py --compiler g++`
albo z kompilatorem z PlatformIO (typowo `~/.platformio/packages/toolchain-xtensa-esp32s3/bin/xtensa-esp32s3-elf-g++`). Wynik ma być „41/41 offline suites passed”. Dodatkowo `pio run -e poe_main`.

## Po wgraniu na ESP32
Sprawdź pole `build` w `/api/state`, potem oznacz commit:
`git tag -a flashed/2026-11-12-ALPACA-PWM-20261109-R2 -m "wgrane na ESP32"`
i zaktualizuj `docs/STAN.md`.

## Powrót do wersji wgranej na ESP32
`git switch --detach flashed/<data>-<build>` (albo `fw/ALPACA-PWM-20261109`), skompiluj i wgraj. Wróć: `git switch main`.

## Gdy dwie gałęzie zmieniły to samo
Nie scalaj obu. Scal jedną, potem poproś **jednego** agenta, żeby przeniósł drugą zmianę na nowy `main` i przeszedł bramkę testów.

## Końcówki linii
W projekcie są pliki CRLF i LF. `.gitattributes` wyłącza konwersję. Nie używaj „Format document” ani „zamień końcówki” na całych plikach.
