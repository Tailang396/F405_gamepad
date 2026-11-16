#include "direwolf2.h"
#include <assert.h>
#include <string.h>

int main(void) {
    uint8_t data[20] = {0x00, 0x14, 0x01, 0x10, 0x80, 0xff,
                        0x00, 0x80, 0xff, 0x7f, 0xff, 0xff, 0x34, 0x12,
                        0x00, 0xff, 0x8f};
    dw2_state s, old;
    memset(&s, 0x55, sizeof(s));
    old = s;
    assert(!dw2_parse_report(data, 13, &s));
    assert(memcmp(&s, &old, sizeof(s)) == 0);
    assert(!dw2_parse_report(NULL, 20, &s));
    assert(!dw2_parse_report(data, 20, NULL));
    data[0] = 1;
    assert(!dw2_parse_report(data, 20, &s));
    data[0] = 0;
    data[1] = 0x13;
    assert(!dw2_parse_report(data, 20, &s));
    data[1] = 0x14;
    assert(dw2_parse_report(data, 14, &s));
    assert(s.buttons == (DW2_A | DW2_UP) && s.lt == 128 && s.rt == 255);
    assert(s.lx == -32768 && s.ly == 32767 && s.rx == -1 && s.ry == 0x1234);
    assert(!s.motion_available);
    assert(dw2_parse_report(data, sizeof(data), &s));
    assert(s.motion_available && s.motion_h == -2048 && s.motion_v == -1);
    return 0;
}
