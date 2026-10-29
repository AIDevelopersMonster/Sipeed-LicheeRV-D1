#!/usr/bin/env bash
# D1 Video Diagnostics v1.0
# Public-video-safe read-only diagnostics; a temporary C example is compiled
# under /tmp and removed on exit. No sudo or secret output is required.
set -u
export LC_ALL=C

PASS=0
FAIL=0
WARN=0

pass() { printf '  [PASS] %s\n' "$1"; PASS=$((PASS + 1)); }
fail() { printf '  [FAIL] %s\n' "$1"; FAIL=$((FAIL + 1)); }
warn() { printf '  [INFO] %s\n' "$1"; WARN=$((WARN + 1)); }
section() {
    printf '\n============================================================\n'
    printf '  %s\n' "$1"
    printf '============================================================\n'
    sleep 2
}

clear
printf '============================================================\n'
printf '       SIPEED LICHEERV D1 DOCK  |  RISC-V C906\n'
printf '           UBUNTU HARDWARE VIDEO DIAGNOSTICS\n'
printf '============================================================\n'
sleep 3

section '01 / OPERATING SYSTEM'
if [ -f /etc/os-release ]; then
    grep '^PRETTY_NAME=' /etc/os-release | tr -d '"'
fi
printf '  Kernel: %s\n' "$(uname -r)"
printf '  Architecture: %s\n' "$(uname -m)"
if [ "$(uname -m)" = 'riscv64' ]; then pass 'RISC-V 64-bit OS'; else fail 'RISC-V architecture'; fi

section '02 / REAL PROCESSOR'
grep -E '^(processor|hart|isa|mmu|uarch)[[:space:]]*:' /proc/cpuinfo || true
if grep -q 'thead,c906' /proc/cpuinfo; then pass 'XuanTie C906 detected'; else fail 'XuanTie C906 identification'; fi

section '03 / PHYSICAL MEMORY'
free -h
if grep -q '^MemTotal:' /proc/meminfo; then pass 'Linux memory information available'; else fail 'Memory information'; fi

section '04 / MICROSD AND FILESYSTEM'
lsblk -o NAME,SIZE,FSTYPE,MOUNTPOINTS
df -h /
if findmnt -n -o SOURCE / | grep -q '^/dev/mmcblk'; then
    pass 'Root filesystem on microSD'
else
    warn 'Root filesystem is not reported as /dev/mmcblk*'
fi

section '05 / WIFI AND IP CONNECTIVITY'
if [ -d /sys/class/net/wlan0 ]; then
    printf '  Interface: wlan0\n'
    driver_path="$(readlink -f /sys/class/net/wlan0/device/driver 2>/dev/null || true)"
    if [ -n "$driver_path" ]; then printf '  Driver: %s\n' "$(basename "$driver_path")"; fi
    printf '  Link state: %s\n' "$(cat /sys/class/net/wlan0/operstate)"
    if [ "$(cat /sys/class/net/wlan0/operstate)" = up ] &&
       ip -4 -o addr show dev wlan0 | grep -q 'inet '; then
        pass 'Wi-Fi link UP and IPv4 assigned'
    else
        fail 'Wi-Fi link or IPv4'
    fi
else
    fail 'wlan0 interface'
fi
if ip -4 route show default | grep -q '^default '; then
    pass 'IPv4 default route'
else
    fail 'IPv4 default route'
fi
printf '  Privacy: IP address, MAC and SSID intentionally hidden\n'

section '06 / REMOTE MANAGEMENT'
if [ -n "$SSH_CONNECTION" ]; then
    pass 'Interactive SSH connection'
else
    warn 'This terminal session is not over SSH'
fi
if systemctl is-active --quiet systemd-networkd; then
    pass 'systemd-networkd running'
else
    warn 'systemd-networkd is not active'
fi

section '07 / NATIVE DEVELOPMENT TOOLCHAIN'
for tool in gcc make gdb python3 objdump; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        fail "Missing tool: $tool"
    fi
done
if command -v gcc >/dev/null 2>&1; then gcc --version | head -n 1; fi
if command -v make >/dev/null 2>&1; then make --version | head -n 1; fi
if command -v gdb >/dev/null 2>&1; then gdb --version | head -n 1; fi
if command -v python3 >/dev/null 2>&1; then python3 --version; fi
if command -v gcc >/dev/null 2>&1 &&
   command -v objdump >/dev/null 2>&1; then
    pass 'Native compiler and disassembler installed'
else
    fail 'Native compiler or disassembler missing'
fi

section '08 / COMPILE AND EXECUTE ON C906'
work="$(mktemp -d 2>/dev/null || true)"
if [ -z "$work" ]; then
    fail 'Temporary build directory'
else
    trap 'rm -rf "$work"' EXIT
    cat > "$work/check.c" <<'C_SOURCE'
#include <stdio.h>
#include <stdint.h>

__attribute__((noinline))
uint64_t add(uint64_t a, uint64_t b) { return a + b; }

int main(void) {
    volatile uint64_t a = 123, b = 456;
    uint64_t result = add(a, b);
    printf("  C906 EXECUTION: 123 + 456 = %llu\n",
           (unsigned long long)result);
    return result == 579 ? 0 : 1;
}
C_SOURCE
    if command -v gcc >/dev/null 2>&1 &&
       gcc -O2 -g -o "$work/check" "$work/check.c" &&
       "$work/check"; then
        pass 'Native C compilation and execution'
        printf '\n  RISC-V ISA disassembly of add():\n'
        if command -v objdump >/dev/null 2>&1; then
            objdump -d -M no-aliases "$work/check" |
                sed -n '/<add>:/,+6p'
        fi
    else
        fail 'Native C compilation or execution'
    fi
fi

section '09 / FINAL DIAGNOSTIC RESULT'
printf '  PASS: %d   FAIL: %d   INFO: %d\n' "$PASS" "$FAIL" "$WARN"
printf '  Secrets: not displayed\n'
if [ "$FAIL" -eq 0 ]; then
    printf '\n  D1_VIDEO_DIAGNOSTICS_PASS\n'
else
    printf '\n  D1_VIDEO_DIAGNOSTICS_REVIEW_REQUIRED\n'
fi
printf '============================================================\n'
if [ "$FAIL" -eq 0 ]; then exit 0; else exit 1; fi
