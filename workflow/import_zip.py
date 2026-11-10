#!/usr/bin/env python3
"""Importuje paczke ZIP od agenta z czatu na NOWA galaz git.

Nic nie scala do main i niczego nie commituje - tylko przygotowuje galaz ze zmianami do przegladu.
Pomija: secrets.ini, .git, .pio, __pycache__. Pliki, ktorych nie ma w ZIP-ie, zostaja bez zmian.

Uzycie: python workflow/import_zip.py paczka.zip --branch agent/chatgpt/temat [--base main]
"""
import argparse
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {".git", ".pio", "__pycache__", "__MACOSX"}
SKIP_FILES = {"secrets.ini"}


def git(*args, check=True):
    r = subprocess.run(["git", *args], cwd=ROOT, text=True, capture_output=True)
    if check and r.returncode != 0:
        sys.exit("git " + " ".join(args) + " -> " + (r.stderr or r.stdout).strip())
    return r.stdout.strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("zip")
    ap.add_argument("--branch", required=True, help="nazwa nowej galezi, musi zaczynac sie od agent/")
    ap.add_argument("--base", default="main", help="galaz bazowa (domyslnie main)")
    args = ap.parse_args()

    if not args.branch.startswith("agent/"):
        sys.exit("Galaz musi miec nazwe agent/<kto>/<temat>, np. agent/chatgpt/pwm-retry")
    if git("rev-parse", "--is-inside-work-tree", check=False) != "true":
        sys.exit("To nie jest repozytorium git.")
    if git("status", "--porcelain"):
        sys.exit("Drzewo robocze nie jest czyste. Zacommituj lub schowaj zmiany i sprobuj ponownie.")
    if git("branch", "--list", args.branch):
        sys.exit("Galaz " + args.branch + " juz istnieje. Wybierz inna nazwe.")
    if not git("branch", "--list", args.base):
        sys.exit("Nie ma galezi bazowej " + args.base)
    zpath = Path(args.zip)
    if not zpath.is_file():
        sys.exit("Nie ma pliku: " + args.zip)

    with tempfile.TemporaryDirectory() as tmp:
        tmp_root = Path(tmp).resolve()
        with zipfile.ZipFile(zpath) as z:
            for name in z.namelist():
                try:
                    (tmp_root / name).resolve().relative_to(tmp_root)
                except ValueError:
                    sys.exit("Niebezpieczna sciezka w ZIP-ie: " + name)
            z.extractall(tmp_root)
        src = tmp_root
        entries = [e for e in src.iterdir() if e.name not in SKIP_DIRS]
        if len(entries) == 1 and entries[0].is_dir():
            src = entries[0]
        if not (src / "platformio.ini").is_file():
            sys.exit("W ZIP-ie nie ma platformio.ini - to nie wyglada na paczke projektu.")

        git("switch", "-c", args.branch, args.base)
        copied = skipped = 0
        for f in sorted(src.rglob("*")):
            if not f.is_file():
                continue
            rel = f.relative_to(src)
            if SKIP_DIRS.intersection(rel.parts) or rel.name in SKIP_FILES:
                skipped += 1
                continue
            dst = ROOT / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(f, dst)
            copied += 1

    print("Galaz " + args.branch + " utworzona z " + args.base + ". Skopiowano plikow: " + str(copied) + ", pominieto: " + str(skipped) + ".")
    status = git("status", "--short")
    if not status:
        print("Brak roznic wzgledem " + args.base + " (paczka identyczna z repozytorium).")
    else:
        print(status)
        print()
        print(git("diff", "--stat", "--ignore-cr-at-eol"))
        print("\nNastepne kroki: uruchom testy (python tests/run_all.py --compiler g++), obejrzyj git diff, potem git add -A i git commit.")


if __name__ == "__main__":
    main()
