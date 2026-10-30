#!/bin/sh
# Build a SEPARATE DoomGeneric USB keyboard+mouse version.
# Never modifies the original working Doom binaries or HDMI configuration.
set -eu
BASE="$HOME/d1-doom-fb/upstream/doomgeneric"
DEST="$HOME/d1-doom-fb/doomgeneric-usb"
URL="https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/doom-fb"
if [ ! -x "$BASE/doomgeneric" ] || ! grep -q 'doomgeneric_fb.o' "$BASE/Makefile"; then
  echo "Working framebuffer Doom source missing in $BASE" >&2; exit 1
fi
if [ ! -e "$DEST" ]; then
  cp -a "$BASE" "$DEST"
  : > "$DEST/.d1-usb-build"
elif [ ! -f "$DEST/.d1-usb-build" ]; then
  echo "Refusing to modify unmanaged directory $DEST" >&2; exit 1
fi
cd "$DEST"
curl -fL "$URL/d1_usb_hid.h" -o d1_usb_hid.h
curl -fL "$URL/d1_usb_hid.c" -o d1_usb_hid.c
curl -fL "$URL/doomgeneric_fb.c" -o doomgeneric_fb.c

python3 - <<'PY'
from pathlib import Path
p=Path("Makefile")
s=p.read_text()
if "doomgeneric_fb.o" not in s: raise SystemExit("Not framebuffer Doom")
if "d1_usb_hid.o" not in s:
    s=s.replace("doomgeneric_fb.o", "doomgeneric_fb.o d1_usb_hid.o")
p.write_text(s)

p=Path("i_input.c")
s=p.read_text()
if '#include "d1_usb_hid.h"' not in s:
    s=s.replace('#include "doomgeneric.h"', '#include "doomgeneric.h"\n#include "d1_usb_hid.h"')
if "D1_USB_Poll();" not in s:
    needle="    unsigned char key;"
    if needle not in s: raise SystemExit("i_input.c key declarations changed")
    s=s.replace(needle,needle+"\n    D1_USB_Poll();",1)
if 'D1_USB_GetKey(&pressed, &key)' not in s:
    needle='while (DG_GetKey(&pressed, &key))'
    if needle not in s: raise SystemExit("i_input.c key loop changed")
    s=s.replace(needle,'while (D1_USB_GetKey(&pressed, &key) || DG_GetKey(&pressed, &key))',1)
if 'D1_USB_GetMouse(&buttons, &dx, &dy)' not in s:
    needle='\n\n\n                /*\n            case SDL_MOUSEMOTION:'
    if needle not in s: raise SystemExit("i_input.c mouse insertion point changed")
    addon='''\n    {
        int buttons = 0, dx = 0, dy = 0;
        if (D1_USB_GetMouse(&buttons, &dx, &dy)) {
            event.type = ev_mouse;
            event.data1 = buttons;
            event.data2 = dx;
            event.data3 = dy;
            event.data4 = 0;
            D_PostEvent(&event);
        }
    }
'''
    s=s.replace(needle,"\n"+addon+needle,1)
if 'D1_USB_Init();' not in s:
    needle='void I_InitInput(void)\n{\n}'
    if needle not in s: raise SystemExit("i_input.c init function changed")
    s=s.replace(needle,'void I_InitInput(void)\n{\n    D1_USB_Init();\n}',1)
p.write_text(s)

p=Path("doomgeneric_fb.c")
s=p.read_text()
if '#include "d1_usb_hid.h"' not in s:
    s=s.replace('#include "doomkeys.h"', '#include "doomkeys.h"\n#include "d1_usb_hid.h"')
if 'D1_USB_Close();' not in s:
    needle="static void cleanup(void)\n{"
    if needle not in s: raise SystemExit("Framebuffer cleanup anchor missing")
    s=s.replace(needle,needle+"\n    D1_USB_Close();",1)
p.write_text(s)
print("D1_USB_CODE_PATCHED")
PY

# Recompile only the separate copy, without a destructive global clean.
make CC=gcc clean
make CC=gcc -j1
test -x ./doomgeneric
file ./doomgeneric
printf '\nD1_USB_DOOM_BUILD_FINISHED: %s\n' "$(pwd)/doomgeneric"
printf 'Original Doom binary remains: %s/doomgeneric\n' "$BASE"
