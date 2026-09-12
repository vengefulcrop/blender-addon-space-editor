import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Reserved files per OKF spec that don't need concept frontmatter
RESERVED = {"index.md", "log.md", "README.md", "CLAUDE.md", "GEMINI.md"}

# Folders excluded from validation
EXCLUDED_DIRS = {".git", "notes", "raw"}

def parse_frontmatter(content: str):
    if not content.startswith("---"):
        return None
    parts = content.split("---", 2)
    if len(parts) < 3:
        return None
    frontmatter_text = parts[1]
    # Simple key-value extraction to avoid hard dependency on PyYAML
    data = {}
    for line in frontmatter_text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if ":" in line:
            k, v = line.split(":", 1)
            data[k.strip()] = v.strip().strip("\"'")
    return data

def main():
    errors = []
    checked = 0

    for path in ROOT.rglob("*.md"):
        # Skip excluded dirs
        if any(part in EXCLUDED_DIRS for part in path.parts):
            continue
        if path.name in RESERVED:
            continue

        rel_path = path.relative_to(ROOT)
        checked += 1
        try:
            content = path.read_text(encoding="utf-8-sig")
        except Exception as e:
            errors.append(f"[{rel_path}] Could not read file: {e}")
            continue

        fm = parse_frontmatter(content)
        if fm is None:
            errors.append(f"[{rel_path}] Missing YAML frontmatter (must start with '---')")
            continue

        if "type" not in fm or not fm["type"]:
            errors.append(f"[{rel_path}] Missing required 'type' field in frontmatter")

    print(f"OKF Validator: checked {checked} concept documents.")
    if errors:
        print(f"\nFound {len(errors)} conformance errors:")
        for err in errors:
            print(f"  - {err}")
        return 1
    else:
        print("All documents conform to OKF frontmatter requirements!")
        return 0

if __name__ == "__main__":
    sys.exit(main())
