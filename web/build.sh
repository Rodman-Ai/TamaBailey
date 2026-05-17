#!/usr/bin/env bash
# Build the TamaBailey WebAssembly bundle.
# Requires an activated Emscripten SDK (`source emsdk_env.sh`).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WEB="$ROOT/web"
DIST="$WEB/dist"
CORE="$ROOT/lib/tama_core"

mkdir -p "$DIST"

EMCC=${EMCC:-emcc}

"$EMCC" \
  -std=gnu++17 -O2 -fno-exceptions -fno-rtti \
  -I "$CORE/include" -I "$WEB" \
  "$CORE/src/achievements.cpp" \
  "$CORE/src/audio_clips.cpp" \
  "$CORE/src/clock.cpp" \
  "$CORE/src/font_6x8.cpp" \
  "$CORE/src/friends.cpp" \
  "$CORE/src/game.cpp" \
  "$CORE/src/save.cpp" \
  "$CORE/src/sprites.cpp" \
  "$CORE/src/ui.cpp" \
  "$WEB/web_renderer.cpp" \
  "$WEB/main_web.cpp" \
  -s WASM=1 \
  -s MODULARIZE=0 \
  -s EXPORTED_FUNCTIONS='["_bailey_init","_bailey_frame","_bailey_input","_bailey_apply_sync_code","_bailey_generate_sync_code","_bailey_set_spectator","_bailey_set_weather","_bailey_get_stat","_malloc","_free"]' \
  -s EXPORTED_RUNTIME_METHODS='["HEAPU8","HEAP16","ccall","cwrap"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s INITIAL_MEMORY=8mb \
  -s ENVIRONMENT=web \
  -o "$DIST/tama.js"

cp "$WEB/index.html" "$DIST/index.html"
cp "$WEB/style.css"  "$DIST/style.css"
cp "$WEB/shell.js"   "$DIST/shell.js"

echo
echo "Built into $DIST"
echo "Serve locally with:  python3 -m http.server -d \"$DIST\" 8000"
