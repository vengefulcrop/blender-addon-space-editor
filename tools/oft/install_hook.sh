#!/bin/sh
# Install the pre-commit trace hook into this clone.
set -e
ROOT=$(git rev-parse --show-toplevel)
HOOK="$ROOT/.git/hooks/pre-commit"
if [ -e "$HOOK" ]; then
  echo "a pre-commit hook already exists at $HOOK" >&2
  echo "merge tools/oft/pre-commit into it by hand" >&2
  exit 1
fi
cp "$ROOT/tools/oft/pre-commit" "$HOOK"
chmod +x "$HOOK"
echo "installed $HOOK"
