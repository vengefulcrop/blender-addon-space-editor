"""Compare a document against its previous git revision.

It extracts the facts that a style rewrite must never change, then reports
what was lost or altered. Run it after every edit.

    python docs_wiki/tools/check_facts.py <file> [git-rev]

The default revision is HEAD. Exit code 1 means a fact changed.
"""

import re
import subprocess
import sys
from pathlib import Path

# A path, a symbol, a function, a struct field, or a file:line citation.
IDENT = re.compile(
    r"`[^`\n]+`"
    r"|\b[A-Za-z_][A-Za-z0-9_]*\.(?:cc|hh|h|c|py|toml|md|txt|yaml)\b(?::\d+(?:-\d+)?)?"
    r"|\b[A-Z][A-Za-z0-9_]*::[A-Za-z0-9_]+\b"
    r"|\b[A-Z][A-Z0-9_]{3,}\b"
)
NUMBER = re.compile(r"(?<![\w.])\d+(?:[.,]\d+)?(?![\w])")
# Polarity words. A style rewrite must not flip one.
POLARITY = re.compile(
    r"\b(not|never|no|none|cannot|without|neither|nor|fails?|failed|"
    r"open|fixed|rejected|accepted|deferred|done|wrong|correct|safe|unsafe|"
    r"dead|live|removed|added|kept|deleted)\b",
    re.I,
)
CODE_FENCE = re.compile(r"```.*?```", re.S)


def facts(text):
    return {
        "identifiers": sorted(set(IDENT.findall(text))),
        "numbers": sorted(set(NUMBER.findall(text))),
        "links": sorted(set(re.findall(r"\]\(([^)]+)\)", text))),
        "headings": [h.strip() for h in re.findall(r"^#{1,6}\s+(.*)$", text, re.M)],
        "code_blocks": CODE_FENCE.findall(text),
    }


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    path = Path(sys.argv[1])
    rev = sys.argv[2] if len(sys.argv) > 2 else "HEAD"

    old = subprocess.run(
        ["git", "show", f"{rev}:{path.as_posix()}"],
        capture_output=True, text=True, encoding="utf-8",
    )
    if old.returncode != 0:
        print(f"{path}: not in {rev}, nothing to compare")
        return 0
    before, after = old.stdout, path.read_text(encoding="utf-8")
    fb, fa = facts(before), facts(after)

    problems = []
    for key in ("identifiers", "numbers", "links"):
        lost = sorted(set(fb[key]) - set(fa[key]))
        if lost:
            problems.append(f"LOST {key} ({len(lost)}): {lost[:20]}")

    if fb["code_blocks"] != fa["code_blocks"]:
        problems.append("CODE BLOCKS CHANGED. A code block must stay verbatim.")

    lost_head = sorted(set(fb["headings"]) - set(fa["headings"]))
    if lost_head:
        problems.append(
            f"HEADINGS CHANGED ({len(lost_head)}): {lost_head}. "
            "Another file may link to one of these by anchor."
        )

    pb = sorted(w.lower() for w in POLARITY.findall(CODE_FENCE.sub("", before)))
    pa = sorted(w.lower() for w in POLARITY.findall(CODE_FENCE.sub("", after)))
    if pb != pa:
        from collections import Counter
        cb, ca = Counter(pb), Counter(pa)
        delta = {w: ca[w] - cb[w] for w in set(cb) | set(ca) if ca[w] != cb[w]}
        problems.append(
            f"POLARITY WORD COUNTS CHANGED: {delta}. "
            "Read every changed sentence. A flipped negation reverses a fact."
        )

    if problems:
        print(f"[FAIL] {path}")
        for p in problems:
            print("  -", p)
        return 1
    print(f"[ok]   {path}: every identifier, number, link, heading, "
          f"code block, and polarity word survived")
    return 0


if __name__ == "__main__":
    sys.exit(main())
