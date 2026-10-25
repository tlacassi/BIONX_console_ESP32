/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <string.h>
#include "mcp2515_core.h"
#include "mcp2515_diagnostic.h"

typedef struct {
    uint8_t regs[256];
    unsigned transfers;
    int64_t now_us;
    unsigned fail_at;
    bool absent, ignore_mode, active_allowed, require_rx_ack_before_health;
    unsigned tx_result;
    uint8_t corrupt_cnf;
} model_t;

static esp_err_t model_spi(void *ctx, const uint8_t *tx, uint8_t *rx, size_t n)
{
    model_t *m = ctx;
    if (++m->transfers == m->fail_at) return ESP_FAIL;
    if (rx) memset(rx, m->absent ? 0xff : 0, n);
    if (m->absent) return ESP_OK;
    switch (tx[0]) {
    case 0xc0:
        assert(n == 1);
        memset(m->regs, 0, sizeof(m->regs));
        m->regs[0x0e] = m->regs[0x0f] = 0x80;
        break;
    case 0x03:
        assert(rx && n >= 3);
        if (m->require_rx_ack_before_health && (tx[1]==0x0e || tx[1]==0x1c))
            assert(!(m->regs[0x2c]&1)); /* Free RX0 before doing diagnostics. */
        for (size_t i = 2; i < n; ++i) rx[i] = m->regs[(uint8_t)(tx[1] + i - 2)];
        if (tx[1] == 0x28) rx[2] ^= m->corrupt_cnf;
        break;
    case 0x90:
    case 0x94: {
        assert(rx && n==14);
        unsigned buffer=tx[0]==0x94;
        unsigned base=buffer ? 0x71 : 0x61;
        memcpy(rx+1,m->regs+base,13);
        m->regs[0x2c]&=~(1U<<buffer);
        break;
    }
    case 0x02:
        if (tx[1]==0x31) {
            assert(m->active_allowed && n>=7 && n<=15);
            for (size_t i=2;i<n;++i) m->regs[tx[1]+i-2]=tx[i];
            break;
        }
        assert(n == 3);
        /* Only configuration/interrupt/RX registers; no TX buffer write. */
        assert((tx[1] >= 0x28 && tx[1] <= 0x2c) || tx[1] == 0x60 || tx[1] == 0x70);
        m->regs[tx[1]] = tx[2];
        break;
    case 0x05:
        assert(n == 4);
        assert(tx[1] == 0x0f || tx[1] == 0x2c || tx[1] == 0x2d || (m->active_allowed && tx[1]==0x30));
        m->regs[tx[1]] = (m->regs[tx[1]] & ~tx[2]) | (tx[3] & tx[2]);
        if (tx[1] == 0x0f) {
            assert(m->active_allowed || (tx[3] & 0xe0) == 0x60); /* Never normal, sleep or loopback. */
            if (!m->ignore_mode) m->regs[0x0e] = m->regs[0x0f] & 0xe0;
        }
        if (tx[1]==0x30 && (tx[3]&8) && m->tx_result) {
            m->regs[0x30]=m->tx_result==2 ? 0x50 : m->tx_result==3 ? 0x60 : 0;
            if (m->tx_result==1) m->regs[0x2c]|=4;
        }
        break;
    default: assert(!"Unexpected SPI instruction, especially any RTS or TX load");
    }
    return ESP_OK;
}
static void delay(void *ctx, unsigned ms) { (void)ctx; (void)ms; }
static int64_t now(void *ctx) { model_t *m=ctx; return 123456+m->now_us; }

static void inject(model_t *m, unsigned buffer, uint32_t id, bool ext, bool rtr, unsigned dlc)
{
    unsigned base = buffer ? 0x70 : 0x60;
    m->regs[0x2c] |= 1 << buffer;
    m->regs[base] = rtr ? 8 : 0;
    uint32_t sid = ext ? id >> 18 : id;
    m->regs[base + 1] = sid >> 3;
    m->regs[base + 2] = ((sid & 7) << 5) | (ext ? 8 | ((id >> 16) & 3) : rtr ? 0x10 : 0);
    m->regs[base + 3] = ext ? id >> 8 : 0;
    m->regs[base + 4] = ext ? id : 0;
    m->regs[base + 5] = dlc | ((ext && rtr) ? 0x40 : 0);
    for (unsigned i = 0; i < 8; ++i) m->regs[base + 6 + i] = 0xa0 + i;
}

int main(void)
{
    model_t m = {0};
    mcp2515_t dev;
    const mcp2515_io_t io = {.ctx = &m, .transfer = model_spi, .delay_ms = delay, .now_us = now};
    can_frame_t f;
    assert(mcp2515_init_passive(&dev, &io, 0) == ESP_ERR_INVALID_ARG && m.transfers == 0);
    assert(mcp2515_init_passive(&dev, &io, 12) == ESP_ERR_INVALID_ARG);
    for (unsigned osc = 8; osc <= 20; osc += osc == 8 ? 8 : 4) {
        assert(mcp2515_init_passive(&dev, &io, osc) == ESP_OK);
        /* Independently calculate bitrate and sample point from registers. */
        unsigned tq = 1 + (m.regs[0x29] & 7) + 1 + ((m.regs[0x29] >> 3) & 7) + 1 + (m.regs[0x28] & 7) + 1;
        assert(osc * 1000000U / (2 * ((m.regs[0x2a] & 63) + 1) * tq) == 125000);
        assert((tq - ((m.regs[0x28] & 7) + 1)) * 100 / tq == 75);
    }
    assert(mcp2515_poll(&dev, &f) == ESP_ERR_NOT_FOUND);
    inject(&m, 0, 0x7ff, false, false, 8);
    inject(&m, 1, 0x1fffffff, true, true, 8);
    assert(mcp2515_poll(&dev, &f) == ESP_OK && f.id == 0x7ff && !f.extended && !f.rtr);
    assert(f.data[7] == 0xa7 && f.timestamp_us == 123456 && m.regs[0x2c] == 2);
    assert(mcp2515_poll(&dev, &f) == ESP_OK && f.id == 0x1fffffff && f.extended && f.rtr);
    assert(f.dlc == 8 && f.data[0] == 0 && m.regs[0x2c] == 0);
    inject(&m, 1, 0x1234567, true, false, 4);
    assert(mcp2515_poll(&dev, &f) == ESP_OK && f.id == 0x1234567 && f.data[3] == 0xa3);
    inject(&m, 0, 0, false, true, 0);
    assert(mcp2515_poll(&dev, &f) == ESP_OK && f.id == 0 && !f.extended && f.rtr && f.dlc == 0);
    for (unsigned dlc = 9; dlc <= 15; ++dlc) {
        inject(&m, 0, 8, false, false, dlc);
        assert(mcp2515_poll(&dev, &f) == ESP_ERR_INVALID_RESPONSE);
    }
    assert(dev.stats.malformed == 7);
    m.regs[0x2d] = 0xc0; m.regs[0x2c] = 0x20;
    assert(mcp2515_poll(&dev, &f) == ESP_ERR_NOT_FOUND);
    assert(dev.stats.overflow0_events == 1 && dev.stats.overflow1_events == 1);
    assert(m.regs[0x2c] == 0 && m.regs[0x2d] == 0);
    m.regs[0x0e] = 0x80;
    assert(mcp2515_poll(&dev, &f) == ESP_ERR_INVALID_STATE && !dev.stats.ready);
    assert(mcp2515_poll(&dev, &f) == ESP_ERR_INVALID_STATE);

    m = (model_t){.absent = true};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_ERR_TIMEOUT);
    assert(m.transfers == 11); /* Reset + 10 bounded mode checks. */
    m = (model_t){.ignore_mode = true};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_ERR_TIMEOUT);
    m = (model_t){.corrupt_cnf = 1};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_ERR_INVALID_RESPONSE);
    /* Fail every initialization transfer in turn; none may leave ready=true. */
    m = (model_t){0};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_OK);
    const unsigned init_count = m.transfers;
    for (unsigned n = 1; n <= init_count; ++n) {
        m = (model_t){.fail_at = n};
        assert(mcp2515_init_passive(&dev, &io, 8) == ESP_FAIL && !dev.stats.ready);
    }
    m = (model_t){0};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_OK);
    m.fail_at = m.transfers + 1;
    assert(mcp2515_poll(&dev, &f) == ESP_FAIL && dev.stats.spi_errors == 1 && !dev.stats.ready);
    m = (model_t){0};
    assert(mcp2515_init_passive(&dev, &io, 8) == ESP_OK);
    inject(&m, 0, 0x20, false, false, 4);
    unsigned before = m.transfers;
    assert(mcp2515_poll(&dev, &f) == ESP_OK);
    const unsigned poll_count = m.transfers - before;
    for (unsigned n = 1; n <= poll_count; ++n) {
        m = (model_t){0};
        assert(mcp2515_init_passive(&dev, &io, 8) == ESP_OK);
        inject(&m, 0, 0x20, false, false, 4);
        m.fail_at = m.transfers + n;
        assert(mcp2515_poll(&dev, &f) == ESP_FAIL && !dev.stats.ready);
    }
    /* Fast passive RX preserves two-buffer drain, errors and bounded health. */
    m=(model_t){0};
    assert(mcp2515_init_passive(&dev,&io,8)==ESP_OK);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_NOT_FOUND);
    inject(&m,0,0x20,false,false,4);
    inject(&m,1,0x08,false,true,2);
    before=m.transfers;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && f.id==0x20);
    assert(m.transfers-before==2); /* flags, READ RX BUFFER + implicit acknowledge */
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && f.id==8 && f.rtr);
    inject(&m,1,0x1abcde,true,true,8);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && f.extended && f.rtr && f.id==0x1abcde);
    inject(&m,0,0x123456,true,false,4);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && !f.rtr && f.data[3]==0xa3);
    inject(&m,0,0x20,false,false,15);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_RESPONSE && !(m.regs[0x2c]&1));
    m.regs[0x2d]=0xc0;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_NOT_FOUND);
    assert(dev.stats.overflow0_events==1 && dev.stats.overflow1_events==1);
    m.regs[0x0e]=0x80; m.now_us+=10000;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_STATE && !dev.stats.ready);
    m=(model_t){0};
    assert(mcp2515_init_passive(&dev,&io,8)==ESP_OK);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_NOT_FOUND);
    m.fail_at=m.transfers+1;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_FAIL && !dev.stats.ready);
    for (unsigned n=1;n<=2;++n) {
        m=(model_t){0};
        assert(mcp2515_init_passive(&dev,&io,8)==ESP_OK);
        assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_NOT_FOUND);
        inject(&m,0,0x10,false,false,2);
        m.fail_at=m.transfers+n;
        assert(mcp2515_poll_fast(&dev,&f)==ESP_FAIL && !dev.stats.ready);
    }
    /* Both buffers full when health becomes due: RX0 must be released first. */
    m=(model_t){0};
    assert(mcp2515_init_passive(&dev,&io,8)==ESP_OK);
    inject(&m,0,0x20,false,false,4);
    inject(&m,1,0x08,false,false,4);
    m.require_rx_ack_before_health=true;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && f.id==0x20);
    assert(m.regs[0x2c]==2 && dev.stats.mode==0x60);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_OK && f.id==8);
    /* A due mode fault still blocks delivery even after consuming the buffer. */
    m.now_us+=10000; m.regs[0x0e]=0x80;
    inject(&m,0,0x10,false,false,2);
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_STATE);
    assert(!dev.stats.ready && !(m.regs[0x2c]&1));
    for (unsigned n=1;n<=4;++n) {
        m=(model_t){0};
        assert(mcp2515_init_passive(&dev,&io,8)==ESP_OK);
        inject(&m,0,0x10,false,false,2);
        m.fail_at=m.transfers+n; /* flags, buffer, mode, TEC/REC */
        assert(mcp2515_poll_fast(&dev,&f)==ESP_FAIL && !dev.stats.ready);
    }
    /* Explicit active path: one-shot encoding and positive TX confirmation. */
    m=(model_t){.active_allowed=true,.tx_result=1};
    assert(mcp2515_init_active(&dev,&io,16)==ESP_OK);
    assert((m.regs[0x0f]&0xe8)==8);
    f=(can_frame_t){.id=0x20,.dlc=4,.data={0,9,0,0}};
    assert(mcp2515_tx_begin(&dev,&f)==ESP_OK);
    assert(m.regs[0x31]==4 && m.regs[0x32]==0 && m.regs[0x35]==4 && m.regs[0x37]==9);
    assert(mcp2515_tx_status(&dev)==ESP_OK && dev.stats.transmitted==1);
    assert(mcp2515_tx_status(&dev)==ESP_ERR_INVALID_STATE);
    for (unsigned outcome=0;outcome<=4;++outcome) {
        if (outcome==1) continue;
        m=(model_t){.active_allowed=true,.tx_result=outcome};
        assert(mcp2515_init_active(&dev,&io,16)==ESP_OK);
        assert(mcp2515_tx_begin(&dev,&f)==ESP_OK);
        if (!outcome) {
            assert(mcp2515_tx_status(&dev)==ESP_ERR_NOT_FOUND);
            assert(mcp2515_tx_begin(&dev,&f)==ESP_ERR_INVALID_STATE);
            assert(mcp2515_tx_abort(&dev)==ESP_OK && !(m.regs[0x30]&8) && !dev.stats.ready);
        } else assert(mcp2515_tx_status(&dev)==ESP_FAIL && !dev.stats.ready);
    }
    m=(model_t){.active_allowed=true};
    assert(mcp2515_init_active(&dev,&io,16)==ESP_OK);
    m.regs[0x2d]=0x20;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_STATE && dev.stats.bus_off==1);
    m=(model_t){.active_allowed=true};
    assert(mcp2515_init_active(&dev,&io,8)==ESP_OK);
    inject(&m,0,0x20,false,false,4); m.regs[0x0e]=0x80;
    assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_STATE);
    assert(m.regs[0x2c]&1); /* Active mode checks must precede consuming RX. */
    /* Discovery-only completed failures stay usable, no reset, no false TX success. */
    for (unsigned outcome=2;outcome<=4;++outcome) {
        m=(model_t){.active_allowed=true,.tx_result=outcome};
        assert(mcp2515_init_active(&dev,&io,8)==ESP_OK); dev.discovery=true;
        for (unsigned attempt=0;attempt<3;++attempt) {
            assert(mcp2515_tx_begin(&dev,&f)==ESP_OK);
            assert(mcp2515_tx_status(&dev)==ESP_ERR_INVALID_RESPONSE);
            assert(dev.stats.ready && !dev.tx_pending && !dev.stats.transmitted);
        }
        m.regs[0x2d]=0x10; /* TX error-passive can persist while no node ACKs. */
        assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_NOT_FOUND);
        m.tx_result=1;
        assert(mcp2515_tx_begin(&dev,&f)==ESP_OK);
        assert(mcp2515_tx_status(&dev)==ESP_OK && dev.stats.transmitted==1);
        m.regs[0x2d]=0x20;
        assert(mcp2515_poll_fast(&dev,&f)==ESP_ERR_INVALID_STATE && !dev.stats.ready);
    }
    m=(model_t){.active_allowed=true};
    assert(mcp2515_init_active(&dev,&io,8)==ESP_OK); dev.discovery=true;
    assert(mcp2515_tx_begin(&dev,&f)==ESP_OK);
    assert(mcp2515_probe_cancel(&dev)==ESP_OK && dev.stats.ready && !dev.tx_pending);
    dev.discovery=false; assert(mcp2515_probe_cancel(&dev)==ESP_ERR_INVALID_STATE);
    m=(model_t){0};
    assert(mcp2515_init_passive(&dev,&io,16)==ESP_OK);
    assert(mcp2515_tx_begin(&dev,&f)==ESP_ERR_INVALID_STATE);
    mcp2515_stats_t status={.last_tx_ctrl=0x60};
    assert(!strcmp(mcp2515_tx_failure_name(&status,ESP_FAIL),"ARBITRATION"));
    status.last_tx_ctrl=0x50;
    assert(!strcmp(mcp2515_tx_failure_name(&status,ESP_FAIL),"TX BUS ERROR"));
    assert(!strcmp(mcp2515_tx_failure_name(&status,ESP_ERR_TIMEOUT),"TX TIMEOUT"));
    status.spi_errors=1;
    assert(!strcmp(mcp2515_tx_failure_name(&status,ESP_FAIL),"SPI ERROR"));
    status.spi_errors=0;status.eflg=0x20;
    assert(!strcmp(mcp2515_tx_failure_name(&status,ESP_FAIL),"BUS OFF"));
    return 0;
}
