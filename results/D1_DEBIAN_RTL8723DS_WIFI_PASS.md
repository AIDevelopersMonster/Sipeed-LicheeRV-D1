# D1 vendor Debian: RTL8723DS Wi-Fi association and IP connectivity confirmed

**Date:** 2026-10-31 (operator report; board time may differ)

**Hardware:** Sipeed LicheeRV D1 Dock, vendor Debian kernel `5.4.61 #217`, RTL8723DS Wi-Fi, HDMI display, USB keyboard/mouse. This is the same Debian image whose HDMI stereo audio was confirmed on ALSA `plughw:2,0`.

## Operator-verified result

Using the pre-installed ConnMan `connmanctl`, the adapter scanned and detected multiple 2.4 GHz WLANs. The operator used an interactive ConnMan agent to enter the network password, then selected a secured network. ConnMan displayed the associated service with status **`*AO`**.

`ip -4 addr` subsequently reported **`wlan1`** in state `UP,LOWER_UP` with a valid dynamically allocated private IPv4 address and an active default IPv4 route via `wlan1`. (Private SSID, Wi-Fi password, LAN host address, and router address are intentionally omitted from this public record.)

`ping -c 3 8.8.8.8` returned **3/3 successful replies, 0% packet loss**, with average RTT **46.028 ms**. This confirms external IP connectivity, not yet DNS resolution.

**Important:** On this image, the working Wi-Fi interface is **`wlan1`**, not `wlan0`. Do not hard-code `wlan0` in future scripts.

## Outstanding kernel warning

During/near this network configuration, the operator saw a **single** new `Oops [#183]` at kernel uptime `13961.549990` (one UART notification). Previously the vendor kernel had emitted Oops repeatedly every approximately 25 seconds; those recurrent crashes stopped after `rtkit-daemon.service` was masked, and the operator separately reported one hour of stability.

There is **no evidence yet** that this isolated `Oops [#183]` was caused by Wi-Fi. No call trace or `Comm` for Oops #183 has been captured; avoid attributing causality to RTL8723DS, ConnMan, HDMI, or RTKit until that trace is read.

Read-only next diagnostics:

```sh
dmesg | grep -F -B 8 -A 24 'Oops [#183]'
systemctl is-enabled rtkit-daemon.service
getent hosts github.com
```

Do not change kernel, DTB, U-Boot, or the working HDMI and sound configuration. Preserve the proven Ubuntu 22.10 SD card.

## Conclusion

**PASS — RTL8723DS Wi-Fi connected and working via ConnMan on vendor Debian 5.4.61, external IP ping verified.** DNS and sustained Wi-Fi stability are not yet independently confirmed. Kernel `Oops [#183]` remains under investigation.
