# LAB-HDMI-DOOM-01 — direct HDMI Doom / Freedoom on RISC-V D1

**Status: PREPARATION / NO GAMEPLAY PASS YET**  
**Target:** Sipeed LicheeRV D1 Dock / Ubuntu 22.10 / kernel 5.17.0-1003-allwinner / framebuffer `sun4i-drmdrmfb`.  
**2026-10-29**.

## Goal

Run the *software-rendered* Doom engine on the real C906, outputting frames directly to the physical HDMI monitor through the **already-working** `/dev/fb0`. Avoid SSH/X11 network latency. Use legally redistributable Freedoom game data.

Upstream engine: [ozkl/doomgeneric](https://github.com/ozkl/doomgeneric), inspected at commit `e257a09527062bfb986b6e5e0780839b6cbd5b71`. The upstream Xlib demo is **not suitable for direct HDMI**; a separate framebuffer backend is required.

## Hardware preservation

- **Do not** update kernel, U-Boot, DTB, boot arguments, DRM drivers, or HDMI timing.
- No `apt upgrade` or desktop installation on the working card.
- Do not change modes with FBIOPUT_* or DRM modesetting ioctl.
- Keep the UART root shell available as recovery while testing.
- Backup the microSD before framebuffer writes or interactive game tests. Direct pixel writes are temporary, but an untested executable can still fail; a screenshot of color bars proves display output but not program robustness.

## Gate 0 — read-only preflight (perform now)

From the current SSH session:

```bash
curl -fL https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/doom-fb/preflight.sh -o /tmp/d1-doom-preflight.sh
sh /tmp/d1-doom-preflight.sh
```

The script **only reads** system information: framebuffer, VT, permissions, build tools, WAD availability, memory, and Wi-Fi address. It does not touch the video subsystem. Save the console output to the laboratory log.

## Go/no-go gates

1. `/sys/class/tty/tty0/active` → `tty1`, `/proc/fb` → `0 sun4i-drmdrmfb`.
2. Record `virtual_size`, `bits_per_pixel`, and `stride`. Only implement formats that the backend explicitly supports; do not guess RGB byte order.
3. Confirm `gcc`, `make` and legally distributable `freedoom1.wad` (not a proprietary game WAD).
4. Implement and compile the `DG_*` framebuffer backend with a reproducible, pinned upstream engine. Build **without** X11 dependencies.
5. Controlled short run on physical HDMI, preserve console and restore previous framebuffer bytes on exit.
6. Check gameplay and input separately. Only after operator's photo/video can `D1_DOOM_HDMI_RENDER_PASS` be declared.

Note: ownership/permissions differ between `d1user` (no sudo, shown during previous tests) and the UART `root`. Never change `sudoers` solely to launch Doom.

## What has already been verified

- Ubuntu 22.10, physical HDMI console `tty1`, `/dev/fb0`, direct five-color-bar framebuffer drawing — PASS.
- Wi-Fi and SSH — PASS.
- Freedoom on **Ubuntu 24.04.3 through remote X11** — rendering PASS, interactive latency FAIL. This is **not** a HDMI Doom test.

## Links

- [Working Ubuntu 22.10 HDMI baseline](../../docs/ubuntu-22.10-hdmi/README.md)
- [Framebuffer/Wi-Fi hardware receipt](../../results/D1_UBUNTU22_HDMI_WIFI_BASELINE_PASS.md)
- [Read-only preflight](preflight.sh)


## Gate 1 — code published, native build not yet validated on physical D1

Source: [`doomgeneric_fb.c`](doomgeneric_fb.c), [`build.sh`](build.sh), [`get-freedoom.sh`](get-freedoom.sh).

New preflight actual findings (29 Oct 2026):

```text
tty1
0 sun4i-drmdrmfb
virtual_size=1024,600
bits_per_pixel=32
stride=4096
d1user groups: d1user only
FB_READ=NO; FB_WRITE=NO
gcc/make/git/curl=present; unzip=missing
Freedoom WAD=missing
available RAM=311 MiB
```

**Access:** On the existing UART **root** console, run `usermod -aG video d1user`, then fully disconnect and reconnect the SSH session; verify `id` contains `video`. This does not change video drivers, but grants direct framebuffer access. Do not use `chmod 666 /dev/fb0`, do not enable root SSH password login.

**WAD and build (after checking scripts):**

```bash
curl -fL https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/doom-fb/get-freedoom.sh -o /tmp/d1-get-freedoom.sh
sh /tmp/d1-get-freedoom.sh
curl -fL https://raw.githubusercontent.com/AIDevelopersMonster/Sipeed-LicheeRV-D1/main/experiments/doom-fb/build.sh -o /tmp/d1-build-doom.sh
sh /tmp/d1-build-doom.sh
```

These two scripts do **not** write to `/dev/fb0`, modify kernel configuration or install packages. They download upstream sources and game assets into `~/d1-doom-fb` and compile using the system GCC. `build.sh` pins upstream commit `e257a09527062bfb986b6e5e0780839b6cbd5b71`; its upstream source remains GPL licensed.

**Controlled interactive test only after build completes and UART is available:**

```bash
cd ~/d1-doom-fb/upstream/doomgeneric
./doomgeneric -iwad ~/d1-doom-fb/freedoom1.wad -nosound
```

Run in an **interactive** SSH terminal; keys are sent from SSH, pixels go to HDMI. Quit with `q` or Ctrl-C in the active terminal. The backend reads framebuffer layout via read-only FBIOGET_* ioctls, rejects unsupported layouts, never sets video mode, centers its 640x400 rendered rectangle, preserves the original pixels in RAM and restores them upon *normal exit*. **Power loss, SIGKILL, crash, or tty redraw can prevent restoration**; do not imply guaranteed recovery for abnormal stops. A test may temporarily overwrite the framebuffer content but should not alter display configuration. Do not claim hardware/gameplay PASS until the operator confirms the visible result.

SSH terminal keyboard input is an initial prototype; holding keys, autorepeat and menu use may require refinement. FPS logging is a rough per-frame indicator, not benchmark proof.
