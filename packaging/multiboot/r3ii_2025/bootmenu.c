/* Boot menu for the R3II 2025: three frames blitted straight to the
 * framebuffer, countdown from small pre-rendered strips.
 *
 * Runs from the boot hook (S20switch) before the system starts, picks a target,
 * and writes it to the path given on the command line. Any failure -- no
 * framebuffer, no input device, no frames -- exits non-zero so the hook falls
 * back to the marker file it already has.
 *
 *   bootmenu <marker-file> [timeout-seconds]
 *
 * Keys on this device: the wheel reports KEY_LEFT / KEY_RIGHT, so it navigates;
 * PLAYPAUSE or NEXTSONG confirms. Holding PLAYPAUSE (or NEXTSONG) while the
 * device starts is what opens the menu at all.
 */
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define WIDTH   320
#define HEIGHT  480
#define FRAME_BYTES (WIDTH * HEIGHT * 3)
#define FRAME_DIR "/usr/share/bootmenu"

/* Countdown strip geometry, matching what the renderer emits. */
#define STRIP_W 96
#define STRIP_H 30
#define STRIP_X 112
#define STRIP_Y 436
/* The whole bottom line, blanked on first input. */
#define LINE_X 16
#define LINE_Y 428
#define LINE_W 288
#define LINE_H 44

#define BG_R 12
#define BG_G 14
#define BG_B 18

static const char * const TARGETS[] = { "stock", "compas", "sonix" };
#define OPTION_COUNT (int)(sizeof(TARGETS) / sizeof(TARGETS[0]))

static int fb_fd = -1;
static uint8_t * fb_map;
static size_t fb_size;
static struct fb_var_screeninfo var;
static struct fb_fix_screeninfo fix;
static int bpp;

static int open_framebuffer(void) {
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) return -1;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0) return -1;
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0) return -1;
    bpp = var.bits_per_pixel;
    if (bpp != 16 && bpp != 32) return -1;
    fb_size = fix.smem_len;
    fb_map = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_map == MAP_FAILED) { fb_map = NULL; return -1; }
    return 0;
}

static void put_pixel(unsigned x, unsigned y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= WIDTH || y >= HEIGHT) return;
    uint8_t * p = fb_map + (size_t)y * fix.line_length + (size_t)x * (bpp / 8);
    if (bpp == 32) {
        uint32_t v = (0xffu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        memcpy(p, &v, 4);
    } else {
        uint16_t v = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        memcpy(p, &v, 2);
    }
}

static void fill_rect(int x, int y, int w, int h) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) put_pixel((unsigned)(x + i), (unsigned)(y + j), BG_R, BG_G, BG_B);
}

static int blit_rgb24(const char * path, int dx, int dy, int w, int h) {
    FILE * f = fopen(path, "rb");
    if (!f) return -1;
    static uint8_t row[WIDTH * 3];
    size_t row_bytes = (size_t)w * 3;
    for (int y = 0; y < h; y++) {
        if (fread(row, 1, row_bytes, f) != row_bytes) { fclose(f); return -1; }
        for (int x = 0; x < w; x++) {
            put_pixel((unsigned)(dx + x), (unsigned)(dy + y), row[x * 3], row[x * 3 + 1], row[x * 3 + 2]);
        }
    }
    fclose(f);
    return 0;
}

static int draw_frame(int index) {
    char path[128];
    snprintf(path, sizeof(path), FRAME_DIR "/bootmenu_%d.raw", index);
    return blit_rgb24(path, 0, 0, WIDTH, HEIGHT);
}

static int draw_countdown(int seconds) {
    char path[128];
    snprintf(path, sizeof(path), FRAME_DIR "/count_%d.raw", seconds);
    return blit_rgb24(path, STRIP_X, STRIP_Y, STRIP_W, STRIP_H);
}

static int open_inputs(int fds[], int cap) {
    int n = 0;
    for (int i = 0; i < 8 && n < cap; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK);
        if (fd >= 0) fds[n++] = fd;
    }
    return n;
}

/* True when a menu key is already held down: that is how the menu is
 * requested. Power is ignored -- the device is usually still held by it when
 * the hook runs, and that must not open the menu by accident. */
static int any_key_down(int fds[], int n) {
    static const unsigned codes[] = {
        KEY_PLAYPAUSE, KEY_NEXTSONG, KEY_PREVIOUSSONG, KEY_ENTER,
        KEY_LEFT, KEY_RIGHT, KEY_VOLUMEUP, KEY_VOLUMEDOWN,
    };
    for (int i = 0; i < n; i++) {
        uint8_t keys[KEY_CNT / 8 + 1];
        memset(keys, 0, sizeof(keys));
        if (ioctl(fds[i], EVIOCGKEY(sizeof(keys)), keys) < 0) continue;
        for (size_t c = 0; c < sizeof(codes) / sizeof(codes[0]); c++) {
            unsigned code = codes[c];
            if (keys[code / 8] & (1u << (code % 8))) return 1;
        }
    }
    return 0;
}

/* Returns 1 for "previous", -1 for "next", 2 for confirm, 0 for nothing. */
static int poll_keys(int fds[], int n) {
    fd_set set;
    FD_ZERO(&set);
    int maxfd = -1;
    for (int i = 0; i < n; i++) { FD_SET(fds[i], &set); if (fds[i] > maxfd) maxfd = fds[i]; }
    struct timeval tv = { 0, 200000 };
    int rc = select(maxfd + 1, &set, NULL, NULL, &tv);
    if (rc <= 0) return 0;

    int action = 0;
    for (int i = 0; i < n; i++) {
        if (!FD_ISSET(fds[i], &set)) continue;
        struct input_event ev;
        while (read(fds[i], &ev, sizeof(ev)) == (ssize_t)sizeof(ev)) {
            if (ev.type != EV_KEY || ev.value != 1) continue;
            switch (ev.code) {
                case KEY_LEFT:          /* wheel turned one way */
                case KEY_VOLUMEUP:
                case KEY_PREVIOUSSONG:  action = 1; break;
                case KEY_RIGHT:         /* wheel turned the other way */
                case KEY_VOLUMEDOWN:
                case KEY_NEXTSONG:      action = -1; break;
                default:                if (!action) action = 2; break;
            }
        }
    }
    return action;
}

static int write_marker(const char * path, const char * target) {
    FILE * f = fopen(path, "w");
    if (!f) return -1;
    int ok = fputs(target, f) >= 0 && fputc('\n', f) != EOF;
    ok = (fflush(f) == 0) && ok;
    ok = (fsync(fileno(f)) == 0) && ok;
    ok = (fclose(f) == 0) && ok;
    return ok ? 0 : -1;
}

static double elapsed_since(const struct timespec * start) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (double)(now.tv_sec - start->tv_sec) + (now.tv_nsec - start->tv_nsec) / 1e9;
}

int main(int argc, char ** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <marker-file> [timeout]\n", argv[0]);
        return 2;
    }
    const char * marker = argv[1];
    int timeout = argc > 2 ? atoi(argv[2]) : 10;
    if (timeout <= 0) timeout = 10;

    if (open_framebuffer() != 0) { fprintf(stderr, "bootmenu: no framebuffer\n"); return 1; }

    int fds[8];
    int n = open_inputs(fds, 8);
    if (n == 0) { fprintf(stderr, "bootmenu: no input devices\n"); return 1; }

    if (!any_key_down(fds, n)) return 3;

    int selected = 0;
    FILE * m = fopen(marker, "r");
    if (m) {
        char buf[32] = "";
        if (fscanf(m, "%31s", buf) == 1) {
            for (int i = 0; i < OPTION_COUNT; i++) if (!strcmp(buf, TARGETS[i])) selected = i;
        }
        fclose(m);
    }

    if (draw_frame(selected) != 0) { fprintf(stderr, "bootmenu: frames missing\n"); return 1; }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int shown = -1;
    int interacted = 0;
    int confirmed = 0;

    while (!confirmed) {
        if (!interacted) {
            int left = timeout - (int)elapsed_since(&start);
            if (left < 0) left = 0;
            if (left != shown) {
                shown = left;
                if (left > 0) {
                    draw_countdown(left);
                } else {
                    fill_rect(LINE_X, LINE_Y, LINE_W, LINE_H);
                    break;
                }
            }
        }

        int action = poll_keys(fds, n);
        if (action == 2) { confirmed = 1; break; }
        if (action == 1 || action == -1) {
            interacted = 1;
            selected = (selected + (action > 0 ? OPTION_COUNT - 1 : 1)) % OPTION_COUNT;
            draw_frame(selected);
            fill_rect(LINE_X, LINE_Y, LINE_W, LINE_H);
        }
    }

    for (int i = 0; i < n; i++) close(fds[i]);
    return write_marker(marker, TARGETS[selected]) == 0 ? 0 : 1;
}
