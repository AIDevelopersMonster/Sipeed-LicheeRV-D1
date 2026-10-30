#!/usr/bin/env python3
"""Low-level, low-amplitude ALSA speaker test for Sipeed LicheeRV D1 Dock.

No package installations, mixer changes, HDMI/DTB/kernel changes. Stops if
expected sun20i-codec playback PCM is absent.
"""
import array
import ctypes as C
import math
from pathlib import Path
import re
import sys

SAMPLE_RATE = 48000
FREQ_HZ = 440
DURATION_SECONDS = 0.35
PEAK_AMPLITUDE = 1100  # About -29.5 dBFS; 3.4% of 16-bit full-scale.
CARD = b"hw:0,0"


def stop(reason):
    print("D1_AUDIO_STOP:", reason, file=sys.stderr)
    return 1


def main():
    cards = Path("/proc/asound/cards")
    pcms = Path("/proc/asound/pcm")
    dev = Path("/dev/snd/pcmC0D0p")

    if not cards.exists() or not pcms.exists():
        return stop("ALSA information not available. No tone was played.")
    card_text, pcm_text = cards.read_text(), pcms.read_text()
    print("=== ALSA CARDS ===\n" + card_text.strip())
    print("=== ALSA PCM ===\n" + pcm_text.strip())
    if not re.search(r"(?m)^\s*0\s*\[sun20icodec\s*\]", card_text):
        return stop("Card 0 is not the expected sun20icodec; refusing to guess audio output.")
    pcm_lines = [x for x in pcm_text.splitlines() if re.match(r"\s*00-00:\s*", x)]
    if not pcm_lines or not any(re.search(r"\bplayback\s+[1-9][0-9]*\b", x) for x in pcm_lines):
        return stop("Card 0/device 0 has no advertised playback PCM. No tone was played.")
    if not dev.exists():
        return stop("/dev/snd/pcmC0D0p not present. No tone was played.")
    print("D1_AUDIO_DEVICE: hw:0,0 = sun20i-codec playback")
    try:
        lib = C.CDLL("libasound.so.2")
    except OSError as exc:
        return stop("ALSA shared library libasound.so.2 unavailable: " + str(exc))

    def api(name, restype, argtypes):
        f = getattr(lib, name)
        f.restype = restype
        f.argtypes = argtypes
        return f

    void_p = C.c_void_p
    open_pcm = api("snd_pcm_open", C.c_int, [C.POINTER(void_p), C.c_char_p, C.c_int, C.c_int])
    close_pcm = api("snd_pcm_close", C.c_int, [void_p])
    set_params = api("snd_pcm_set_params", C.c_int,
                     [void_p, C.c_int, C.c_int, C.c_uint, C.c_uint, C.c_int, C.c_uint])
    fmt_value = api("snd_pcm_format_value", C.c_int, [C.c_char_p])
    write_pcm = api("snd_pcm_writei", C.c_long, [void_p, void_p, C.c_ulong])
    drain_pcm = api("snd_pcm_drain", C.c_int, [void_p])
    strerror = api("snd_strerror", C.c_char_p, [C.c_int])

    def err(rc):
        return strerror(int(rc)).decode("utf-8", errors="replace")

    handle = void_p()
    rc = open_pcm(C.byref(handle), CARD, 0, 0)  # 0 = playback stream
    if rc < 0:
        return stop("Cannot open hw:0,0: " + err(rc))
    try:
        format_16 = fmt_value(b"S16_LE")
        if format_16 < 0:
            return stop("ALSA does not recognize 16-bit little-endian format")
        channels = 0
        for count in (2, 1):
            # Access 3 = RW_INTERLEAVED; no software resampling.
            rc = set_params(handle, format_16, 3, count, SAMPLE_RATE, 0, 100000)
            if rc == 0:
                channels = count
                break
            print("ALSA setup with %d channels unavailable: %s" % (count, err(rc)))
        if not channels:
            return stop("Codec rejected the safe 48 kHz S16_LE PCM formats")

        frames = round(SAMPLE_RATE * DURATION_SECONDS)
        samples = array.array("h")
        ramp = max(1, round(SAMPLE_RATE * 0.020))
        for index in range(frames):
            edge = min(1.0, index / ramp, (frames - 1 - index) / ramp)
            sample = int(PEAK_AMPLITUDE * edge * math.sin(2 * math.pi * FREQ_HZ * index / SAMPLE_RATE))
            samples.extend([sample] * channels)
        buf = (C.c_short * len(samples)).from_buffer(samples)
        print("D1_AUDIO_TEST: %.2fs, %d Hz, 48 kHz, %d channel(s), peak %.1f%%" %
              (DURATION_SECONDS, FREQ_HZ, channels, 100 * PEAK_AMPLITUDE / 32767))
        print("D1_AUDIO_NOTE: Mixer volume/routes are NOT changed.")
        sent = 0
        while sent < frames:
            offset = sent * channels * C.sizeof(C.c_short)
            wrote = write_pcm(handle, C.byref(buf, offset), frames - sent)
            if wrote <= 0:
                return stop("PCM write error: " + (err(wrote) if wrote < 0 else "0 frames written"))
            sent += wrote
        rc = drain_pcm(handle)
        if rc < 0:
            return stop("PCM drain error: " + err(rc))
        print("D1_AUDIO_PCM_WRITE_COMPLETE: ALSA accepted the test stream.")
        print("Operator must confirm whether tone was actually audible from SPK.")
        return 0
    finally:
        close_pcm(handle)


if __name__ == "__main__":
    sys.exit(main())
