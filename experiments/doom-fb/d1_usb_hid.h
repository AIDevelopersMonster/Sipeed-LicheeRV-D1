// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef D1_USB_HID_H
#define D1_USB_HID_H

// Read-only access to Linux evdev keyboard/mouse input. No display mode changes.
void D1_USB_Init(void);
void D1_USB_Poll(void);
int D1_USB_GetKey(int *pressed, unsigned char *doom_key);
int D1_USB_GetMouse(int *buttons, int *dx, int *dy);
void D1_USB_Close(void);

#endif
