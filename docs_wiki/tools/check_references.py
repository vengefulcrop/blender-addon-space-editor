"""Find every reference to code that no longer exists.

Deleting a feature leaves its documentation behind. This script finds the
leftovers. It scans the distributed wiki and the comments this fork adds,
then reports:

  1. A named symbol that is not in `source/` or `scripts/`.
  2. A `file.cc:123` citation whose file is missing, or whose line is past
     the end of the file.

    python docs_wiki/tools/check_references.py [--code-only|--docs-only]

Exit code 1 means a reference is dead. Run it after you delete or rename
anything.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
CODE_DIRS = ["source", "scripts"]
# This fork's own test scripts. The wiki cites the symbols they define, so
# index them. `docs_ui` holds untracked working notes and older copies, and
# stays in the list for a clone that still has them.
EXTRA_DIRS = ["tests/pyareas", "docs_ui"]
CODE_EXT = {".cc", ".hh", ".h", ".c", ".py"}

# Files this fork adds or edits. Only their comments are ours to answer for.
OUR_CODE = [
    "source/blender/editors/space_addon",
    "source/blender/makesdna/DNA_space_types.h",
    "source/blender/makesdna/DNA_screen_types.h",
    "source/blender/makesdna/DNA_userdef_types.h",
    "scripts/startup/bl_ui/space_addon.py",
]

SYMBOL = re.compile(r"#([A-Z][A-Za-z0-9_]*(?:::[A-Za-z0-9_]+)?|[a-z_][a-z0-9_]{4,})\b")
BACKTICKED = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*(?:::[A-Za-z0-9_]+)?)\(?\)?`")

# Only symbols this fork owns. A symbol from elsewhere (an MSVC error code, a
# spec example, a test fixture) is not this project's to keep alive.
OURS = re.compile(
    r"^(ADDON_|SPACE_ADDON|addon_|bAddon|USER_ADDON_|AddonTree"
    r"|CTX_wm_space_addon|BKE_paneltypes_|PanelDrawContextOverride"
    r"|ED_region_panels_layout_ex|ctx_wm_area_effective)"
)

# A line that says a symbol is gone is correct, not a defect. Never flag it.
# A line that says a symbol is gone is correct, not a defect.
GONE_PHRASES = (
    "deleted", "delete ", "removed", "remove ", "no longer",
    "does not exist", "dead code", "superseded", "used to",
    "formerly", "scheduled for deletion", "obsolete", "not exist",
)


def says_gone(text):
    low = text.lower()
    return any(w in low for w in GONE_PHRASES)
CITATION = re.compile(r"`?([A-Za-z_][A-Za-z0-9_]*\.(?:cc|hh|h|py)):(\d+)(?:-(\d+))?`?")

# C and C++ keywords, doxygen commands, and words that look like symbols.
IGNORE = {
    "include", "define", "ifdef", "ifndef", "endif", "else", "elif", "pragma",
    "param", "return", "brief", "note", "todo", "warning", "section", "file",
    "ingroup", "code", "endcode", "details", "since", "author", "struct",
    "class", "enum", "union", "typedef", "namespace", "template", "public",
    "private", "protected", "static", "const", "inline", "nullptr", "true",
    "false", "return", "sizeof", "while", "switch", "break", "continue",
}


def index_symbols():
    names = set()
    for d in CODE_DIRS + EXTRA_DIRS:
        if not (ROOT / d).is_dir():
            continue
        for p in (ROOT / d).rglob("*"):
            if p.suffix not in CODE_EXT or "__pycache__" in p.parts:
                continue
            try:
                text = p.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            names.update(re.findall(r"\b([A-Za-z_][A-Za-z0-9_]{2,})\b", text))
    return names


def find_file(name):
    for d in CODE_DIRS:
        hits = list((ROOT / d).rglob(name))
        if hits:
            return hits[0]
    return None


def scan(paths, names, label):
    problems = []
    for p in paths:
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = p.relative_to(ROOT).as_posix()
        all_lines = text.split(chr(10))
        for n, line in enumerate(all_lines, 1):
            # A wrapped sentence splits the phrase, so read a small window.
            window = " ".join(all_lines[max(0, n - 2):n + 1])
            for m in list(SYMBOL.finditer(line)) + list(BACKTICKED.finditer(line)):
                sym = m.group(1).split("::")[0]
                if sym in IGNORE or sym in names or not OURS.match(sym):
                    continue
                if says_gone(window):
                    continue
                problems.append((rel, n, f"DEAD SYMBOL `{sym}`"))
            for m in CITATION.finditer(line):
                fname, ln = m.group(1), int(m.group(2))
                target = find_file(fname)
                if target is None:
                    problems.append((rel, n, f"MISSING FILE `{fname}`"))
                    continue
                total = len(target.read_text(encoding="utf-8", errors="replace").split("\n"))
                if ln > total:
                    problems.append((rel, n, f"LINE PAST EOF `{fname}:{ln}` (file has {total})"))
    return problems


def main():
    names = index_symbols()
    doc_paths, code_paths = [], []

    tracked = subprocess.run(
        ["git", "ls-files", "docs_wiki"], capture_output=True, text=True, cwd=ROOT
    ).stdout.split()
    for f in tracked:
        p = ROOT / f
        # The rule files give example citations, not claims about code.
        if p.name in {"CLAUDE.md", "GEMINI.md"}:
            continue
        if p.suffix == ".md" and p.exists():
            head = p.read_text(encoding="utf-8", errors="replace")[:2000]
            if "verbatim: true" in head:
                continue
            doc_paths.append(p)

    for entry in OUR_CODE:
        p = ROOT / entry
        if p.is_dir():
            code_paths += [q for q in p.rglob("*") if q.suffix in CODE_EXT]
        elif p.exists():
            code_paths.append(p)

    problems = []
    if "--code-only" not in sys.argv:
        problems += scan(doc_paths, names, "wiki")
    if "--docs-only" not in sys.argv:
        problems += scan(code_paths, names, "code")

    if problems:
        print(f"[FAIL] {len(problems)} dead reference(s):")
        for rel, n, msg in problems:
            print(f"  {rel}:{n}  {msg}")
        return 1
    print("[ok] every referenced symbol, file, and line exists")
    return 0


if __name__ == "__main__":
    sys.exit(main())
