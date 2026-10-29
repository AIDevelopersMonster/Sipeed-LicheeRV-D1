// SPDX-License-Identifier: GPL-2.0-or-later
// Sipeed LicheeRV D1 framebuffer backend for ozkl/doomgeneric.
// No mode-setting ioctls. Terminal input is read from the SSH session.
#define _DEFAULT_SOURCE
#include "doomgeneric.h"
#include "doomkeys.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define QUEUE_SIZE 128
#define KEY_HOLD_MS 170

typedef struct { int pressed; unsigned char key; } KeyEvent;

static int g_fb_fd = -1;
static unsigned char *g_map = MAP_FAILED;
static size_t g_map_len;
static unsigned char *g_backup;
static size_t g_width_bytes;
static size_t g_xoffset_bytes, g_yoffset;
static struct fb_fix_screeninfo g_fix;
static struct fb_var_screeninfo g_var;
static struct termios g_old_term;
static int g_term_saved;
static int g_stdin_flags = -1;
static int g_stdin_flags_saved;
static volatile sig_atomic_t g_stop;
static KeyEvent g_keys[QUEUE_SIZE];
static unsigned g_head, g_tail;
static unsigned char g_down[256];
static uint32_t g_expires[256];
static int g_esc_state;
static uint32_t g_esc_deadline;
static uint32_t g_frame_last_report;
static unsigned g_frame_count;

static uint32_t ticks(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return (uint32_t)((uint64_t)now.tv_sec * 1000U + now.tv_nsec / 1000000U);
}

static void stop_handler(int signal_number)
{
    (void)signal_number;
    g_stop = 1;
}

static void cleanup(void)
{
    if (g_term_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
    if (g_stdin_flags_saved) fcntl(STDIN_FILENO, F_SETFL, g_stdin_flags);

    // Restore only the rectangle we changed; never change display mode.
    if (g_map != MAP_FAILED && g_backup != NULL) {
        for (unsigned y = 0; y < DOOMGENERIC_RESY; ++y) {
            size_t offset = (g_yoffset + y) * g_fix.line_length + g_xoffset_bytes;
            memcpy(g_map + offset, g_backup + y * g_width_bytes, g_width_bytes);
        }
    }
    free(g_backup);
    if (g_map != MAP_FAILED) munmap(g_map, g_map_len);
    if (g_fb_fd >= 0) close(g_fb_fd);
    fprintf(stderr, "\nD1_DOOM_FB_EXIT: resources released, previous framebuffer area restored.\n");
}

static void fail(const char *message)
{
    fprintf(stderr, "D1_DOOM_FB_FAIL: %s: %s\n", message, strerror(errno));
    exit(EXIT_FAILURE);
}

static void fail_unsupported(const char *message)
{
    fprintf(stderr, "D1_DOOM_FB_UNSUPPORTED: %s\n", message);
    exit(EXIT_FAILURE);
}

void DG_Init(void)
{
    atexit(cleanup);
    if (!isatty(STDIN_FILENO)) fail_unsupported("Run in an interactive SSH or UART terminal");

    g_fb_fd = open("/dev/fb0", O_RDWR | O_CLOEXEC);
    if (g_fb_fd < 0) fail("open /dev/fb0 (add d1user to video group from UART)");
    if (ioctl(g_fb_fd, FBIOGET_FSCREENINFO, &g_fix) < 0) fail("FBIOGET_FSCREENINFO");
    if (ioctl(g_fb_fd, FBIOGET_VSCREENINFO, &g_var) < 0) fail("FBIOGET_VSCREENINFO");

    fprintf(stderr, "D1_DOOM_FB: screen %ux%u, virtual %ux%u, %ubpp, stride %u\n",
            g_var.xres, g_var.yres, g_var.xres_virtual, g_var.yres_virtual,
            g_var.bits_per_pixel, g_fix.line_length);
    fprintf(stderr, "D1_DOOM_FB: RGBA offsets R%u G%u B%u A%u, lengths %u/%u/%u/%u\n",
            g_var.red.offset, g_var.green.offset, g_var.blue.offset, g_var.transp.offset,
            g_var.red.length, g_var.green.length, g_var.blue.length, g_var.transp.length);

    if (g_fix.type != FB_TYPE_PACKED_PIXELS ||
        (g_fix.visual != FB_VISUAL_TRUECOLOR && g_fix.visual != FB_VISUAL_DIRECTCOLOR) ||
        g_var.bits_per_pixel != 32 ||
        g_var.red.length != 8 || g_var.green.length != 8 || g_var.blue.length != 8 ||
        g_var.red.msb_right || g_var.green.msb_right || g_var.blue.msb_right ||
        g_var.red.offset > 24 || g_var.green.offset > 24 || g_var.blue.offset > 24 ||
        (g_var.transp.length != 0 && g_var.transp.length != 8) ||
        (g_var.transp.length == 8 && (g_var.transp.offset > 24 || g_var.transp.msb_right))) {
        fail_unsupported("Unsupported framebuffer layout; mode unchanged");
    }
    if (g_var.xres < DOOMGENERIC_RESX || g_var.yres < DOOMGENERIC_RESY ||
        g_fix.line_length == 0 || g_fix.smem_len == 0) {
        fail_unsupported("Framebuffer too small or invalid; mode unchanged");
    }

    unsigned x = (g_var.xres - DOOMGENERIC_RESX) / 2;
    unsigned y = (g_var.yres - DOOMGENERIC_RESY) / 2;
    if ((uint64_t)g_var.xoffset + x + DOOMGENERIC_RESX > g_var.xres_virtual ||
        (uint64_t)g_var.yoffset + y + DOOMGENERIC_RESY > g_var.yres_virtual) {
        fail_unsupported("Framebuffer virtual coordinates invalid");
    }

    g_width_bytes = (size_t)DOOMGENERIC_RESX * 4U;
    g_xoffset_bytes = ((size_t)g_var.xoffset + x) * 4U;
    g_yoffset = (size_t)g_var.yoffset + y;
    if (g_xoffset_bytes + g_width_bytes > g_fix.line_length) {
        fail_unsupported("Framebuffer row exceeds stride");
    }
    uint64_t final_byte = (uint64_t)(g_yoffset + DOOMGENERIC_RESY - 1U) *
                          g_fix.line_length + g_xoffset_bytes + g_width_bytes;
    if (final_byte > g_fix.smem_len) fail_unsupported("Frame exceeds mapped video RAM");

    g_map_len = g_fix.smem_len;
    g_map = mmap(NULL, g_map_len, PROT_READ | PROT_WRITE, MAP_SHARED, g_fb_fd, 0);
    if (g_map == MAP_FAILED) fail("mmap framebuffer");

    g_backup = malloc((size_t)DOOMGENERIC_RESY * g_width_bytes);
    if (!g_backup) fail("allocate framebuffer backup");
    for (unsigned row = 0; row < DOOMGENERIC_RESY; ++row) {
        size_t offset = (g_yoffset + row) * g_fix.line_length + g_xoffset_bytes;
        memcpy(g_backup + row * g_width_bytes, g_map + offset, g_width_bytes);
    }

    if (tcgetattr(STDIN_FILENO, &g_old_term) < 0) fail("tcgetattr");
    g_term_saved = 1;
    struct termios t = g_old_term;
    t.c_lflag &= ~(ICANON | ECHO); // keep ISIG, so Ctrl-C stops safely
    t.c_iflag &= ~(IXON | ICRNL);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) < 0) fail("tcsetattr");
    g_stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
    if (g_stdin_flags < 0) fail("fcntl F_GETFL");
    g_stdin_flags_saved = 1;
    if (fcntl(STDIN_FILENO, F_SETFL, g_stdin_flags | O_NONBLOCK) < 0)
        fail("fcntl F_SETFL");
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);
    fprintf(stderr, "D1_DOOM_FB_READY: centered %dx%d, no modeset.\n",
            DOOMGENERIC_RESX, DOOMGENERIC_RESY);
    fprintf(stderr, "SSH controls: WASD/arrows move, F fire, SPACE/E use, ENTER select, ESC menu, Q quit, Ctrl-C stop.\n");
    g_frame_last_report = ticks();
}

void DG_DrawFrame(void)
{
    const uint32_t *source = (const uint32_t *)DG_ScreenBuffer;
    for (unsigned row = 0; row < DOOMGENERIC_RESY; ++row) {
        uint32_t *dest = (uint32_t *)(g_map + (g_yoffset + row) * g_fix.line_length + g_xoffset_bytes);
        for (unsigned col = 0; col < DOOMGENERIC_RESX; ++col) {
            uint32_t pix = source[row * DOOMGENERIC_RESX + col]; // 0x00RRGGBB from DoomGeneric
            uint32_t rgb = (((pix >> 16) & 255U) << g_var.red.offset) |
                           (((pix >> 8) & 255U) << g_var.green.offset) |
                           ((pix & 255U) << g_var.blue.offset);
            if (g_var.transp.length == 8) rgb |= 255U << g_var.transp.offset;
            dest[col] = rgb;
        }
    }
    g_frame_count++;
    uint32_t now = ticks();
    if (now - g_frame_last_report >= 5000U) {
        float fps = 1000.0f * g_frame_count / (now - g_frame_last_report);
        fprintf(stderr, "D1_DOOM_FB_FPS approx %.1f frames/s\n", fps);
        g_frame_count = 0;
        g_frame_last_report = now;
    }
}

void DG_SleepMs(uint32_t milliseconds)
{
    struct timespec ts = { .tv_sec = milliseconds / 1000U,
                           .tv_nsec = (long)(milliseconds % 1000U) * 1000000L };
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR && !g_stop) {}
}

uint32_t DG_GetTicksMs(void) { return ticks(); }
void DG_SetWindowTitle(const char *title) { (void)title; }

static void enqueue(int pressed, unsigned char key)
{
    unsigned next = (g_tail + 1U) % QUEUE_SIZE;
    if (next == g_head) return;
    g_keys[g_tail] = (KeyEvent){ .pressed = pressed, .key = key };
    g_tail = next;
}

static void press(unsigned char key)
{
    if (!g_down[key]) {
        g_down[key] = 1;
        enqueue(1, key);
    }
    g_expires[key] = ticks() + KEY_HOLD_MS;
}

static void input_byte(unsigned char c)
{
    if (g_esc_state == 1) {
        g_esc_state = 0;
        if (c == '[' || c == 'O') { g_esc_state = 2; return; }
        press(KEY_ESCAPE);
    } else if (g_esc_state == 2) {
        g_esc_state = 0;
        if (c == 'A') { press(KEY_UPARROW); return; }
        if (c == 'B') { press(KEY_DOWNARROW); return; }
        if (c == 'C') { press(KEY_RIGHTARROW); return; }
        if (c == 'D') { press(KEY_LEFTARROW); return; }
        return;
    }
    if (c == 27) { g_esc_state = 1; g_esc_deadline = ticks() + 60U; return; }
    if (c == 'q' || c == 'Q') { g_stop = 1; return; }
    if (c == 'w' || c == 'W') { press(KEY_UPARROW); return; }
    if (c == 's' || c == 'S') { press(KEY_DOWNARROW); return; }
    if (c == 'a' || c == 'A') { press(KEY_LEFTARROW); return; }
    if (c == 'd' || c == 'D') { press(KEY_RIGHTARROW); return; }
    if (c == 'f' || c == 'F') { press(KEY_FIRE); return; }
    if (c == 'e' || c == 'E' || c == ' ') { press(KEY_USE); return; }
    if (c == '\n' || c == '\r') { press(KEY_ENTER); return; }
    if (c == '\t') { press(KEY_TAB); return; }
    if (c >= '0' && c <= '9') { press(c); return; }
    if (c == 'p' || c == 'P') { press('p'); return; }
}

int DG_GetKey(int *pressed, unsigned char *key)
{
    unsigned char input[64];
    ssize_t count;
    while ((count = read(STDIN_FILENO, input, sizeof(input))) > 0) {
        for (ssize_t i = 0; i < count; ++i) input_byte(input[i]);
    }
    uint32_t now = ticks();
    if (g_esc_state == 1 && (int32_t)(now - g_esc_deadline) >= 0) {
        g_esc_state = 0;
        press(KEY_ESCAPE);
    }
    for (unsigned i = 0; i < 256; ++i) {
        if (g_down[i] && (int32_t)(now - g_expires[i]) >= 0) {
            g_down[i] = 0;
            enqueue(0, (unsigned char)i);
        }
    }
    if (g_head == g_tail) return 0;
    *pressed = g_keys[g_head].pressed;
    *key = g_keys[g_head].key;
    g_head = (g_head + 1U) % QUEUE_SIZE;
    return 1;
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);
    while (!g_stop) doomgeneric_Tick();
    return EXIT_SUCCESS;
}
