/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MIDI IN on the TRS jack (TONITOR 0.51, as BIGORNA's build 47; from MELODEE 1.4.5's midi_uart.c, its RX verified on an FM-1): PH8 -> input
 * channel 1 -> UART1 RX, 31250 baud, RX DMA into a 128-byte ring, polled from the TIMER5 ISR at 2 kHz (no UART IRQ),
 * nested in the audio too, so a clock's timing does not wait on the render. Channel messages go into the MIDI ring
 * next to USB's, as USB-MIDI packets on cable 1 (USB's are cable 0), so midi_take (setup.c) routes them by channel as
 * USB's, and PORTS > TRS IN can leave them out. Realtime (TONITOR: no clock), system common and SysEx are ignored
 * (no SysEx -> UBOOT from the jack).
 *
 * The ring is read by content, not by the DMA count (MELODEE's finding): every slot the parser has not been given
 * holds UM_EMPTY, and the reader takes bytes until it meets one -- the count once missed a byte on an FM-1 and the
 * reader stayed a byte behind until reboot. The count still tells an overrun (more than the ring since the last
 * poll). */
#include "../hal/fm1_uart.h"   /* registers; relative, so the host tests find it too */

#define UM_RING 128u
#define UM_EMPTY 0xFDu         /* an undefined MIDI realtime byte: never sent, it marks unread slots */
static volatile uint8_t um_ring[UM_RING] __attribute__((aligned(16)));
static struct {
    uint32_t rd;
    volatile uint32_t bytes, drops;
    uint8_t st, need, got, d0, sysex;
} um;

static void uart_midi_init(void)                   /* (before TIMER5 starts: PORTH read-modify-write) */
{
    uint32_t i;
    for (i = 0; i < UM_RING; i++)
        um_ring[i] = UM_EMPTY;
    fm1_uart1_midi_init(um_ring, UM_RING);
}

static void um_put(uint32_t pkt)                   /* (a channel message: the MIDI ring, cable 1) */
{
    if (mi_w - mi_r < MQ) {
        midi_in_q[mi_w % MQ] = pkt | 0x10u;
        RING_PUBLISH();
        mi_w++;
    } else
        um.drops++;
}
static uint32_t um_len(uint32_t s) { return (s & 0xE0u) == 0xC0u ? 1u : 2u; }   /* (Cx, Dx: 1 data byte) */
static void um_byte(uint32_t b)
{
    if (b >= 0xF8u)                                /* realtime: ignored, running status kept */
        return;
    if (b & 0x80u) {
        um.sysex = b == 0xF0u;
        um.st = b < 0xF0u ? (uint8_t)b : 0;        /* (system common, SysEx: running status cancelled) */
        um.need = (uint8_t)um_len(b);
        um.got = 0;
        return;
    }
    if (um.sysex || !um.st)
        return;
    if (um.need == 2u && !um.got) {
        um.d0 = (uint8_t)b;
        um.got = 1;
        return;
    }
    {
        uint32_t d1 = um.need == 2u ? um.d0 : b, d2 = um.need == 2u ? b : 0u;
        um.got = 0;
        um_put((um.st >> 4) | (uint32_t)um.st << 8 | d1 << 16 | d2 << 24);
    }
}
/* the bytes the DMA has written since the last call, by content; n is its count, which only tells an overrun (the
 * DMA went round the ring past unread bytes: the oldest UM_RING are read, the rest counted as drops). A received
 * UM_EMPTY (line noise) is passed over once the byte after it has landed */
static void uart_midi_take(uint32_t n)
{
    uint32_t k;
    if (n > UM_RING) {
        um.drops += n - UM_RING;
        um.rd = (um.rd + n - UM_RING) & (UM_RING - 1u);
        um.st = um.got = um.sysex = 0;             /* (bytes lost: wait for a whole new status) */
    }
    for (k = 0; k < UM_RING; k++) {
        uint32_t b = um_ring[um.rd];
        if (b == UM_EMPTY) {
            if (um_ring[(um.rd + 1u) & (UM_RING - 1u)] == UM_EMPTY)
                break;
            b = um_ring[um.rd];                    /* (the DMA writes in order: this one has landed too) */
        }
        um_ring[um.rd] = UM_EMPTY;
        um.rd = (um.rd + 1u) & (UM_RING - 1u);
        um.bytes++;
        um_byte(b);                                /* (a UM_EMPTY: an unknown realtime byte, ignored) */
    }
}
static void uart_midi_poll(void) { uart_midi_take(fm1_uart1_rx_take()); }   /* (TIMER5, 2 kHz) */
