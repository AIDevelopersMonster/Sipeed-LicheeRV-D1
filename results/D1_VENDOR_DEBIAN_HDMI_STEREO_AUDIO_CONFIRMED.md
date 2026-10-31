# HDMI stereo audio on Sipeed LicheeRV D1 Dock — physically confirmed

**Date:** 2026-10-31

**Hardware:** Sipeed LicheeRV D1 Dock, Allwinner D1 / XuanTie C906,
HDMI monitor with built-in speakers, USB keyboard and mouse.

**System:** Manufacturer's 20211230 Debian HDMI RTL8723DS image,
kernel `5.4.61`, booted from a separate microSD. The original
`IMAGEWTY` firmware container was converted to a raw disk IMG with
`phoenix2img.py`, then written using USBImager.

## ALSA discovery

The operator independently showed:

```text
 0 [audiocodec]: audiocodec
 1 [snddmic]: snddmic
 2 [sndhdmi]: sndhdmi

card 0: audiocodec, device 0: SUNXI-CODEC 2030000.codec-0
card 2: sndhdmi, device 0: 2034000.daudio-audiohdmi-dai 20340a4.hdmiaudio-0
```

## Physical stereo result

The test proposed and then **audibly confirmed by the operator**:

```sh
timeout 5s speaker-test -D plughw:2,0 -c 2 -r 48000 -t sine -f 440
```

The user reports: **"Работают и левый и правый"** ("Both left and
right work"), referring to the connected HDMI monitor's speakers.
This establishes physical two-channel playback via the D1's HDMI
audio card `sndhdmi` / `plughw:2,0`. This is not an inference
from `aplay -l`; it is a real hardware playback report.

The built-in Dock analog `SPK` output is **still silent** on this
Debian install, even after trying the documented LINEOUT path. Do not
conflate the successful HDMI sound with analog speaker operation.

## Limits and safety

- Ubuntu 22.10 + framebuffer DOOM with USB keyboard and mouse is
  confirmed working on **a different preserved microSD**.
- HDMI audio on that Ubuntu system was not verified; the Ubuntu
  `sun20i-codec` path produced PCM errors/hangs.
- DOOM **on this Debian system** and sound effects in DOOM
  have not yet been tested. An audio-capable game build must select
  `plughw:2,0` (through ALSA or appropriately configured SDL).
- This Debian build previously emitted recurring kernel Oops. The
  operator reported that the burst ceased. The system is not yet
  certified stable under longer-term gameplay.
- No changes to HDMI, DTB, kernel, bootloader, or display mode are
  needed for the stereo tone test.

## Reproduction

With HDMI monitor audio enabled and at a low volume:

```sh
cat /proc/asound/cards
aplay -l
timeout 5s speaker-test -D plughw:2,0 -c 2 -r 48000 -t sine -f 440
```

The next engineering goal is to bring sound effects to a game under
this known-working HDMI audio stack, without regressing the earlier
Ubuntu DOOM demonstration.

See [manufacturer/BSP audio comparison](../docs/audio-spk/reference-implementations.md).
