#!/bin/sh
# D1 HDMI DOOM preflight: READ ONLY, zero video mode changes.
set -u
echo '=== LAB-HDMI-DOOM-01 PREFLIGHT ==='
uname -a
echo '=== ACTIVE VT AND FRAMEBUFFER ==='
cat /sys/class/tty/tty0/active 2>&1
cat /proc/fb 2>&1
for field in virtual_size bits_per_pixel stride; do
  file="/sys/class/graphics/fb0/$field"
  if [ -r "$file" ]; then printf '%s=' "$field"; cat "$file"; fi
done
ls -l /dev/fb0 /dev/dri/card0 2>&1
echo '=== USER AND PERMISSIONS ==='
id
if [ -r /dev/fb0 ]; then echo 'FB_READ=YES'; else echo 'FB_READ=NO'; fi
if [ -w /dev/fb0 ]; then echo 'FB_WRITE=YES'; else echo 'FB_WRITE=NO'; fi
echo '=== BUILD TOOLS ==='
for tool in gcc make git curl unzip; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf '%s=%s\n' "$tool" "$(command -v "$tool")"
  else
    printf '%s=MISSING\n' "$tool"
  fi
done
echo '=== FREEDOOM DATA ==='
for file in /usr/share/games/doom/freedoom1.wad /usr/local/share/games/doom/freedoom1.wad "$HOME/freedoom1.wad"; do
  if [ -f "$file" ]; then ls -lh "$file"; fi
done
echo '=== MEMORY ==='
free -h
echo '=== DONE: NO SYSTEM MODIFICATIONS ==='
