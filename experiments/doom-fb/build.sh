#!/bin/sh
# Build DoomGeneric locally for riscv64; no kernel/HDMI modifications.
set -eu
UPSTREAM=e257a09527062bfb986b6e5e0780839b6cbd5b71
LAB=https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/doom-fb
DEST="${HOME}/d1-doom-fb"
mkdir -p "$DEST"
cd "$DEST"
if [ ! -d upstream/.git ]; then
  git clone https://github.com/ozkl/doomgeneric.git upstream
fi
cd upstream
git fetch --depth 1 origin "$UPSTREAM"
git checkout --detach "$UPSTREAM"
cd doomgeneric
curl -fL "$LAB/doomgeneric_fb.c" -o doomgeneric_fb.c
# Keep all changes in downloaded source tree, not in system files.
python3 - <<'PY'
from pathlib import Path
p=Path("Makefile")
s=p.read_text()
assert "doomgeneric_xlib.o" in s
assert "-lX11" in s
s=s.replace("doomgeneric_xlib.o", "doomgeneric_fb.o")
s=s.replace("-lX11", "")
p.write_text(s)
PY
make CC=gcc -j1
file doomgeneric
printf '\nD1_DOOM_BUILD_COMPLETE: %s\n' "$(pwd)/doomgeneric"
printf 'RUN ONLY AFTER video-group and WAD checks; keep UART open.\n'
