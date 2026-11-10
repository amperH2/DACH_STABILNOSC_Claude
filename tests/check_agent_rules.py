"""Pilnuje, zeby AGENTS.md i CLAUDE.md byly identyczne i zawieraly kluczowe zasady."""
from pathlib import Path
root = Path(__file__).resolve().parents[1]
a = (root / "AGENTS.md").read_bytes()
c = (root / "CLAUDE.md").read_bytes()
assert a == c, "AGENTS.md i CLAUDE.md musza byc identyczne (skopiuj jeden na drugi)"
text = a.decode("utf-8")
for needle in ("secrets.ini", "tests/run_all.py", "agent/<claude|codex>", "CRLF/LF", "docs/STAN.md", "CHANGELOG.md"):
    assert needle in text, "Brak w regulach: " + needle
for name in ("docs/STAN.md", "CHANGELOG.md", "docs/PRACA_Z_AGENTAMI.md", "workflow/import_zip.py"):
    assert (root / name).is_file(), "Brak pliku: " + name
print("PASS reguly agentow: AGENTS.md == CLAUDE.md, wymagane pliki obecne")
