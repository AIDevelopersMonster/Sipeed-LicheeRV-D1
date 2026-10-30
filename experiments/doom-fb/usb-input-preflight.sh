#!/bin/sh
# LAB-HDMI-DOOM USB HID preflight, read-only.
# Does not modify HDMI, USB configuration, input permissions, or Linux services.
set -u

echo '=== D1 USB HID PREFLIGHT (READ-ONLY) ==='
uname -r

echo '=== USB HOST / HUB TOPOLOGY ==='
if command -v lsusb >/dev/null 2>&1; then
    lsusb
    lsusb -t
else
    echo 'lsusb is not installed; no installation is required for this check.'
fi

echo '=== USB DEVICE LABELS FROM SYSFS ==='
found=0
for node in /sys/bus/usb/devices/*/product; do
    [ -r "$node" ] || continue
    found=1
    printf '%s: ' "$node"
    cat "$node"
done
if [ "$found" -eq 0 ]; then
    echo 'No readable USB product labels found in sysfs.'
fi

echo '=== LINUX INPUT HANDLERS ==='
if [ -r /proc/bus/input/devices ]; then
    cat /proc/bus/input/devices
else
    echo '/proc/bus/input/devices unavailable'
fi

echo '=== INPUT DEVICE NODES / ACCESS ==='
for node in /dev/input/event*; do
    [ -e "$node" ] || continue
    ls -l "$node"
    if [ -r "$node" ]; then
        echo "INPUT_READABLE: $node"
    else
        echo "INPUT_NOT_READABLE: $node"
    fi
done

echo '=== PERSISTENT INPUT IDENTIFIERS ==='
if [ -d /dev/input/by-id ]; then
    ls -l /dev/input/by-id
else
    echo 'No /dev/input/by-id directory detected'
fi

echo '=== USB HID PREFLIGHT COMPLETE: NO CHANGES MADE ==='
