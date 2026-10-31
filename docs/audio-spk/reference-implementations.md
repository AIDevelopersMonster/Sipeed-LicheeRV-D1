# Reference audio implementations: Sipeed LicheeRV D1 Dock

Research date: 2026-10-30. This is a **literature/source review**, not a new
hardware PASS for our Ubuntu 22.10 machine.

## 1. Manufacturer's actual audio playback workflow

Sipeed's LicheeRV basic usage explicitly demonstrates the audio stack in
their BSP/Tina-era images:

- ALSA card **0: audiocodec**: analog codec, playback at `hw:0,0`;
- ALSA card **1: snddmic**: digital microphone;
- ALSA card **2: sndhdmi**: HDMI output, playback at `hw:2,0`.

Published commands:

```sh
arecord -D hw:1,0 -f S16_LE -t wav -d 3 t.wav
aplay -D hw:0,0 t.wav
ffmpeg -i /mnt/UDISK/badapple_640480_xvid.mp4 -pix_fmt bgra -f fbdev /dev/fb0 -f alsa hw:0,0
ffmpeg -i /mnt/UDISK/badapple_640480_xvid.mp4 -pix_fmt bgra -f fbdev /dev/fb0 -f alsa hw:2,0
```

These commands describe the **Sipeed image** and must NOT be used to assume
that our Ubuntu exposes `sndhdmi`.

Source: https://wiki.sipeed.com/hardware/en/lichee/RV/user.html

The Dock officially provides a passive speaker connector with on-board
amplification rated to support a 4-ohm / 3-W speaker:
https://wiki.sipeed.com/hardware/en/lichee/RV/Dock.html

## 2. Other independent, real work

### Music via HDMI

An independently authored Dock review describes playing a local MP3 file
through `ffmpeg` and `hw:2,0`:
https://boardor.com/blog/lichee-d1-dock-pro-development-board-review

### DOOM on the exact hardware (2022)

YuzukiTsuru's article dated 2022-01-27, `在 Lichee RV 上玩游戏：DOOM`,
describes a Tina/SDL DOOM port and Dock gameplay. **Audio in DOOM is not
documented as working by the cited text**, so do not cite as proof of
DOOM sound.
https://bbs.aw-ol.com/topic/997/%E5%9C%A8-lichee-rv-%E4%B8%8A%E7%8E%A9%E6%B8%B8%E6%88%8F-doom

An independent register-level project also reports DOOM on the D1 and a
Sipeed Lichee RV + Dock target; not an audio implementation:
https://github.com/ylyamin/allwinner_d1_hal

### Voice demo

Bilibili search indexed `LicheeRV音频播放测试-天气播报`
(`LicheeRV audio playback test — weather report`), uploaded 2022-03-20.
Board setup, image and audio path not independently verified.
https://search.bilibili.com/all?keyword=licheerv

## 3. Relevant confirmed mixer-route fix on BSP/Tina, not our Ubuntu

Community users of D1s Tina/tplayer report fixing silent playback by
opening the LINEOUT mixer path. The forum lists:

```text
tinymix set 15 31
tinymix set 17 1
tinymix set 18 1
tinymix set 31 1
```

Another project for Allwinner D1-H recommends **named** controls
`LINEOUTL Output Select`, `LINEOUTR Output Select`,
`LINEOUT volume`, and `LINEOUT Switch`, not opaque numeric indices.

Sources:
https://bbs.aw-ol.com/topic/1598/d1s-tina-sdk-2-0%E4%B8%ADtplayer%E9%97%AE%E9%A2%98/8?lang=zh-CN
https://docs.100ask.net/D1h-DualDisplay/part6/XR829ChipAdaptation/

**Never apply `tinymix` numeric indices from the BSP to Ubuntu
`sun20icodec`.** They are different control sets and the observed Ubuntu
error is PCM write `EIO` / a blocked write, not merely lack of audible
volume. Mixer configuration might be one additional issue, but cannot
yet explain the entire observed error sequence.

## 4. Compare current Ubuntu 22.10 against BSP

Our board: Ubuntu 22.10 riscv64 / 5.17.0-1003-allwinner:

```text
0 [sun20icodec]: sun20i-codec
00-00: sun20i-codec snd-soc-dummy-dai-0 : playback 1 : capture 1
```

Only one ALSA card; no `sndhdmi` card. The kernel log contains an
`hdmi-audio-codec` DAI registration, but that does **not** constitute
a separately usable HDMI sound card.

User-observed:
- original `first-tone.py`: `snd_pcm_writei()` -> `Input/output error`
- repeat: blocked in `snd_pcm_writei()`, operator used Ctrl+C
- raising the signal level (3.4% to 50%) is **not** a fix or test of
  the audio device's ability to transfer PCM data.

Exactly matching Ubuntu failure on LicheeRV Dock (Ubuntu 23.04, 5.19),
reported in LP #2018657 in 2023, with `sun20i-codec snd-soc-dummy-dai-0`
and `aplay` write `EIO`:
https://bugs.launchpad.net/bugs/2018657
https://www.mail-archive.com/search?f=1&l=kernel-packages%40lists.launchpad.net&o=newest&q=date%3A20230506

Ubuntu's D1 audio driver is explicitly carried as `SAUCE`, including
a `[WIP]` codec patch:
https://lists.ubuntu.com/archives/kinetic-changes/2022-October/008424.html

Different/upstream effort: D1/T113s audio codec patch series, Aug 2023:
https://patchew.org/linux/20230805180506.718364-1-bigunclemax%40gmail.com/

This source evidence supports a **software/driver-family explanation**,
not a conclusive diagnosis of our exact 5.17 kernel internals.

## 5. Preferred engineering plan

1. **Preserve** the original Ubuntu 22.10 SD card and working HDMI/Doom.
2. On a **separate microSD**, prepare the matching official Sipeed
   `LicheeRV_Tina_hdmi_8723ds` BSP image using the vendor's recommended
   `PhoenixCard` process, not an assumed raw imaging method; verify the
   actual boot first using the UART serial console. (An earlier attempt
   with this family of image yielded no visible display; UART boot
   verification is essential before labeling it broken.)
   Official flashing guidance:
   https://wiki.sipeed.com/hardware/en/lichee/RV/flash.html
3. If the Sipeed image boots, read `cat /proc/asound/cards`,
   `aplay -l` and named mixer controls, verify SPK with a short quiet
   known-good WAV file. Then optionally verify the monitor's HDMI audio.
4. If sound is physically confirmed there, choose:
   - Keep the Ubuntu 22.10 gameplay system and add a separately validated
     USB audio output; or
   - Isolate audio driver/DT/mixer differences from the functioning
     BSP in an **offline** comparison before contemplating a carefully
     recoverable change to the Ubuntu baseline.

**Do not** attempt an in-place kernel swap or blindly copy DTB,
`tinymix` indices, display/audio modes, or run `apt upgrade` on the
verified gameplay SD card.

## 6. Vendor **Debian** reference image: better matched to the manufacturer example

The manufacturer’s `root@MaixLinux` audio example is documented under
“Notes about Debian” on the Dock basic-use page; the audio example includes
three cards: analog `audiocodec`, `snddmic`, and `sndhdmi`:

https://wiki.sipeed.com/hardware/en/lichee/RV/user.html

For the actual Dock + HDMI + **RTL8723DS** board, Sipeed's FAQ names
**`20211230_LicheeRV_debian_d1_hdmi_8723ds.7z`** and explains why the
`LicheeRV_Debian_hdmi_xr829.7z` variant lacked appropriate Wi-Fi:

https://wiki.sipeed.com/hardware/en/lichee/RV/problems.html

Image naming and PhoenixCard setup:
https://wiki.sipeed.com/hardware/en/lichee/RV/flash.html

This is a **vendor-customized Debian/BSP** implementation, *not* evidence
that generic Debian uses a fully working upstream audio driver.

External BSP DTS examples have a specifically named
`allwinner,sunxi-dummy-cpudai` at `0x203034c`, with actual
`dmas = <&dma 7>, <&dma 7>` and `dma-names = "tx", "rx"`.
This is not interchangeable with the generic ASoC
`snd-soc-dummy-dai` label seen in Ubuntu; the latter is not, by
itself, proof of a broken data path.

Sources:
https://bbs.aw-ol.com/user/dyd2022/posts
https://code.googlesource.com/linux/torvalds/linux/+/f155f3b3ed1af23884ffaffe8a669722b87ac9d6/sound/soc/soc-utils.c

**Next validation:** boot precisely the matched vendor Debian image on a
different SD card; inspect `cat /proc/asound/cards` and `aplay -l` and
use a short low-volume WAV to test analog SPK on `hw:0,0`. If
`sndhdmi` also exists, consider an HDMI sound test via `hw:2,0`.
Only a physically heard tone establishes functionality on our board.
Keep the working Ubuntu 22.10 SD untouched.


## 7. Physical vendor Debian boot and ALSA enumeration (2026-10-31)

**Operator-observed result**, using the same Sipeed LicheeRV D1 Dock
hardware as the earlier Ubuntu DOOM experiment:

- The archive `20211230_LicheeRV_debian_d1_hdmi_8723ds.7z` contained
  a 4,004,696,064-byte `.img` with `IMAGEWTY` at its beginning.
  This is an Allwinner firmware container, **not** a raw SD image.
- Conversion through `phoenix2img.py` produced a
  **8,621,146,624-byte** raw disk image with an MBR signature `55 AA`
  and GPT magic `EFI PART`.
- PhoenixCard attempts had failed; BalenaEtcher refused the image.
  USBImager successfully wrote the converted file to a separate SD card.
- **Debian booted physically; HDMI, USB keyboard, and USB mouse worked.**
- Early UART output showed a series of kernel `Oops` messages,
  some approximately 25 seconds apart. The operator later reported
  that this burst stopped. This build is **not certified stable**.
- Serial console shows `root@sipeed`, with Linux kernel **5.4.61**.

Read-only hardware enumeration reported by operator:

```text
root@sipeed:~# uname -r
5.4.61

root@sipeed:~# cat /proc/asound/cards
 0 [audiocodec     ]: audiocodec - audiocodec
                      audiocodec
 1 [snddmic        ]: snddmic - snddmic
                      snddmic
 2 [sndhdmi        ]: sndhdmi - sndhdmi
                      sndhdmi

root@sipeed:~# aplay -l
**** List of PLAYBACK Hardware Devices ****
card 0: audiocodec [audiocodec], device 0: SUNXI-CODEC 2030000.codec-0 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
card 2: sndhdmi [sndhdmi], device 0: 2034000.daudio-audiohdmi-dai 20340a4.hdmiaudio-0 []
  Subdevices: 1/1
  Subdevice #0: subdevice #0
```

These are exactly the manufacturer's Debian sound card names and ALSA
indices. **Only device enumeration is physically confirmed.** Actual
sound on SPK `hw:0,0` or HDMI `hw:2,0` has NOT yet been heard or
confirmed. Do not claim otherwise. The next short test, with modest
mixer volume and no configuration changes, is:

```sh
command -v speaker-test
amixer -c 0 scontrols
timeout 3s speaker-test -D hw:0,0 -c 2 -r 48000 -t sine -f 440
```

Stop further playback tests if the known kernel `Oops` recurs.
Keep the proven Ubuntu 22.10 + DOOM microSD untouched.


## 8. Vendor Debian PCM playback runs but SPK silent: route hypothesis (2026-10-31)

On **Debian 5.4.61** after the exact vendor `audiocodec/snddmic/sndhdmi`
enumeration, the operator ran:

```sh
timeout 3s speaker-test -D hw:0,0 -c 2 -r 48000 -t sine -f 440
```

The `speaker-test` log showed its 48000 Hz, S16_LE, stereo playback
stream, left/right channel messages and `Time per period = 2.899526`
without the Ubuntu-era PCM write `EIO`. **The operator heard no sound
from the connected passive speaker.** This confirms a different failure
mode from Ubuntu and does NOT confirm any physically functional audio.

The operator's `amixer -c 0 scontrols` exposes the vendor driver
controls `LINEOUT`, `LINEOUT volume`, `LINEOUTL Output Select`,
`LINEOUTR Output Select`, `DAC volume`, `digital volume`,
`Headphone`, and `HpSpeaker`, among others.

**Source-derived next test (NOT yet executed or physically validated):**

The Allwinner D1 Linux AudioCodec Development Guide 1.0 (2021-04-14),
section 4.3.2, documents `LINEOUT + SPK` with `tinymix` controls:
15→LINEOUT volume, 17/18→differential output mux,
31→LINEOUT switch. Its documented test uses
`tinymix 15 31; tinymix 17 1; tinymix 18 1; tinymix 31 1`.
For an initial short test use a more conservative level
20/31 rather than maximum 31/31, and use **named ALSA controls**
matching the Debian card:

```sh
amixer -c 0 cset name='LINEOUT Switch' 0
amixer -c 0 cset name='LINEOUT volume' 20
amixer -c 0 cset name='LINEOUTL Output Select' 1
amixer -c 0 cset name='LINEOUTR Output Select' 1
amixer -c 0 cset name='LINEOUT Switch' 1
timeout 4s speaker-test -D hw:0,0 -c 2 -r 48000 -t sine -f 440
```

Reference guide:
https://bbs.aw-ol.com/assets/uploads/files/1728962087800-d1_linux_audiocodec_%E5%BC%80%E5%8F%91%E6%8C%87%E5%8D%97.pdf

A parallel vendor-oriented tutorial demonstrates `amixer -D hw:audiocodec
cset name='LINEOUT Switch' 1`, `LINEOUT Output Select`, and
`LINEOUT volume` on a related Allwinner SoC:
https://icode.best/i/775124165991566

Potential **next checks if silent after routing** are reading the actual
DAC/digital mixer values and identifying whether a GPIO-controlled
external power amplifier is enabled on the Dock. Do not assert it is a
hardware fault or modify any boot files. This Debian build previously
emitted recurring kernel Oops, so stop playback testing if they recur.


## 9. Debian stability after rtkit-daemon block (2026-10-31)

The user confirmed working HDMI stereo output (both left and right speakers) using ALSA `plughw:2,0` on Sipeed LicheeRV D1 Dock, vendor Debian kernel 5.4.61. Repeated kernel Oops roughly every 25 seconds were correlated with D-Bus activation of `rtkit-daemon.service` by PulseAudio; the service exited with signal SEGV. Its package was not reported as installed by APT, but systemd service logs unambiguously confirmed attempted starts and crashes.

The user applied `systemctl mask --runtime --now rtkit-daemon.service`. Following the block, the user **reported one hour without kernel errors or HDMI resets**. This is an observed one-hour stability check, not proof of long-term stability. Whether the service mask is also persistent under `/etc/systemd/system` and whether stability holds after reboot remain unverified.

Next: verify mask persistence before reboot; preserve working SD image; test HDMI playback with application audio (DOOM) while keeping the analog SPK fault as a separate task.
