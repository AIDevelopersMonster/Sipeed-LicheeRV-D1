#!/bin/sh
# Install ONLY the native ALSA runtime needed by first-tone.py on Ubuntu 22.10
# (kinetic/riscv64) from the official Ubuntu end-of-life archive.
# Preserves /etc/apt/sources.list, existing kernel, HDMI drivers and DTB.
set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root on the D1 UART console." >&2
    exit 1
fi

if [ ! -r /etc/os-release ]; then
    echo "Cannot identify OS, refusing package installation." >&2
    exit 1
fi
. /etc/os-release
arch="$(dpkg --print-architecture)"
if [ "$ID" != "ubuntu" ] || [ "$VERSION_ID" != "22.10" ] || [ "$arch" != "riscv64" ]; then
    echo "This helper is only for Ubuntu 22.10 riscv64, found: $ID $VERSION_ID $arch" >&2
    exit 1
fi

if python3 -c 'import ctypes; ctypes.CDLL("libasound.so.2")' >/dev/null 2>&1; then
    echo "libasound.so.2 is already available. Nothing to install."
    exit 0
fi

echo "ALSA shared library missing; use the official EOL kinetic package archive."
echo "This does NOT run apt upgrade or edit /etc/apt/sources.list."
echo "Only libasound2 and, if required, libasound2-data are permitted."

list="$(mktemp /tmp/d1-kinetic-alsa-sources.XXXXXX)"
trap 'rm -f "$list"' 0 1 2 15
printf '%s\n' \
    'deb [arch=riscv64] http://old-releases.ubuntu.com/ubuntu kinetic main' \
    > "$list"

# Force just this temporary trusted, signed Ubuntu archive as a source.
# This does update APT's downloadable package metadata cache, but leaves
# /etc/apt/sources.list and other persistent source config untouched.
set -- \
  -o "Dir::Etc::sourcelist=$list" \
  -o "Dir::Etc::sourceparts=-" \
  -o "Acquire::Check-Valid-Until=false" \
  -o "APT::Get::List-Cleanup=0"
apt-get "$@" update

# Guard against any unexpected OS upgrade, removals or unrelated installs.
simulation="$(apt-get "$@" -s --no-install-recommends install libasound2)"
printf '%s\n' "$simulation"
unexpected="$(printf '%s\n' "$simulation" | awk '
    $1=="Remv" { print; next }
    $1=="Inst" && $2!="libasound2" && $2!="libasound2-data" { print }
')"
if [ -n "$unexpected" ]; then
    printf 'Unexpected apt actions; installation ABORTED:\n%s\n' "$unexpected" >&2
    exit 1
fi
if ! printf '%s\n' "$simulation" | grep -Eq '^Inst libasound2 '; then
    echo "apt did not plan to install libasound2; refusing to guess." >&2
    exit 1
fi

apt-get "$@" -y --no-install-recommends install libasound2

python3 -c 'import ctypes; ctypes.CDLL("libasound.so.2"); print("ALSA runtime library is loadable.")'
echo "Done. You can now run: python3 /tmp/d1-first-tone.py"
