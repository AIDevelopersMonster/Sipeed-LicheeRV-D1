#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""
Build the already-tested D1 DoomGeneric framebuffer+USB backend on the
vendor Debian 5.4.61 image, without git/curl/wget or system package changes.

Only writes under ~/d1-doom-debian. No framebuffer writes, no modeset,
no kernel/DTB/bootloader changes. Running the built game is a separate step.
"""
import hashlib
import io
import os
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile

UPSTREAM = "e257a09527062bfb986b6e5e0780839b6cbd5b71"
LAB_REF = "0ba70957323bf42df31119f79ab1d400d0b0317e"
UPSTREAM_URL = "https://codeload.github.com/ozkl/doomgeneric/tar.gz/" + UPSTREAM
FREEDOOM_URL = "https://github.com/freedoom/freedoom/releases/download/v0.13.0/freedoom-0.13.0.zip"
LAB_BASE = (
    "https://raw.githubusercontent.com/AIDevelopersMonster/"
    "Sipeed-LicheeRV-D1/" + LAB_REF + "/experiments/doom-fb/"
)
BASE = Path.home() / "d1-doom-debian"
SRC = BASE / "doomgeneric-usb"
MARKER = SRC / ".d1-managed-debian"
WAD = BASE / "freedoom1.wad"

PROBE_SOURCE = r"""
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
int main(void) {
    int fd = open("/dev/fb0", O_RDONLY | O_CLOEXEC);
    if (fd < 0) { perror("open /dev/fb0"); return 1; }
    struct fb_var_screeninfo v = {0};
    struct fb_fix_screeninfo f = {0};
    if (ioctl(fd, FBIOGET_VSCREENINFO, &v) < 0 ||
        ioctl(fd, FBIOGET_FSCREENINFO, &f) < 0) {
        perror("read-only framebuffer ioctl"); close(fd); return 2;
    }
    close(fd);
    printf("D1_FB: id=%s type=%u visual=%u visible=%ux%u virtual=%ux%u "
           "bpp=%u stride=%u smem_len=%u\n",
           f.id, f.type, f.visual, v.xres, v.yres, v.xres_virtual,
           v.yres_virtual, v.bits_per_pixel, f.line_length, f.smem_len);
    printf("D1_FB_COLOR: R%u/%u G%u/%u B%u/%u A%u/%u\n",
           v.red.offset, v.red.length, v.green.offset, v.green.length,
           v.blue.offset, v.blue.length, v.transp.offset, v.transp.length);
    if (f.type != FB_TYPE_PACKED_PIXELS ||
        (f.visual != FB_VISUAL_TRUECOLOR && f.visual != FB_VISUAL_DIRECTCOLOR) ||
        v.xres < 960 || v.yres < 600 || v.bits_per_pixel != 32 ||
        v.red.length != 8 || v.green.length != 8 || v.blue.length != 8 ||
        f.line_length < v.xres * 4 || f.smem_len == 0) {
        fprintf(stderr, "D1_DEBIAN_FB_UNSUPPORTED: no video written\n");
        return 3;
    }
    puts("D1_DEBIAN_FB_PREFLIGHT_PASS: read-only inspection");
    return 0;
}
"""

def download(url, destination, limit):
    if destination.is_file() and destination.stat().st_size > 128:
        print("USING_CACHED:", destination, flush=True)
        return
    temp = destination.with_name(destination.name + ".partial")
    print("DOWNLOADING:", url, flush=True)
    req = urllib.request.Request(url, headers={"User-Agent": "D1Lab-Debian-Doom/1.0"})
    with urllib.request.urlopen(req, timeout=90) as response, temp.open("wb") as out:
        transferred = 0
        while True:
            chunk = response.read(128 * 1024)
            if not chunk:
                break
            transferred += len(chunk)
            if transferred > limit:
                raise RuntimeError("Download exceeds expected size: " + url)
            out.write(chunk)
    if transferred <= 128:
        raise RuntimeError("Downloaded file too small: " + url)
    temp.replace(destination)
    print("DOWNLOAD_OK:", destination.name, transferred, "bytes", flush=True)

def run(*argv, cwd=None):
    print("COMMAND:", " ".join(map(str, argv)), flush=True)
    subprocess.run(list(map(str, argv)), cwd=cwd, check=True)

def replace_once(text, old, new, label):
    occurrences = text.count(old)
    if occurrences != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {occurrences}")
    return text.replace(old, new, 1)

def framebuffer_probe():
    BASE.mkdir(parents=True, exist_ok=True)
    source = BASE / "d1-fb-probe.c"
    exe = BASE / "d1-fb-probe"
    source.write_text(PROBE_SOURCE)
    run("gcc", "-O2", "-Wall", "-o", exe, source)
    run(exe)

def extract_source():
    if SRC.exists():
        if not MARKER.exists():
            raise RuntimeError("Refusing unmanaged existing directory: " + str(SRC))
        print("SOURCE_ALREADY_MANAGED:", SRC, flush=True)
        return
    archive = BASE / ("doomgeneric-" + UPSTREAM + ".tar.gz")
    download(UPSTREAM_URL, archive, 60 * 1024 * 1024)
    SRC.mkdir()
    prefix = "doomgeneric-" + UPSTREAM + "/doomgeneric/"
    count, total = 0, 0
    with tarfile.open(archive, "r:gz") as t:
        for member in t:
            if not member.isfile() or not member.name.startswith(prefix):
                continue
            relative = PurePosixPath(member.name[len(prefix):])
            if not relative.parts or ".." in relative.parts:
                raise RuntimeError("Unsafe archive member")
            if member.size > 10 * 1024 * 1024:
                raise RuntimeError("Unexpectedly large source member")
            total += member.size
            if total > 80 * 1024 * 1024:
                raise RuntimeError("Unexpected source archive volume")
            target = SRC.joinpath(*relative.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            stream = t.extractfile(member)
            if stream is None:
                raise RuntimeError("Cannot extract " + member.name)
            with stream, target.open("wb") as out:
                shutil.copyfileobj(stream, out)
            count += 1
    if count < 50 or not (SRC / "Makefile").exists():
        raise RuntimeError("Upstream DoomGeneric source not found in archive")
    MARKER.write_text("Upstream: " + UPSTREAM + "\nLab commit: " + LAB_REF + "\n")
    print("SOURCE_EXTRACTED:", count, "files", flush=True)

def add_platform_files():
    for name in ("doomgeneric_fb.c", "d1_usb_hid.c", "d1_usb_hid.h"):
        download(LAB_BASE + name, SRC / name, 3 * 1024 * 1024)

def patch_sources():
    p = SRC / "Makefile"
    s = p.read_text()
    if "doomgeneric_xlib.o" in s:
        s = replace_once(s, "doomgeneric_xlib.o", "doomgeneric_fb.o", "Makefile backend")
    if "-lX11" in s:
        s = s.replace("-lX11", "")
    if "doomgeneric_fb.o" not in s or "doomgeneric_xlib.o" in s:
        raise RuntimeError("Framebuffer backend patch missing")
    if "d1_usb_hid.o" not in s:
        s = replace_once(s, "doomgeneric_fb.o",
                         "doomgeneric_fb.o d1_usb_hid.o", "Makefile USB")
    flags = "CFLAGS += -DDOOMGENERIC_RESX=960 -DDOOMGENERIC_RESY=600"
    if flags not in s:
        if "-DDOOMGENERIC_RESX=" in s:
            raise RuntimeError("Conflicting Doom frame dimensions")
        s += "\n# Debian D1: 960x600 centered in HDMI framebuffer\n" + flags + "\n"
    p.write_text(s)

    p = SRC / "i_input.c"
    s = p.read_text()
    if '#include "d1_usb_hid.h"' not in s:
        s = replace_once(s, '#include "doomgeneric.h"',
                         '#include "doomgeneric.h"\n#include "d1_usb_hid.h"',
                         "USB header")
    if "D1_USB_Poll();" not in s:
        s = replace_once(s, "    unsigned char key;",
                         "    unsigned char key;\n    D1_USB_Poll();", "USB poll")
    if "D1_USB_GetKey(&pressed, &key)" not in s:
        s = replace_once(s, "while (DG_GetKey(&pressed, &key))",
                         "while (D1_USB_GetKey(&pressed, &key) || DG_GetKey(&pressed, &key))",
                         "USB keyboard")
    if "D1_USB_GetMouse(&buttons, &dx, &dy)" not in s:
        needle = "\n\n\n                /*\n            case SDL_MOUSEMOTION:"
        addon = """
    {
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
"""
        s = replace_once(s, needle, "\n" + addon + needle, "USB mouse")
    if "D1_USB_Init();" not in s:
        s = replace_once(s, "void I_InitInput(void)\n{\n}",
                         "void I_InitInput(void)\n{\n    D1_USB_Init();\n}",
                         "USB initialization")
    p.write_text(s)

    p = SRC / "doomgeneric_fb.c"
    s = p.read_text()
    if '#include "d1_usb_hid.h"' not in s:
        s = replace_once(s, '#include "doomkeys.h"',
                         '#include "doomkeys.h"\n#include "d1_usb_hid.h"',
                         "Framebuffer USB header")
    if "D1_USB_Close();" not in s:
        s = replace_once(s, "static void cleanup(void)\n{",
                         "static void cleanup(void)\n{\n    D1_USB_Close();",
                         "Framebuffer USB cleanup")
    p.write_text(s)
    print("D1_DEBIAN_USB_CODE_PATCHED", flush=True)

def ensure_wad():
    if WAD.exists() and WAD.stat().st_size > 1000000:
        print("WAD_PRESENT:", WAD, flush=True)
    else:
        archive = BASE / "freedoom-0.13.0.zip"
        download(FREEDOOM_URL, archive, 100 * 1024 * 1024)
        with zipfile.ZipFile(archive) as z:
            options = [x for x in z.namelist() if PurePosixPath(x).name.lower() == "freedoom1.wad"]
            if len(options) != 1:
                raise RuntimeError("Expected one freedoom1.wad, got " + str(options))
            info = z.getinfo(options[0])
            if not 1000000 < info.file_size < 100 * 1024 * 1024:
                raise RuntimeError("Unexpected WAD size")
            with z.open(info) as stream, WAD.open("wb") as out:
                shutil.copyfileobj(stream, out)
    with WAD.open("rb") as f:
        sig = f.read(4)
    if sig not in (b"IWAD", b"PWAD"):
        raise RuntimeError("Bad WAD signature")
    print("FREEDOOM_WAD_READY:", WAD, WAD.stat().st_size, "bytes", flush=True)

def main():
    if sys.platform != "linux":
        raise RuntimeError("Run on the Linux D1, not on Windows")
    if os.geteuid() != 0:
        print("NOTE: running as non-root. Framebuffer and USB access may need groups.")
    print("D1_DEBIAN_DOOM_BUILDER", flush=True)
    framebuffer_probe()
    extract_source()
    add_platform_files()
    patch_sources()
    ensure_wad()
    run("make", "CC=gcc", "-j1", cwd=SRC)
    exe = SRC / "doomgeneric"
    if not exe.is_file() or not os.access(exe, os.X_OK):
        raise RuntimeError("Build did not produce executable")
    print("\nD1_DEBIAN_USB_DOOM_BUILD_PASS", flush=True)
    print("Binary:", exe)
    print("WAD:", WAD)
    print("Launch manually from the *interactive* SSH session:")
    print("  cd ~/d1-doom-debian/doomgeneric-usb")
    print("  ./doomgeneric -iwad ~/d1-doom-debian/freedoom1.wad -nosound -skill 3 -warp 1 1")
    print("Physical USB keyboard Q to quit; SSH Ctrl+C to stop. This script does NOT run game.")
    print("Sound is NOT yet integrated into this framebuffer port.")

if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print("\nD1_DEBIAN_DOOM_BUILD_FAIL:", type(exc).__name__ + ":", exc, file=sys.stderr)
        sys.exit(1)
