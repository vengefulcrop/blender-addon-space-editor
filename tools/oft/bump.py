"""Raise the version of a traceability concept.

Two steps, on purpose:

    python tools/oft/bump.py arch~single-delegate-per-area
        Raise the wiki concept from ~N to ~N+1. The code tags stay at ~N.
        The trace then reports every claiming site as outdated, and names it.

    python tools/oft/bump.py arch~single-delegate-per-area --accept
        Raise the code tags to the version the wiki now carries.

Review every site the trace names before you run --accept. One command that
raised both sides at once would defeat the check, because the point of the
version is to force a look at every claiming site.

    python tools/oft/bump.py --list
        Show every concept, its wiki version, and the code tags claiming it.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
DOC_DIRS = ["docs_wiki/architecture", "docs_wiki/decisions"]
CODE_EXT = {".cc", ".hh", ".h", ".c", ".py"}
SRC_DIRS = ["source", "scripts"]

DOC_ID = re.compile(r"`([a-z]+~[a-z0-9-]+)~(\d+)`")
TAG = re.compile(r"\[(impl|utest|itest)->([a-z]+~[a-z0-9-]+)~(\d+)\]")


def doc_files():
    out = []
    for d in DOC_DIRS:
        p = ROOT / d
        if p.is_dir():
            out += sorted(p.rglob("*.md"))
    return out


def code_files():
    tracked = subprocess.run(
        ["git", "ls-files"] + SRC_DIRS, capture_output=True, text=True, cwd=ROOT
    ).stdout.split()
    return [ROOT / f for f in tracked if Path(f).suffix in CODE_EXT]


def read(p):
    raw = p.read_bytes()
    return raw.decode("utf-8"), ("\r\n" if b"\r\n" in raw else "\n")


def survey():
    concepts = {}
    for p in doc_files():
        text, _ = read(p)
        for name, ver in DOC_ID.findall(text):
            concepts.setdefault(name, {"doc": None, "tags": []})
            concepts[name]["doc"] = (p, int(ver))
    for p in code_files():
        text, _ = read(p)
        for _, name, ver in TAG.findall(text):
            concepts.setdefault(name, {"doc": None, "tags": []})
            concepts[name]["tags"].append((p, int(ver)))
    return concepts


def show(concepts):
    for name in sorted(concepts):
        c = concepts[name]
        dv = c["doc"][1] if c["doc"] else "-"
        print(f"{name}~{dv}")
        if not c["doc"]:
            print("    wiki: NONE (orphaned tag)")
        for p, v in c["tags"]:
            flag = "" if c["doc"] and v == c["doc"][1] else "   <- OUTDATED"
            print(f"    {p.relative_to(ROOT).as_posix()}  ~{v}{flag}")
        if not c["tags"]:
            print("    code: NONE (uncovered)")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    accept = "--accept" in sys.argv
    concepts = survey()

    if "--list" in sys.argv or not args:
        show(concepts)
        return 0

    name = args[0].rsplit("~", 1)[0] if re.search(r"~\d+$", args[0]) else args[0]
    if name not in concepts:
        print(f"unknown concept: {name}\n", file=sys.stderr)
        show(concepts)
        return 1
    c = concepts[name]
    if not c["doc"]:
        print(f"{name} has no wiki section. Nothing to raise.", file=sys.stderr)
        return 1

    doc_path, doc_ver = c["doc"]

    if not accept:
        new_ver = doc_ver + 1
        text, nl = read(doc_path)
        old, new = f"`{name}~{doc_ver}`", f"`{name}~{new_ver}`"
        if old not in text:
            print(f"could not find {old} in {doc_path}", file=sys.stderr)
            return 1
        doc_path.write_bytes(text.replace(old, new, 1).encode("utf-8"))
        print(f"raised {name} to ~{new_ver} in "
              f"{doc_path.relative_to(ROOT).as_posix()}")
        if c["tags"]:
            print(f"\n{len(c['tags'])} code site(s) still claim ~{doc_ver}. "
                  "Review each one:")
            for p, v in c["tags"]:
                print(f"  {p.relative_to(ROOT).as_posix()}  ~{v}")
            print("\nThen run:")
            print(f"  python tools/oft/bump.py {name} --accept")
        else:
            print("no code site claims this concept")
        return 0

    changed = 0
    for p, v in c["tags"]:
        if v == doc_ver:
            continue
        text, nl = read(p)
        old, new = f"->{name}~{v}]", f"->{name}~{doc_ver}]"
        if old not in text:
            continue
        p.write_bytes(text.replace(old, new).encode("utf-8"))
        print(f"raised {p.relative_to(ROOT).as_posix()}  ~{v} -> ~{doc_ver}")
        changed += 1
    if not changed:
        print(f"every code tag already claims {name}~{doc_ver}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
