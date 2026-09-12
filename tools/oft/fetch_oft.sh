#!/bin/sh
# Download the OpenFastTrace jar and verify it.
#
# The jar is not committed. OpenFastTrace is GPL-3.0, and shipping the binary
# carries source-availability duties this fork does not need. The tool runs as
# a separate process, so no license reaches the Blender code.
set -e
VERSION=4.9.0
EXPECTED=d4ed42503ae066f51d55c3aad7c6e4b16acb80365921951ef5a065a4dc3d94f3
DIR=$(dirname "$0")
JAR="$DIR/openfasttrace-$VERSION.jar"
URL="https://github.com/itsallcode/openfasttrace/releases/download/$VERSION/openfasttrace-$VERSION.jar"

if [ -f "$JAR" ]; then
  echo "already present: $JAR"
  exit 0
fi

echo "downloading OpenFastTrace $VERSION"
curl -sSL -o "$JAR" "$URL"

ACTUAL=$(sha256sum "$JAR" | cut -d" " -f1)
if [ "$ACTUAL" != "$EXPECTED" ]; then
  rm -f "$JAR"
  echo "checksum mismatch. Expected $EXPECTED, got $ACTUAL. File deleted." >&2
  exit 1
fi
echo "verified and wrote $JAR"
