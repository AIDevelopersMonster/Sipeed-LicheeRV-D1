#!/bin/sh
# Read-only inventory of HDMI state.
echo '=== KERNEL ==='
uname -a
echo '=== FRAMEBUFFER ==='
cat /proc/fb
cat /sys/class/tty/tty0/active
cat /sys/class/graphics/fb0/virtual_size
cat /sys/class/graphics/fb0/bits_per_pixel
echo '=== DEVICES ==='
ls -l /dev/fb0 /dev/dri/card0
echo '=== NETWORK ==='
ip -br addr show wlan0
