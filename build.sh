#!/bin/bash
set -e

BIN="HyprTile"

[ -f "$BIN" ] && rm "$BIN"

echo "🔧 Building HyprTile..."
START=$(date +%s)

qmake6 launcher.pro
make -j10

strip --strip-all "$BIN"

echo "📦 Running UPX..."
upx --ultra-brute "$BIN"

END=$(date +%s)
SIZE=$(stat -c%s "$BIN")

echo "✅ Build done in $((END-START)) seconds"
echo "📏 File size: $SIZE bytes (~$((SIZE/1024)) kB)"

./"$BIN"
