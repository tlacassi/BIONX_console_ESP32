/* SPDX-License-Identifier: GPL-3.0-only
 * Original implementation from Microchip MCP2515 datasheet DS20001801J.
 */
#include <string.h>
#include "mcp2515_core.h"

enum { CANSTAT = 0x0e, CANCTRL = 0x0f, TEC = 0x1c, CNF3 = 0x28,
    CANINTE = 0x2b, CANINTF = 0x2c, EFLG = 0x2d, RXB0CTRL = 0x60, RXB1CTRL = 0x70 };

static esp_err_t transfer(mcp2515_t *d, const uint8_t *tx, uint8_t *rx, size_t n)
{
    esp_err_t err = d->io.transfer(d->io.ctx, tx, rx, n);
    if (err != ESP_OK) {
        ++d->stats.spi_errors;
        d->stats.ready = false;
    }
    return err;
}

static esp_err_t read_regs(mcp2515_t *d, uint8_t addr, uint8_t *out, size_t n)
{
    if (n > 14) return ESP_ERR_INVALID_ARG;
    uint8_t tx[16] = {0x03, addr}, rx[16] = {0};
    esp_err_t err = transfer(d, tx, rx, n + 2);
    if (err == ESP_OK) memcpy(out, rx + 2, n);
    return err;
}

static esp_err_t write_reg(mcp2515_t *d, uint8_t addr, uint8_t value)
{
    const uint8_t tx[] = {0x02, addr, value};
    return transfer(d, tx, NULL, sizeof(tx));
}

static esp_err_t modify(mcp2515_t *d, uint8_t addr, uint8_t mask, uint8_t value)
{
    const uint8_t tx[] = {0x05, addr, mask, value};
    return transfer(d, tx, NULL, sizeof(tx));
}

static esp_err_t expect_mode(mcp2515_t *d, uint8_t mode)
{
    for (unsigned i = 0; i < 10; ++i) {
        uint8_t status;
        esp_err_t err = read_regs(d, CANSTAT, &status, 1);
        if (err != ESP_OK) return err;
        d->stats.mode = status & 0xe0;
        if (d->stats.mode == mode) return ESP_OK;
        d->io.delay_ms(d->io.ctx, 1);
    }
    ++d->stats.mode_faults;
    d->stats.ready = false;
    return ESP_ERR_TIMEOUT;
}

static esp_err_t init(mcp2515_t *d, const mcp2515_io_t *io, unsigned osc, bool active)
{
    if (!d || !io || !io->transfer || !io->delay_ms || !io->now_us) return ESP_ERR_INVALID_ARG;
    memset(d, 0, sizeof(*d));
    d->io = *io;
    d->active = active;
    if (osc != 8 && osc != 16 && osc != 20) return ESP_ERR_INVALID_ARG;
    const uint8_t reset = 0xc0;
    esp_err_t err = transfer(d, &reset, NULL, 1);
    if (err != ESP_OK) return err;
    d->io.delay_ms(d->io.ctx, 10);
    err = expect_mode(d, 0x80); /* Reset enters configuration, never normal. */
    if (err != ESP_OK) return err;
    /* 125 kbit/s, 16 TQ, sample point 75%, SJW=1:
     * TQ=0.5 us; Sync=1, Prop=5, PS1=6, PS2=4.
     * BRP=(Fosc/(2*125000*16))-1 => 1,3,4 for 8/16/20 MHz. */
    const uint8_t cnf[] = {0x03, 0xac, (uint8_t)(osc / 4 - 1)};
    for (unsigned i = 0; i < 3; ++i) {
        err = write_reg(d, CNF3 + i, cnf[i]);
        if (err != ESP_OK) return err;
    }
    uint8_t actual[3];
    err = read_regs(d, CNF3, actual, 3);
    if (err != ESP_OK) return err;
    if (memcmp(cnf, actual, 3) != 0) return ESP_ERR_INVALID_RESPONSE;
    /* Reset masks/filters are zero: accept all identifiers; RXB0 rolls to RXB1.
     * Listen-only can also capture damaged bus frames (datasheet section 10.3).
     * Structural validation below is not a CAN CRC integrity guarantee. */
    err = write_reg(d, RXB0CTRL, 0x04);
    if (err != ESP_OK) return err;
    err = write_reg(d, RXB1CTRL, 0);
    if (err != ESP_OK) return err;
    err = write_reg(d, CANINTF, 0);
    if (err != ESP_OK) return err;
    err = write_reg(d, CANINTE, 0x23); /* RX0, RX1 and error interrupts. */
    if (err != ESP_OK) return err;
    err = modify(d, CANCTRL, 0xe8, active ? 0x08 : 0x60);
    if (err != ESP_OK) return err;
    err = expect_mode(d, active ? 0 : 0x60);
    if (err == ESP_OK) d->stats.ready = true;
    return err;
}

static esp_err_t check_health(mcp2515_t *d, int64_t now)
{
    uint8_t mode, health[2];
    esp_err_t err=read_regs(d,CANSTAT,&mode,1);
    if (err!=ESP_OK) return err;
    d->stats.mode=mode&0xe0;
    if (d->stats.mode!=(d->active ? 0 : 0x60)) {
        ++d->stats.mode_faults;
        d->stats.ready=false;
        return ESP_ERR_INVALID_STATE;
    }
    err=read_regs(d,TEC,health,2);
    if (err!=ESP_OK) return err;
    d->stats.tec=health[0]; d->stats.rec=health[1];
    d->next_health_us=now+10000;
    return ESP_OK;
}

static esp_err_t poll_rx(mcp2515_t *d, can_frame_t *frame, bool fast)
{
    if (!d || !frame) return ESP_ERR_INVALID_ARG;
    if (!d->stats.ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err;
    uint8_t flags[2];
    const int64_t now=d->io.now_us(d->io.ctx);
    const bool deferred_health=fast && !d->active && now>=d->next_health_us;
    if (!fast || d->active) {
        err=check_health(d,now);
        if (err!=ESP_OK) return err;
    }
    err = read_regs(d, CANINTF, flags, 2);
    if (err != ESP_OK) return err;
    d->stats.eflg = flags[1];
    if (d->active && (flags[1] & (d->discovery ? 0x28 : 0x38))) {
        if (flags[1] & 0x20) ++d->stats.bus_off;
        d->stats.ready = false;
        return ESP_ERR_INVALID_STATE;
    }
    if (flags[1] & 0xc0) {
        if (flags[1] & 0x40) ++d->stats.overflow0_events;
        if (flags[1] & 0x80) ++d->stats.overflow1_events;
        err = modify(d, EFLG, 0xc0, 0);
        if (err != ESP_OK) return err;
    }
    if (flags[0] & 0x20) {
        err = modify(d, CANINTF, 0x20, 0);
        if (err != ESP_OK) return err;
    }
    if (!(flags[0] & 3)) {
        if (deferred_health) {
            err=check_health(d,now);
            if (err!=ESP_OK) return err;
        }
        return ESP_ERR_NOT_FOUND;
    }
    const bool buffer1 = !(flags[0] & 1);
    uint8_t raw[14];
    if (fast && !d->active) {
        /* READ RX BUFFER starts at SIDH and clears RXnIF when CS rises.
         * Reconstruct RXRTR from SIDL (standard) or DLC (extended).
         * Datasheet DS20001801J, sections 4 and 12.4. */
        uint8_t tx[14] = {buffer1 ? 0x94 : 0x90}, rx[14] = {0};
        err = transfer(d,tx,rx,sizeof(tx));
        if (err != ESP_OK) return err;
        memcpy(raw+1,rx+1,13);
        raw[0] = ((raw[2]&8) ? (raw[5]&0x40) : (raw[2]&0x10)) ? 8 : 0;
    } else {
        err = read_regs(d, buffer1 ? RXB1CTRL : RXB0CTRL, raw, sizeof(raw));
        if (err != ESP_OK) return err;
        err = modify(d, CANINTF, buffer1 ? 2 : 1, 0);
        if (err != ESP_OK) return err;
    }
    const int64_t read_us = d->io.now_us(d->io.ctx);
    /* Passive diagnostics must not delay freeing a full hardware RX buffer.
     * Still run the due check before delivering the consumed frame. */
    if (deferred_health) {
        err=check_health(d,d->io.now_us(d->io.ctx));
        if (err!=ESP_OK) return err;
    }
    *frame = (can_frame_t){ .extended = (raw[2] & 8) != 0,
        .rtr = (raw[0] & 8) != 0, .dlc = raw[5] & 15, .timestamp_us = read_us };
    frame->id = ((uint32_t)raw[1] << 3) | (raw[2] >> 5);
    if (frame->extended) frame->id = (frame->id << 18) |
        ((uint32_t)(raw[2] & 3) << 16) | ((uint32_t)raw[3] << 8) | raw[4];
    if (!can_frame_valid(frame)) {
        ++d->stats.malformed;
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (!frame->rtr) memcpy(frame->data, raw + 6, frame->dlc);
    ++d->stats.received;
    return ESP_OK;
}

esp_err_t mcp2515_poll(mcp2515_t *d, can_frame_t *frame)
{ return poll_rx(d, frame, false); }

esp_err_t mcp2515_poll_fast(mcp2515_t *d, can_frame_t *frame)
{ return poll_rx(d, frame, true); }

esp_err_t mcp2515_init_passive(mcp2515_t *d, const mcp2515_io_t *io, unsigned osc)
{ return init(d, io, osc, false); }
esp_err_t mcp2515_init_active(mcp2515_t *d, const mcp2515_io_t *io, unsigned osc)
{ return init(d, io, osc, true); }

esp_err_t mcp2515_tx_abort(mcp2515_t *d)
{
    if (!d || !d->active) return ESP_ERR_INVALID_STATE;
    /* Best effort even after a latched SPI/bus fault. */
    esp_err_t err = modify(d, 0x30, 8, 0);
    d->tx_pending = false;
    d->stats.ready = false;
    return err;
}

esp_err_t mcp2515_tx_begin(mcp2515_t *d, const can_frame_t *f)
{
    if (!d || !can_frame_valid(f) || f->extended || f->rtr) return ESP_ERR_INVALID_ARG;
    if (!d->active || !d->stats.ready || d->tx_pending) return ESP_ERR_INVALID_STATE;
    uint8_t ctrl;
    esp_err_t err = read_regs(d, CANCTRL, &ctrl, 1);
    if (err != ESP_OK) return err;
    if ((ctrl & 0xe8) != 8) { d->stats.ready=false; return ESP_ERR_INVALID_STATE; }
    err = read_regs(d, 0x30, &ctrl, 1);
    if (err != ESP_OK) return err;
    if (ctrl & 8) return ESP_ERR_INVALID_STATE;
    err = modify(d, CANINTF, 4, 0);
    if (err != ESP_OK) return err;
    /* Standard frame SIDH/SIDL/EID8/EID0/DLC, then data. */
    uint8_t tx[15] = {0x02, 0x31, (uint8_t)(f->id >> 3), (uint8_t)(f->id << 5), 0, 0, f->dlc};
    memcpy(tx + 7, f->data, f->dlc);
    err = transfer(d, tx, NULL, 7 + f->dlc);
    if (err != ESP_OK) return err;
    err = modify(d, 0x30, 8, 8);
    if (err == ESP_OK) d->tx_pending = true;
    return err;
}

esp_err_t mcp2515_probe_cancel(mcp2515_t *d)
{
    if (!d || !d->active || !d->discovery || !d->stats.ready) return ESP_ERR_INVALID_STATE;
    esp_err_t err=modify(d,0x30,8,0);
    if (err!=ESP_OK) return err;
    d->tx_pending=false;
    return ESP_OK;
}

esp_err_t mcp2515_tx_status(mcp2515_t *d)
{
    if (!d || !d->active || !d->stats.ready || !d->tx_pending) return ESP_ERR_INVALID_STATE;
    uint8_t ctrl, flags[2], mode;
    esp_err_t err = read_regs(d, CANSTAT, &mode, 1);
    if (err != ESP_OK) return err;
    err = read_regs(d, 0x30, &ctrl, 1);
    if (err != ESP_OK) return err;
    err = read_regs(d, CANINTF, flags, 2);
    if (err != ESP_OK) return err;
    d->stats.eflg = flags[1];
    d->stats.last_tx_ctrl=ctrl; d->stats.last_tx_intf=flags[0];
    d->stats.mode=mode&0xe0;
    if (d->stats.mode) ++d->stats.mode_faults;
    if ((mode&0xe0) || (ctrl&0x70) || (flags[1]&0x38)) {
        /* Freeze current counters before abort; prior RX diagnostics may have
         * preceded the failed transmission and still showed TEC/REC=0. */
        uint8_t health[2];
        err=read_regs(d,TEC,health,2);
        if(err!=ESP_OK) return err;
        d->stats.tec=health[0]; d->stats.rec=health[1];
    }
    if (d->discovery && !(mode & 0xe0) && !(flags[1]&0x28) &&
        !(ctrl&8) && ((ctrl&0x70) || !(flags[0]&4))) {
        ++d->stats.tx_errors;
        err=mcp2515_probe_cancel(d);
        return err==ESP_OK ? ESP_ERR_INVALID_RESPONSE : err;
    }
    if ((mode & 0xe0) || (ctrl & 0x70) || (flags[1] & (d->discovery ? 0x28 : 0x38))) {
        ++d->stats.tx_errors;
        if (flags[1] & 0x20) ++d->stats.bus_off;
        mcp2515_tx_abort(d);
        return ESP_FAIL;
    }
    if (ctrl & 8) return ESP_ERR_NOT_FOUND;
    if (!(flags[0] & 4)) {
        ++d->stats.tx_errors;
        mcp2515_tx_abort(d);
        return ESP_FAIL;
    }
    err = modify(d, CANINTF, 4, 0);
    if (err != ESP_OK) return err;
    d->tx_pending = false;
    ++d->stats.transmitted;
    return ESP_OK;
}
