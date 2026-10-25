/* SPDX-License-Identifier: GPL-3.0-only
 * ESP32 diagnostic starting point. Protocol references: stancecoke /
 * BionX_Minimal_Translator and Ali Saghiran / BionxConsole (2018).
 * No motor actuation or protected register writes in this first version.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_err.h"
#include "board_config.h"
#include "bionx_diagnostic.h"
#include "sdkconfig.h"
#include "driver/twai.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bionx_protocol.h"

static esp_err_t request(uint16_t node, uint8_t reg) {
    twai_message_t m = {.identifier=node, .data_length_code=2};
    m.data[1]=reg;
    return twai_transmit(&m, pdMS_TO_TICKS(5));
}
void bionx_diagnostic_task(void *argument) {
    (void)argument;
#ifdef CONFIG_BIONX_ACTIVE
    const bool active=true;
#else
    const bool active=false;
#endif
    twai_general_config_t g=TWAI_GENERAL_CONFIG_DEFAULT(
        BOARD_CAN_TX, BOARD_CAN_RX,
        active ? TWAI_MODE_NORMAL : TWAI_MODE_LISTEN_ONLY);
    g.rx_queue_len=64;
    twai_timing_config_t t=TWAI_TIMING_CONFIG_125KBITS();
    twai_filter_config_t f=TWAI_FILTER_CONFIG_ACCEPT_ALL();
    ESP_ERROR_CHECK(twai_driver_install(&g,&t,&f));
    ESP_ERROR_CHECK(twai_start());
    /* One outstanding read: console responses share ID 0x08.
       A matching response is only a candidate: no transaction ID exists.
       Do not use active mode alongside another console/master. */
    /* Read-only identification and independent telemetry bytes. Never poll
       motor 0x12 here: its revolution count resets on read. HI/LO values
       remain separate candidates until coherent pairing is implemented. */
    const uint16_t nodes[]={0x20,0x20,0x20,0x20,0x20,0x10,0x10,0x10,0x10,0x10,0x10,0x10};
    const uint8_t regs[]={0x20,0x11,0x14,0x16,0x21,0x3B,0x3C,0x30,0x61,0xBC,0xA6,0xA7};
    size_t next=0;
    int64_t deadline=0;
    int64_t sent_us=0;
    bool waiting=false, fault=false;
    printf("BionX diagnostic; active=%d; motor writes disabled\n",active);
    for (;;) {
        twai_status_info_t status;
        if(twai_get_status_info(&status)!=ESP_OK || status.state!=TWAI_STATE_RUNNING) {
            if(!fault) printf("CAN fault: restart manually after wiring check\n");
            fault=true;
        }
        int64_t now=esp_timer_get_time();
        if(active && !fault && now>=deadline) {
            if(waiting) printf("TIMEOUT node=%02X reg=%02X\n", nodes[next],regs[next]);
            if(waiting) next=(next+1)%(sizeof(regs)/sizeof(regs[0]));
            waiting=false;
            if(request(nodes[next],regs[next])==ESP_OK) { waiting=true; sent_us=esp_timer_get_time(); }
            else { printf("TX failed; diagnostics latched off\n"); fault=true; }
            deadline=(waiting ? sent_us : now)+200000;
        }
        twai_message_t m;
        if(twai_receive(&m,pdMS_TO_TICKS(5))==ESP_OK) {
            const int64_t received_us = esp_timer_get_time();
            printf("%lld ID=%03lX %s%s DLC=%u",(long long)received_us,
                (unsigned long)m.identifier,m.extd?"EXT ":"",m.rtr?"RTR ":"",m.data_length_code);
            if(!m.rtr) for(unsigned i=0;i<m.data_length_code && i<8;i++) printf(" %02X",m.data[i]);
            can_frame_t frame = {.id=m.identifier, .dlc=m.data_length_code,
                .extended=m.extd, .rtr=m.rtr, .timestamp_us=received_us};
            memcpy(frame.data, m.data, sizeof(frame.data));
            /* A late reply is never accepted outside its request window. */
            if(waiting && !fault && bionx_reply_candidate(&frame, regs[next], sent_us, deadline)) {
                printf(" CANDIDATE node=%02X reg=%02X raw=%02X%02X", nodes[next],regs[next],m.data[2],m.data[3]);
                bionx_packet_t packet;
                float value;
                const char *unit;
                const bionx_register_info_t *info = bionx_register_info(nodes[next], regs[next]);
                if (info) printf(" %s", info->name);
                if (bionx_parse(&frame, &packet) &&
                    bionx_value_candidate(&packet, nodes[next], &value, &unit))
                    printf(" candidate=%.3f%s", (double)value, unit);
                waiting=false;
                next=(next+1)%(sizeof(regs)/sizeof(regs[0]));
            }
            printf("\n");
        }
    }
}
