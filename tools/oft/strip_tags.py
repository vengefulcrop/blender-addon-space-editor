"""Remove the OpenFastTrace tags from the source tree.

The tags are not idiomatic Blender. Strip them before you send a patch
upstream. A tag lives on its own line, so removing the line leaves the
surrounding comment untouched.

    python tools/oft/strip_tags.py --check    report, change nothing
    python tools/oft/strip_tags.py            remove the tag lines

Exit code 1 under --check means a tag is present.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
TAG_LINE = re.compile(r"^\s*(/\*|//|#)\s*\[(impl|utest|itest)->[^\]]+\]\s*(\*/)?\s*$")


def source_files():
    out = subprocess.run(
        ["git", "ls-files", "source", "scripts"],
        capture_output=True, text=True, cwd=ROOT,
    ).stdout.split()
    return [ROOT / f for f in out if Path(f).suffix in {".cc", ".hh", ".h", ".c", ".py"}]


def main():
    check = "--check" in sys.argv
    hits = 0
    touched = []
    for p in source_files():
        raw = p.read_bytes()
        nl = "\r\n" if b"\r\n" in raw else "\n"
        lines = raw.decode("utf-8").split(nl)
        keep = [l for l in lines if not TAG_LINE.match(l)]
        removed = len(lines) - len(keep)
        if not removed:
            continue
        hits += removed
        touched.append((p.relative_to(ROOT).as_posix(), removed))
        if not check:
            p.write_bytes(nl.join(keep).encode("utf-8"))

    if not hits:
        print("[ok] no OpenFastTrace tags in the source tree")
        return 0
    verb = "found" if check else "removed"
    print(f"[{'FAIL' if check else 'ok'}] {verb} {hits} tag line(s):")
    for rel, n in touched:
        print(f"  {rel}  ({n})")
    if check:
        print("\nRun without --check to remove them before an upstream patch.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
