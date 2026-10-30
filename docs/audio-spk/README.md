# D1 Dock SPK audio on Ubuntu 22.10: PCM write EIO

Observed on 2026-10-30 on the physically tested Sipeed LicheeRV D1 Dock,
Ubuntu 22.10 riscv64, kernel `5.17.0-1003-allwinner`. HDMI video, Wi-Fi,
USB keyboard/mouse, and framebuffer Doom are working; preserve this baseline.

## Physical setup

A passive speaker was connected to the Dock's dedicated `SPK` header.
No audible sound was produced by the diagnostic sine wave.

## Kernel ALSA discovery

```text
0 [sun20icodec    ]: sun20i-codec - sun20i-codec
                      sun20i-codec
00-00: sun20i-codec snd-soc-dummy-dai-0 : sun20i-codec snd-soc-dummy-dai-0 : playback 1 : capture 1
/dev/snd/pcmC0D0p
```

The ALSA device exists, but this does **not** establish functional audio output.

## Actual test outcome

Script: [first-tone.py](../../experiments/audio-spk/first-tone.py). Attempt:
440 Hz, 0.35 second, 48 kHz, S16_LE stereo, 3.4% of full-scale peak.

The first run failed on missing `libasound.so.2`. After the ALSA runtime
library was installed, the second run reached the PCM hardware interface:

```text
D1_AUDIO_DEVICE: hw:0,0 = sun20i-codec playback
D1_AUDIO_TEST: 0.35s, 440 Hz, 48 kHz, 2 channel(s), peak 3.4%
D1_AUDIO_NOTE: Mixer volume/routes are NOT changed.
D1_AUDIO_STOP: PCM write error: Input/output error
```

**Result:** PCM write returned `EIO`, and the speaker remained silent.
Neither SPK wiring nor amplifier hardware has been independently verified;
however, the audio stream could not be submitted successfully to the ALSA
device, so this run is not a test of the speaker itself.

## External exact-match precedent

Ubuntu bug **LP #2018657**, `Sound output fails`, reported on a
**Sipeed LicheeRV Dock** running **Ubuntu 23.04, Linux 5.19.0-1009-allwinner**.
The report lists precisely `sun20i-codec snd-soc-dummy-dai-0` and
`aplay: pcm_write:2127: write error: Input/output error` with a connected
speaker. A follow-up identified `EIO` from
`ioctl(fd, SNDRV_PCM_IOCTL_WRITEI_FRAMES, &xferi)`, with the fd referring to
`/dev/snd/pcmC0D0p`.

- https://bugs.launchpad.net/bugs/2018657
- https://www.mail-archive.com/search?f=1&l=kernel-packages%40lists.launchpad.net&o=newest&q=date%3A20230506

This strongly suggests a kernel/ASoC playback-path problem; it does not
prove identical root causes across 5.17 and 5.19.

The manufacturer documents the Dock's on-board amplifier and 4-ohm, 3-W
speaker interface:
https://wiki.sipeed.com/hardware/en/lichee/RV/Dock.html

## Recommended next action (read only)

If further diagnosis is wanted, collect the kernel log **immediately
after another single test**, without changing the driver, mixer or DTB:

```sh
python3 /tmp/d1-first-tone.py
dmesg | grep -Ei 'sun20i|codec|asoc|snd|alsa|dma|pcm|audio' | tail -n 60
```

Do not increase speaker volume as a response to a PCM-write EIO.
Do not replace kernel, DTB, bootloader, HDMI/display driver, or run an
uncontrolled system upgrade on the proven Doom boot media.

## Possible alternative later

Testing Sipeed's own BSP-based Tina/Debian image on **separate** removable
storage may help discriminate software vs speaker hardware; it is not part
of the proven Ubuntu HDMI/Doom baseline. An external USB audio adapter may
avoid the on-board codec but ordinarily needs amplified speakers/headphones
(or a separate amplifier for the passive SPK driver), and requires its own
compatibility validation. Neither alternative is yet tested.
