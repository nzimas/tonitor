/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the MIDI settings (GLOBAL SETUP's, setup.c) and what the audio ISR sends: each track's notes on its OUT
 * channel, as they start and end (the keys', the loops', the sequencers', MIDI's), when SEND NOTES is on. The main
 * loop's packets (the CCs sent, setup.c) wait in a ring of their own, which the ISR empties into usb.c's at each
 * block (one producer for usb.c's ring). After BIGORNA builds 46 and 47; no clock, transport or program changes
 * here (Tonitor has no tempo or patterns). */

typedef struct {
    uint8_t in_ch[NLAYER];       /* each track's channel in, 0 off, 1 .. 16 */
    uint8_t auto_ch;             /* the chosen track's, 0 off */
    uint8_t out_ch[NLAYER];      /* each track's channel out, 0 off */
    uint8_t rcv_note, rcv_cc, send_note, send_cc;   /* 0 / 1 */
    uint8_t usb_in, trs_in;      /* the ports listened to */
    uint8_t usb_aud;             /* (GLOBAL SETUP's AUDIO, kept with these) USB audio: 0 STEREO (the mix), 1 TRACKS (six) */
} midi_cfg_t;
#define MIDI_CFG_OLD 19u         /* (the size before USB AUDIO, build 62: such a record still loads) */
#define MIDI_CFG0 {{1, 2, 3, 4, 5, 6}, 16, {1, 2, 3, 4, 5, 6}, 1, 1, 0, 0, 1, 1, 0}
_Static_assert(sizeof(midi_cfg_t) == MIDI_CFG_OLD + 1u, "USB AUDIO after the older settings");
static midi_cfg_t mcfg = MIDI_CFG0;

static uint16_t midi_held[NLAYER][8];            /* (each track's notes sent on, a bit each: one off each) */
static void midi_note_out(uint32_t l, uint32_t note, uint32_t vel, int on)   /* (the audio ISR) */
{
    uint32_t ch = mcfg.out_ch[l], bit = 1u << (note & 15u), i = (note >> 4) & 7u;
    if (!mcfg.send_note || !ch || ch > 16u)
        return;
    ch--;
    if (midi_held[l][i] & bit) {                 /* (sounding: off first, or only) */
        midi_out_event(0x08u | (0x80u | ch) << 8 | (note & 0x7Fu) << 16);
        midi_held[l][i] = (uint16_t)(midi_held[l][i] & ~bit);
    }
    if (on) {
        midi_out_event(0x09u | (0x90u | ch) << 8 | (note & 0x7Fu) << 16 | (vel ? vel : 1u) << 24);
        midi_held[l][i] = (uint16_t)(midi_held[l][i] | bit);
    }
}
static void midi_layer_off(uint32_t l)            /* (every note track l sent on: off) */
{
    uint32_t n;
    for (n = 0; n < 128u; n++)
        if (midi_held[l][n >> 4] & (1u << (n & 15u)))
            midi_note_out(l, n, 0, 0);
}

#define MMQ 32u
static uint32_t mm_q[MMQ];
static volatile uint32_t mm_w, mm_r;
static void midi_main_send(uint32_t pkt)          /* (the main loop) */
{
    if (mm_w - mm_r < MMQ) {
        mm_q[mm_w % MMQ] = pkt;
        RING_PUBLISH();
        mm_w++;
    }
}
static void midi_main_drain(void)                 /* (the audio ISR, each block) */
{
    while (mm_r != mm_w) {
        midi_out_event(mm_q[mm_r % MMQ]);
        RING_PUBLISH();
        mm_r++;
    }
}
