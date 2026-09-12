"""Report ornate wording that the style linter does not catch.

The style linter scores sentence shape. It says nothing about register, so
a document can score well and still read as overwritten. This checks the
register rules in CLAUDE.md rule 3.

    python docs_wiki/tools/check_register.py [file ...]

With no argument it checks every tracked document, skipping a file marked
`verbatim: true` and the raw notes. Exit code 1 means work remains.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

# Allowed at most this many times in one document.
RATIONED = {"silent": 1, "silently": 1}

# Never useful. The replacement is the fact itself.
BANNED = {
    "quiet": "name the fact, or use silent once",
    "quietly": "name the fact, or use silent once",
    "hazard": "caveat, risk, or pitfall",
    "hazards": "caveats, risks, or pitfalls",
    "peril": "risk",
    "subtle": "state what happens",
    "insidious": "state what happens",
    "catastrophic": "state the loss",
    "disastrous": "state the loss",
    "seamless": "state what it does",
    "robust": "state what it does",
    "powerful": "state what it does",
    "elegant": "state what it does",
}

# A heading names its subject. These words in a heading are decoration.
HEADING_WORDS = {
    "silent", "silently", "quiet", "subtle", "worth", "important",
    "critical", "crucial", "surprising", "tricky", "nasty",
    "why", "how", "matters", "yourself",
}


def body(text):
    text = re.sub(r"^---\n.*?\n---\n", "", text, count=1, flags=re.S)
    return re.sub(r"```.*?```", "", text, flags=re.S)


def check(path):
    raw = path.read_text(encoding="utf-8", errors="replace")
    if raw.startswith("---") and "verbatim: true" in raw.split("---")[1]:
        return []
    text = body(raw)
    rel = path.relative_to(ROOT).as_posix()
    out = []

    words = re.findall(r"[A-Za-z]+", text.lower())
    counts = {}
    for w in words:
        counts[w] = counts.get(w, 0) + 1

    for w, limit in RATIONED.items():
        if counts.get(w, 0) > limit:
            out.append(f"{rel}: `{w}` used {counts[w]} times, limit {limit}")
    for w, fix in BANNED.items():
        if counts.get(w, 0):
            out.append(f"{rel}: `{w}` x{counts[w]} -> {fix}")

    for n, line in enumerate(text.split("\n"), 1):
        if not line.startswith("#"):
            continue
        for w in re.findall(r"[A-Za-z]+", line.lower()):
            if w in HEADING_WORDS:
                out.append(f"{rel}:{n}: heading carries `{w}` -> "
                           f"name the subject and stop")
                break
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if args:
        paths = [Path(a).resolve() for a in args]
    else:
        tracked = subprocess.run(
            ["git", "ls-files", "docs_wiki"],
            capture_output=True, text=True, cwd=ROOT,
        ).stdout.split()
        # The rule files define this vocabulary, so they name every word.
        # log.md is a ledger of changes, and records what was reworded.
        skip = {"CLAUDE.md", "GEMINI.md", "log.md"}
        paths = [ROOT / f for f in tracked
                 if f.endswith(".md") and "/raw/" not in f
                 and Path(f).name not in skip]

    problems = []
    for p in paths:
        if p.exists():
            problems += check(p)

    if problems:
        print(f"[FAIL] {len(problems)} register problem(s):")
        for line in problems:
            print("  " + line)
        return 1
    print("[ok] no ornate wording found")
    return 0


if __name__ == "__main__":
    sys.exit(main())
