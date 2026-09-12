#!/bin/sh
# Trace the knowledge base against the code this fork adds.
#
#   tools/oft/trace.sh            report failures
#   tools/oft/trace.sh all        report every item
#
# Exit code 1 means an item is uncovered, outdated, or orphaned.
set -e
DIR=$(dirname "$0")
ROOT="$DIR/../.."
JAR=$(ls "$DIR"/openfasttrace-*.jar 2>/dev/null | head -1)
if [ -z "$JAR" ]; then
  echo "OpenFastTrace is missing. Run: tools/oft/fetch_oft.sh" >&2
  exit 2
fi
VERBOSITY=${1:-failure_details}
exec java -jar "$JAR" trace -v "$VERBOSITY" \
  "$ROOT/docs_wiki/architecture" \
  "$ROOT/docs_wiki/decisions" \
  "$ROOT/source/blender/editors/space_addon" \
  "$ROOT/source/blender/makesdna/DNA_space_types.h" \
  "$ROOT/source/blender/blenkernel/intern/context.cc" \
  "$ROOT/scripts/startup/bl_ui/space_addon.py"
