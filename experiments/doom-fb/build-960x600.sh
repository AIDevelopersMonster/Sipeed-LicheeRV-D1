#!/bin/sh
# Build a separate 960x600 DoomGeneric executable for the existing 1024x600 framebuffer.
# Does NOT change kernel, HDMI mode, DTB, drivers, or the working 640x400 build.
set -eu

BASE="${HOME}/d1-doom-fb/upstream/doomgeneric"
DEST="${HOME}/d1-doom-fb/doomgeneric-960x600"
MARKER=".d1-managed-960x600"
FLAGS="CFLAGS += -DDOOMGENERIC_RESX=960 -DDOOMGENERIC_RESY=600"

if [ ! -x "$BASE/doomgeneric" ] || [ ! -f "$BASE/doomgeneric_fb.c" ]; then
    echo "D1_DOOM_960_PRECHECK_FAIL: working 640x400 build not found: $BASE" >&2
    exit 1
fi
if ! grep -q 'doomgeneric_fb.o' "$BASE/Makefile"; then
    echo 'D1_DOOM_960_PRECHECK_FAIL: original build is not framebuffer variant' >&2
    exit 1
fi
if [ ! -e "$DEST" ]; then
    cp -a "$BASE" "$DEST"
    : > "$DEST/$MARKER"
elif [ ! -f "$DEST/$MARKER" ]; then
    echo "D1_DOOM_960_PRECHECK_FAIL: existing directory is not managed: $DEST" >&2
    exit 1
fi

cd "$DEST"
python3 - <<'PY'
from pathlib import Path
p = Path("Makefile")
s = p.read_text()
flags = "CFLAGS += -DDOOMGENERIC_RESX=960 -DDOOMGENERIC_RESY=600"
if "doomgeneric_fb.o" not in s or "-lX11" in s:
    raise SystemExit("D1_DOOM_960_FAIL: expected framebuffer-only Makefile")
if "-DDOOMGENERIC_RESX=" in s and flags not in s:
    raise SystemExit("D1_DOOM_960_FAIL: conflicting custom display size")
if flags not in s:
    p.write_text(s.rstrip() + "\n\n# D1 larger in-memory rendering rectangle; HDMI mode unchanged.\n" + flags + "\n")
print("D1_DOOM_960_BUILD_FLAGS_PASS")
PY

# These operations affect ONLY the separate 960x600 copy.
make CC=gcc clean
make CC=gcc -j1

test -x ./doomgeneric
file ./doomgeneric
sha256sum ./doomgeneric
printf '\nD1_DOOM_960_BUILD_PASS: %s\n' "$(pwd)/doomgeneric"
printf 'D1_DOOM_640_PRESERVED: %s\n' "$BASE/doomgeneric"
printf 'This is a build check only: no video output or hardware FPS claim yet.\n'
