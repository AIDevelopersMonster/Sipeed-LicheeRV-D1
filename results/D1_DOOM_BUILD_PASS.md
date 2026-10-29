# D1_DOOM_BUILD_PASS — native DoomGeneric framebuffer port

**Date:** 2026-10-29  
**Target:** Sipeed LicheeRV D1 Dock, Allwinner D1 / T-Head C906  
**OS:** Ubuntu 22.10 `riscv64`, Linux `5.17.0-1003-allwinner`  
**Test:** Native GCC compilation on the physical board; operator-provided console log  
**Status:** **`D1_DOOM_BUILD_PASS`**; direct-HDMI gameplay remains **NOT TESTED**.

## Reproduced results

| Test | Recorded result |
| --- | --- |
| `id` for SSH user | `uid=1001(d1user)`, groups `1001(d1user),44(video)` |
| `/dev/fb0` read and write | `D1_FB_ACCESS_PASS` |
| Freedoom release | Official `freedoom-0.13.0.zip` downloaded and `freedoom1.wad` extracted |
| WAD size | 28,795,076 bytes |
| `freedoom1.wad` SHA-256 | `7323bcc168c5a45ff10749b339960e98314740a734c30d4b9f3337001f9e703d` |
| DoomGeneric upstream | Git commit `e257a09527062bfb986b6e5e0780839b6cbd5b71` |
| Backend | `experiments/doom-fb/doomgeneric_fb.c`, compiled |
| Compiler and linker | Completed successfully |
| Binary | `/home/d1user/d1-doom-fb/upstream/doomgeneric/doomgeneric` |
| ELF | `ELF 64-bit LSB pie executable, UCB RISC-V, RVC, double-float ABI` |
| ELF interpreter | `/lib/ld-linux-riscv64-lp64d.so.1` |
| Linker section sizes (text/data/bss) | 309,461 / 84,092 / 272,592 bytes (not the disk file size) |
| Final marker | `D1_DOOM_BUILD_COMPLETE` |

## Compiler diagnostics

Compilation succeeded with warnings, including:

- `i_sound.c`: unused variables related to sample rate.
- `p_maputl.c`: `-Wpointer-to-int-cast` (64-bit pointer cast to 32-bit `int` in upstream legacy overflow emulation). **Potential portability risk; not dismissed without a runtime test.**
- `m_menu.c`: `fread` return value ignored.
- `p_setup.c`, `wi_stuff.c`: possible string truncation warnings.
- Our `doomgeneric_fb.c`: duplicate `_DEFAULT_SOURCE` definition; nonfatal.
- Other upstream unused-symbol warnings.

There were **no compile or link errors**. This **does not** establish absence of runtime bugs.

## Already verified HDMI baseline (separate result)

- `/proc/fb` → `0 sun4i-drmdrmfb`.
- `/sys/class/tty/tty0/active` → `tty1`.
- `virtual_size=1024,600`; `bits_per_pixel=32`; `stride=4096`.
- Physical display and colored framebuffer stripes proven earlier.

## What is NOT yet claimed

- No result for startup or gameplay of this new native binary on the physical HDMI monitor.
- No FPS or interactive latency measurements.
- No demonstrated on-device keyboard playability.
- No kernel, DTB, bootloader, HDMI mode or display-driver changes required by the build.

## Next check

Perform a short **time-limited** run in an SSH terminal, with UART available as the recovery path, after verifying the original microSD backup strategy. Details and scripts: [LAB-HDMI-DOOM-01](../experiments/doom-fb/README.md).

**Source:** User-supplied console output on 29 October 2026, not an independently executed benchmark.
