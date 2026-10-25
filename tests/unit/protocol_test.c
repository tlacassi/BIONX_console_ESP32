/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stddef.h>
#include "bionx_protocol.h"

int main(void)
{
    can_frame_t f = {.id = 8, .dlc = 4, .data = {0, 0x11, 0xff, 0x80}, .timestamp_us = 100};
    bionx_packet_t p;
    assert(bionx_parse(&f, &p) && p.kind == BIONX_REPLY && p.target == 0 && p.raw == 0xff80);
    assert(bionx_reply_candidate(&f, 0x11, 50, 101));
    assert(!bionx_reply_candidate(&f, 0x11, 50, 100));
    assert(!bionx_reply_candidate(&f, 0x11, 101, 200));
    assert(!bionx_reply_candidate(&f, 0x12, 50, 200));
    for (int dlc = 0; dlc <= 15; ++dlc) {
        f.dlc = dlc;
        assert(bionx_parse(&f, &p) == (dlc == 4));
    }
    f.dlc = 4;
    f.rtr = true; assert(!bionx_parse(&f, &p)); f.rtr = false;
    f.extended = true; assert(!bionx_parse(&f, &p)); f.extended = false;
    f.id = 9; assert(!bionx_parse(&f, &p));
    f.id = 0x20; assert(bionx_parse(&f, &p) && p.kind == BIONX_SET && p.target == 0x20);
    f.id = 0x10; f.dlc = 2; assert(bionx_parse(&f, &p) && p.kind == BIONX_QUERY);
    f.data[0] = 1; assert(!bionx_parse(&f, &p));
    f.id = 0x800; assert(!can_frame_valid(&f));
    f.extended = true; f.id = 0x1fffffff; assert(can_frame_valid(&f));
    f.id++; assert(!can_frame_valid(&f));
    assert(!can_frame_valid(NULL) && !bionx_parse(NULL, &p));
    /* Diagnostic traffic must not satisfy normal-console transactions. */
    f = (can_frame_t){.id=0x58, .dlc=4, .data={0,0x11,0,1}, .timestamp_us=100};
    assert(bionx_parse(&f, &p) && p.kind == BIONX_REPLY && p.target == 0);
    assert(!bionx_reply_candidate(&f, 0x11, 50, 101));
    f.dlc=2; f.data[1]=0x9c;
    assert(bionx_parse(&f, &p) && p.kind == BIONX_UNKNOWN && p.target == 0);
    assert(!bionx_register_info(p.target, p.reg));
    const uint8_t targets[]={0x48,0x50,0x60};
    for (size_t i=0;i<sizeof(targets);++i) {
        f.id=targets[i];
        assert(bionx_parse(&f, &p) && p.kind == BIONX_QUERY && p.target == targets[i]);
        f.dlc=4;
        assert(bionx_parse(&f, &p) && p.kind == BIONX_SET);
        f.dlc=2;
    }
    float value=123;
    const char *unit=NULL;
    p=(bionx_packet_t){.kind=BIONX_SET,.reg=9,.raw=0xc0,.target=0x20};
    assert(bionx_value_candidate(&p,0x20,&value,&unit) && value == -100 && unit[0]=='%');
    p.raw=0x40;
    assert(bionx_value_candidate(&p,0x20,&value,&unit) && value == 100);
    p.raw=0x80; assert(!bionx_value_candidate(&p,0x20,&value,&unit));
    p.raw=0xffc0; /* Not a protocol byte; do not silently truncate. */
    assert(!bionx_value_candidate(&p,0x20,&value,&unit));
    p=(bionx_packet_t){.kind=BIONX_REPLY,.reg=0x61,.raw=15};
    assert(bionx_value_candidate(&p,0x50,&value,&unit) && value > 99.99f && value < 100.01f);
    assert(!bionx_value_candidate(&p,0,&value,&unit));
    p.raw=255; assert(!bionx_value_candidate(&p,0x10,&value,&unit));
    p.reg=0xa6; p.raw=0xc3; /* One half is not a voltage. */
    assert(!bionx_value_candidate(&p,0x10,&value,&unit));
    p.reg=0x11; p.raw=22;
    assert(bionx_value_candidate(&p,0x20,&value,&unit) && value > 199.9f && value < 200.1f);
    p.kind=BIONX_QUERY; assert(!bionx_value_candidate(&p,0x20,&value,&unit));
    p.kind=BIONX_SET; p.target=0x10;
    assert(!bionx_value_candidate(&p,0x20,&value,&unit));
    assert(!bionx_value_candidate(NULL,0x20,&value,&unit));
    return 0;
}
