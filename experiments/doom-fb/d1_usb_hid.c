// SPDX-License-Identifier: GPL-2.0-or-later
// USB keyboard and mouse for DoomGeneric on Linux evdev, no X11/SDL.
// Automatically opens -event-kbd / -event-mouse from /dev/input/by-id.
// EVIOCGRAB is held only while the game runs; file close releases it.
#define _DEFAULT_SOURCE
#include "d1_usb_hid.h"
#define DKEY_UP 0xad
#define DKEY_DOWN 0xaf
#define DKEY_LEFT 0xac
#define DKEY_RIGHT 0xae
#define DKEY_FIRE 0xa3
#define DKEY_USE 0xa2
#define DKEY_SHIFT 0xb6
#define DKEY_ALT 0xb8

#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define INPUT_QSIZE 128
#define MAX_EVENTS_PER_POLL 128

typedef struct { int pressed; unsigned char key; } KeyChange;
static KeyChange changes[INPUT_QSIZE];
static unsigned head, tail;
static int kbd_fd = -1, mouse_fd = -1;
static int mouse_buttons;
static int mouse_dx;
static int mouse_dirty;
static int kbd_grabbed, mouse_grabbed;

static void queue_key(int down, unsigned char doomkey) {
    unsigned next = (tail + 1U) % INPUT_QSIZE;
    if (next == head) return; // Avoid blocking or overwriting input.
    changes[tail] = (KeyChange) { .pressed = down, .key = doomkey };
    tail = next;
}

static int translate_key(unsigned short code) {
    switch (code) {
        case KEY_W: case KEY_UP: return DKEY_UP;
        case KEY_S: case KEY_DOWN: return DKEY_DOWN;
        case KEY_A: case KEY_LEFT: return DKEY_LEFT;
        case KEY_D: case KEY_RIGHT: return DKEY_RIGHT;
        case KEY_F: case KEY_LEFTCTRL: case KEY_RIGHTCTRL: return DKEY_FIRE;
        case KEY_E: case KEY_SPACE: return DKEY_USE;
        case KEY_ENTER: case KEY_KPENTER: return 13;
        case KEY_ESC: return 27;
        case KEY_TAB: return 9;
        case KEY_LEFTSHIFT: case KEY_RIGHTSHIFT: return DKEY_SHIFT;
        case KEY_LEFTALT: case KEY_RIGHTALT: return DKEY_ALT;
        case KEY_Q: return 'q'; // Local quit command in Doom framebuffer backend
        case KEY_P: return 'p';
        case KEY_1: return '1';
        case KEY_2: return '2';
        case KEY_3: return '3';
        case KEY_4: return '4';
        case KEY_5: return '5';
        case KEY_6: return '6';
        case KEY_7: return '7';
        case KEY_8: return '8';
        case KEY_9: return '9';
        case KEY_0: return '0';
        default: return 0;
    }
}

// Return a device only if it can be grabbed exclusively: no characters leak
// into fbcon tty1 while Doom paints the framebuffer.
static int open_device(const char *envname, const char *pattern, const char *label,
                       int *grabbed) {
    char *path = NULL;
    const char *choice = getenv(envname);
    glob_t paths = {0};
    if (choice && *choice) {
        path = choice;
    } else {
        int matched = glob(pattern, 0, NULL, &paths);
        if (matched == 0 && paths.gl_pathc > 0) path = paths.gl_pathv[0];
    }
    if (!path) {
        fprintf(stderr, "D1 USB: no %s found; SSH keyboard remains available.\n", label);
        globfree(&paths);
        return -1;
    }

    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fprintf(stderr, "D1 USB: cannot read %s (%s): %s\n",
                label, path, strerror(errno));
        if (errno == EACCES)
            fprintf(stderr, "D1 USB: from UART root use 'usermod -aG input d1user', then reconnect SSH.\n");
        globfree(&paths);
        return -1;
    }

    if (ioctl(fd, EVIOCGRAB, 1) < 0) {
        fprintf(stderr, "D1 USB: cannot exclusively grab %s (%s): %s\n",
                label, path, strerror(errno));
        close(fd);
        globfree(&paths);
        return -1;
    }
    *grabbed = 1;
    fprintf(stderr, "D1 USB: %s ready (%s).\n", label, path);
    globfree(&paths);
    return fd;
}

void D1_USB_Init(void) {
    kbd_fd = open_device("D1_USB_KEYBOARD",
             "/dev/input/by-id/*-event-kbd", "keyboard", &kbd_grabbed);
    mouse_fd = open_device("D1_USB_MOUSE",
             "/dev/input/by-id/*-event-mouse", "mouse", &mouse_grabbed);
    fprintf(stderr, "D1 USB: keyboard W/A/S/D, F fire, E use, Q quit; "
                    "mouse X turn, left fire, right use.\n");
}

static void process_keyboard(struct input_event *ev) {
    if (ev->type != EV_KEY) return;
    if (ev->value != 0 && ev->value != 1) return; // Ignore auto-repeat (2)
    int key = translate_key(ev->code);
    if (key) queue_key(ev->value == 1, (unsigned char)key);
}

static void process_mouse(struct input_event *ev) {
    if (ev->type == EV_REL && ev->code == REL_X) {
        mouse_dx += ev->value;
        if (mouse_dx > 256) mouse_dx = 256;
        if (mouse_dx < -256) mouse_dx = -256;
        mouse_dirty = 1;
    } else if (ev->type == EV_KEY && ev->code == BTN_LEFT &&
               (ev->value == 0 || ev->value == 1)) {
        if (ev->value) mouse_buttons |= 1; else mouse_buttons &= ~1;
        mouse_dirty = 1;
    } else if (ev->type == EV_KEY && ev->code == BTN_RIGHT &&
               (ev->value == 0 || ev->value == 1)) {
        queue_key(ev->value == 1, DKEY_USE);
    }
    // In classic Doom, vertical mouse movement means forward/back, not
    // looking up/down. Ignore REL_Y to avoid accidental movement.
}

static void drain_device(int fd, int is_mouse) {
    if (fd < 0) return;
    struct input_event ev;
    for (int i = 0; i < MAX_EVENTS_PER_POLL; ++i) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != (ssize_t)sizeof(ev)) {
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK &&
                errno != EINTR)
                fprintf(stderr, "D1 USB: input read error: %s\n", strerror(errno));
            break;
        }
        if (is_mouse) process_mouse(&ev); else process_keyboard(&ev);
    }
}

void D1_USB_Poll(void) {
    drain_device(kbd_fd, 0);
    drain_device(mouse_fd, 1);
}

int D1_USB_GetKey(int *pressed, unsigned char *doom_key) {
    if (head == tail) return 0;
    *pressed = changes[head].pressed;
    *doom_key = changes[head].key;
    head = (head + 1U) % INPUT_QSIZE;
    return 1;
}

int D1_USB_GetMouse(int *buttons, int *dx, int *dy) {
    if (!mouse_dirty) return 0;
    *buttons = mouse_buttons;
    *dx = mouse_dx;
    *dy = 0;
    mouse_dx = 0;
    mouse_dirty = 0;
    return 1;
}

void D1_USB_Close(void) {
    if (kbd_fd >= 0) {
        if (kbd_grabbed) (void)ioctl(kbd_fd, EVIOCGRAB, 0);
        close(kbd_fd);
        kbd_fd = -1;
    }
    if (mouse_fd >= 0) {
        if (mouse_grabbed) (void)ioctl(mouse_fd, EVIOCGRAB, 0);
        close(mouse_fd);
        mouse_fd = -1;
    }
}
