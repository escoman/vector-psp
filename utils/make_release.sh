#!/usr/bin/env bash
# Pack the ready PSP game folder (release/VECTOR06C/) into a versioned zip
# for GitHub Releases: release/VECTOR06C-v<VERSION>.zip
#
# <VERSION> is taken from the Makefile (VERSION = X.Y.Z).
# Run from anywhere:  utils/make_release.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PKG_DIR="$ROOT/release/VECTOR06C"

VERSION="$(sed -n 's/^VERSION *= *//p' "$ROOT/Makefile" | head -1)"
if [ -z "$VERSION" ]; then
    echo "ERROR: cannot parse VERSION from $ROOT/Makefile" >&2
    exit 1
fi

if [ ! -f "$PKG_DIR/EBOOT.PBP" ]; then
    echo "ERROR: $PKG_DIR/EBOOT.PBP not found — run 'make' first." >&2
    exit 1
fi

ARCHIVE="VECTOR06C-v${VERSION}.zip"
rm -f "$ROOT/release/$ARCHIVE"
(cd "$ROOT/release" && zip -r "$ARCHIVE" VECTOR06C -x '*.DS_Store')

echo
echo "Created release/$ARCHIVE:"
unzip -l "$ROOT/release/$ARCHIVE"
