#include "direwolf2.h"
#include <ctype.h>
#include <stdio.h>

static int hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* One USB report per line, with whitespace-separated pairs of hex digits. */
static int parse_hex_line(const char *line, uint8_t *bytes, size_t capacity, size_t *count) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)line;
    while (*p) {
        while (isspace(*p)) ++p;
        if (!*p) break;
        int hi = hex_digit(*p++);
        if (hi < 0 || !*p) return 0;
        int lo = hex_digit(*p++);
        if (lo < 0 || (*p && !isspace(*p)) || n == capacity) return 0;
        bytes[n++] = (uint8_t)((hi << 4) | lo);
    }
    *count = n;
    return n != 0;
}

int main(void) {
    char line[1024];
    uint8_t report[256];
    puts("Portable report decoder. No USB input is opened.");
    puts("Paste one complete USB report per line as hex bytes. Ctrl+Z then Enter to exit on Windows.");
    fflush(stdout);
    while (fgets(line, sizeof line, stdin)) {
        size_t count = 0;
        dw2_state state;
        if (!parse_hex_line(line, report, sizeof report, &count)) {
            puts("Invalid hex line; use pairs of hex digits separated by spaces.");
        } else if (!dw2_parse_report(report, count, &state)) {
            puts("Not a supported input report.");
        } else {
            printf("buttons=0x%04X A=%d B=%d X=%d Y=%d LT=%u RT=%u "
                   "LX=%d LY=%d RX=%d RY=%d",
                   (unsigned)state.buttons, !!(state.buttons & DW2_A), !!(state.buttons & DW2_B),
                   !!(state.buttons & DW2_X), !!(state.buttons & DW2_Y),
                   (unsigned)state.lt, (unsigned)state.rt, (int)state.lx, (int)state.ly,
                   (int)state.rx, (int)state.ry);
            if (state.motion_available)
                printf(" motion_h=%d motion_v=%d (experimental)", (int)state.motion_h, (int)state.motion_v);
            putchar('\n');
        }
        fflush(stdout);
    }
    return 0;
}
