/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: boot, the interrupts, the main loop. The boot order, the TIMER5 and audio ISRs, the crash screen
 * and the update hooks follow SLOOP's / Felucca's main.c and audio.c (Leo Kuroshita, GPL-3.0). */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
extern uint32_t _ft_start[], _ft_end[], _ft_load[];

/* ------------------------------------------------------------ audio --- */
#define HALF_FRAMES 256u         /* an I2S half buffer: 5.8 ms */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define OUT_SHIFT 7              /* Q15 -> 24 bit, a -6 dBFS ceiling */
static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));

static struct {
    volatile uint32_t in_audio;  /* TIMER5 reads it: what may nest in the render */
    uint32_t halves, late, max_us, last_us, cpu_q8;
    uint8_t over;                /* bit k: the half k halves ago was over 90 % */
} dbg;
static volatile uint32_t t5_nested_ticks;

#if TELEMETRY
/* the audio's timing, for a diagnosis: each half near the edge or late is logged (its load, the gap since the
 * half before, what the loops and the events did in it, the voices sounding), the main loop sends the log and
 * the highest load of the last 0.2 s over USB-MIDI (SysEx F0 7D 54 ..), never waiting for the host */
typedef struct { uint8_t pct, late, non, voices, sounding; uint16_t gap_10us; uint32_t ev; uint32_t half; } tele_t;
static tele_t tele_log[16];
static volatile uint32_t tele_w, tele_r, tele_halves;
static volatile uint8_t tele_max, tele_lates;
static volatile uint16_t tele_maxgap;
static uint32_t tele_last_t0;
static void tele_half(uint32_t pct, uint32_t late, uint32_t t0)
{
    uint32_t gap = tele_last_t0 ? (t0 - tele_last_t0) / FM1_TICKS_PER_US / 10u : 0u, k, nv = 0, snd = 0;
    tele_last_t0 = t0;
    tele_halves++;
    if (pct > tele_max)
        tele_max = (uint8_t)pct;
    if (gap > tele_maxgap)
        tele_maxgap = (uint16_t)(gap > 0xFFFFu ? 0xFFFFu : gap);
    if (late && tele_lates < 127u)
        tele_lates++;
    if ((pct >= 88u || late || gap > 650u) && tele_w - tele_r < 16u) {
        tele_t *g = &tele_log[tele_w % 16u];
        for (k = 0; k < NVOICE; k++)
            nv += vox[k].sounding;
        for (k = 0; k < NLAYER; k++)
            snd |= (uint32_t)layer_sounding(k) << k;
        g->pct = (uint8_t)(pct > 255u ? 255u : pct);
        g->late = (uint8_t)late;
        g->non = (uint8_t)(tele_non > 127u ? 127u : tele_non);
        g->voices = (uint8_t)nv;
        g->sounding = (uint8_t)snd;
        g->gap_10us = (uint16_t)(gap > 0xFFFFu ? 0xFFFFu : gap);
        g->ev = tele_ev;
        g->half = tele_halves;
        RING_PUBLISH();
        tele_w++;
    }
    tele_ev = 0;
    tele_non = 0;
}
#endif

void fm1_alnk0_irq(void)         /* via isr_alnk0 (hal/fm1_isr.S) */
{
    uint8_t p;
    uint32_t t0;
    dbg.in_audio = 1;
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), b, i, all, us;
        int32_t *o = &abuf[half * HALF_WORDS];
        for (b = 0; b < HALF_FRAMES; b += CTL) {
            mix_block(o + 2u * b);
            for (i = 0; i < 2u * CTL; i++)
                o[2u * b + i] <<= OUT_SHIFT;
        }
        fm1_audio_ack_half();
        all = fm1_ticks() - t0;
        us = (all - t5_nested_ticks) / FM1_TICKS_PER_US;
        {
            uint32_t pc = all / FM1_TICKS_PER_US * 100u / (HALF_FRAMES * 1000000u / FS);
            dbg.over = (uint8_t)(dbg.over << 1 | (pc > 90u));
            if (pc >= 100u)
                shed_req = 2;                    /* late: something goes, a held note if it must (layers.c) */
            else if ((dbg.over & 3u) == 3u && !shed_req)
                shed_req = 1;                    /* two near the edge in a row: a tail goes */
            load_pct = (uint8_t)(pc > 255u ? 255u : pc);
        }
#if TELEMETRY
        tele_half(load_pct, fm1_audio_free_half() != half, t0);
#endif
        dbg.cpu_q8 = (dbg.cpu_q8 * 15u + (us * 256u) / (HALF_FRAMES * 1000000u / FS)) / 16u;
        if (us > dbg.max_us)
            dbg.max_us = us;
        dbg.last_us = us;
        if (fm1_audio_free_half() != half)
            dbg.late++;
        dbg.halves++;
    }
    dbg.in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
}

/* --------------------------------------- TIMER5: the panel, time, USB --- */
/* 10 kHz, above the audio (it may nest in the render: the scan keeps its pace, the LEDs do not shimmer);
 * USB is polled at 2 kHz, never nested */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed, last, acc;
    uint32_t t0 = fm1_ticks();
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t0 - last;
    last = t0;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        fm1_ms++;
    }
    if (sub % 5u == 0u)
        owed |= 1u;
    if (++sub == 10u)
        sub = 0;
    if (dbg.in_audio) {
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
    owed = 0;
}
extern void isr_timer5(void);

/* -------------------------------------------------------------- faults --- */
static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}
static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, RGB(150, 0, 0));
    draw_text_box(0, 20, 240, &FONT_M, "CRASH", C_WHITE, 1);
    hexs(b, c->vec);
    draw_text_box(0, 80, 240, &FONT_M, b, C_WHITE, 1);
    hexs(b, c->pc);
    draw_text_box(0, 130, 240, &FONT_M, b, C_WHITE, 1);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* ------------------------------------------- the flash and the update --- */
#include "fm1_flash.h"
static uint8_t flash_ok;
static uint8_t recovery_active;
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: the audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
/* a sector erase with the audio silenced in the same interrupts-off window (else the DMA loops stale audio) */
static int fl_erase4k_quiet(uint32_t off, uint32_t *took)
{
    uint32_t f = irq_save(), i;
    int rc;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
    rc = FL_FAR(fl_erase4k_ram)(off, took);
    irq_restore(f);
    return rc;
}

#define OTA_IDENTITY (recovery_active ? "FM-1_000" : TONITOR_ID)
#include "ota.c"
static void recovery_poll(void);
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void)
{
    fm1_wdt_feed();
    if (recovery_active)
        recovery_poll();
}
static int ota_in_area(uint32_t off, uint32_t n) { return FL_IN(off, n, OTA_AREA, OTA_AREA + OTA_AREA_LEN); }
static int ota_erase(uint32_t off)
{
    uint32_t took;
    if (!ota_in_area(off, 0x1000u) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!ota_in_area(off, n))
        return -8;
    return fl_write(off, p, n);
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static void ota_show(uint32_t step, int32_t code)
{
    static const char *const STEP[] = {"", "PACKAGE", "CHECK", "LOADER", "CONFIRM", "RESTART"};
    char b[16];
    if (recovery_active)
        return;
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 60, 240, &FONT_L, "UPDATE", C_WHITE, 1);
    if (step < 9u) {
        draw_text_box(0, 130, 240, &FONT_M, STEP[step < 6u ? step : 0], RGB(255, 214, 0), 1);
        return;
    }
    if (code == 1)
        str_cpy(b, "DRY RUN OK", sizeof b);
    else {
        uint32_t k = 0, v = (uint32_t)-code;
        str_cpy(b, "FAILED -", sizeof b);
        while (b[k])
            k++;
        if (v >= 10u)
            b[k++] = (char)('0' + v / 10u);
        b[k++] = (char)('0' + v % 10u);
        b[k] = 0;
    }
    draw_text_box(0, 130, 240, &FONT_M, b, RGB(255, 214, 0), 1);
    fm1_delay_ms(1500);
}
static void ota_commit(const uint8_t *parm)
{
    bootguard_clear(&bootguard);                     /* an intended reset */
    usb_detach();
    fm1_delay_ms(30);
    fm1_enter_update(parm);
}
#include "recovery.c"

/* the sessions' flash (sessions.c): their area only */
static int ses_flash_read(uint32_t off, void *p, uint32_t n) { return st_read(off, p, n); }
static int ses_flash_erase(uint32_t off)
{
    uint32_t took;
    if (!FL_IN(off, 0x1000u, SES_LO, SES_HI) || (off & 0xFFFu))
        return -8;
    return fl_erase4k_quiet(off, &took);
}
static int ses_flash_prog(uint32_t off, const void *p, uint32_t n)
{
    if (!FL_IN(off, n, SES_LO, SES_HI))
        return -8;
    return fl_write(off, p, n);
}

/* ------------------------------------------------------------ the loop --- */
static void splash(void)
{
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 88, 240, &FONT_M, "TONITOR", C_WHITE, 1);   /* (the large size is wider than the screen) */
    draw_text_box(0, 136, 240, &FONT_S, TONITOR_VERSION, C_GREY, 1);
}

static void enter_uboot(const char *why)
{
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, C_BLACK);
    draw_text_box(0, 104, 240, &FONT_M, why, RGB(80, 140, 255), 1);
    fm1_delay_ms(20);
    usb_detach();
    fm1_delay_ms(30);
    bootguard.pending = 0;                           /* an intended reset: not a failed boot */
    fm1_enter_uboot();
}

#if TELEMETRY
/* the cost of the building blocks, on this chip: ticks for 256 of each (interrupts off), sent with the reports */
static uint32_t bench_ticks[10];
static volatile int32_t bench_sink;
static void bench_blocks(void)
{
    static int16_t ramtab[AN_TSIZE + 1u];
    static svf_t sv;
    static svf_c sc;
    uint32_t f = irq_save(), t, i, ph = 74565u, inc = 10485760u;
    int32_t acc = 0, x = 1000;
    for (i = 0; i <= AN_TSIZE; i++)
        ramtab[i] = AN_WT[i];
    svf_coef(&sc, 1000, IX(0.9));
#define BENCH(k, body) do { t = fm1_ticks(); for (i = 0; i < 256u; i++) { body; } bench_ticks[k] = fm1_ticks() - t; } while (0)
    BENCH(0, acc += (int32_t)i);
    BENCH(1, { ph += inc; acc += an_rd(&AN_WT[(i % 24u) * AN_LEVELS * (AN_TSIZE + 1u)], ph); });            /* a wavetable read, the flash's */
    BENCH(2, { ph += inc; const int16_t *d = ramtab; uint32_t j = ph >> 22; int32_t fr = (int32_t)((ph >> 7) & 0x7FFFu);
               acc += d[j] + (((d[j + 1u] - d[j]) * fr) >> 15); });          /* .. from RAM */
    BENCH(3, { x = svf_run(&sv, &sc, x + (int32_t)i, 1); acc += x; });      /* a filter step */
    BENCH(4, { acc = mulq15(acc + 3, 23456); });                             /* a Q15 multiply */
    BENCH(5, { acc += tanh_q15((int32_t)(i * 300u) - 40000); });             /* the soft ceiling */
    BENCH(6, { ph += inc; acc += sin_ph(ph); });                             /* a sine */
    BENCH(7, { acc += (int32_t)(((int64_t)acc * (x | 1)) >> 15); });         /* a 64-bit product */
    BENCH(8, { ph += inc; acc += an_rd(&AN_WT[3u * AN_LEVELS * (AN_TSIZE + 1u)], ph) + an_rd(&AN_WT[9u * AN_LEVELS * (AN_TSIZE + 1u)], ph * 3u)
               + an_rd(&AN_WT[17u * AN_LEVELS * (AN_TSIZE + 1u)], ph * 5u); });
#undef BENCH
    bench_sink = acc;
    irq_restore(f);
}
#endif

static void fm1_main(void)
{
    int32_t knob = 512 * 16;
    uint32_t healthy_since, t_both = 0;
    {
        uint32_t f = irq_save();
        flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;   /* the expected 1 MiB part */
        irq_restore(f);
    }
    if (flash_ok)
        ota_boot_cleanup();                          /* a staging area left by an update */
    ses_scan(flash_ok);                              /* the sessions in flash */
#if TELEMETRY
    bench_blocks();
#endif
    lcd_init();
    splash();
    fm1_input_init();
    fm1_adc_init();
    layers_init();
    ui_init();
    sounds_init();                                   /* a sound of each engine for every layer (~0.2 s) */
    audio_init();
    usb_start();
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(700);
    lcd_fill(0, 0, 240, 240, C_BLACK);
    ui.force = 1;
    healthy_since = fm1_ms;
    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if ((uint32_t)(m - healthy_since) >= 30000u && bootguard.pending)
            bootguard_clear(&bootguard);             /* 30 s of this boot: a good one */
        {
            int32_t a = fm1_adc_read(FM1_ADC_MASTER);
            if (a >= 0) {
                uint32_t k10;
                knob += (a * 16 - knob) / 8;
                k10 = (uint32_t)(knob / 16);
                master_q12 = (int32_t)((k10 * k10) >> 8);   /* 0 .. ~4096 */
            }
        }
        {   /* OCT- and OCT+ held 5 s: UBOOT, the chip's own updater (a way back whatever happens) */
            uint32_t both = (1u << PANEL_BTN[B_OCTDN]) | (1u << PANEL_BTN[B_OCTUP]);
            if ((fm1_in.buttons & both) != both)
                t_both = fm1_ms;
            else if ((uint32_t)(fm1_ms - t_both) > 5000u)
                enter_uboot("UBOOT");
        }
        ota_service();                               /* the installer's handshake */
        if (usb.ota_req) {
            uint32_t l;
            usb.ota_req = 0;
            for (l = 0; l < NLAYER; l++)
                ev_post(EV_STOP, l, 0, 0);
            if (flash_ok)
                ota_session();                       /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, C_BLACK);
            ui.force = 1;
        }
        if (usb.uboot_req)
            enter_uboot("UBOOT (USB)");
        ui_input();
        ui_leds();
        ui_draw();
#if TELEMETRY
        {
            static uint32_t sent_at;
            if ((uint32_t)(fm1_ms - sent_at) >= 200u && usb.config) {
                uint8_t f[96];
                uint32_t n = 0, e = 0;
                sent_at = fm1_ms;
                f[n++] = 0xF0; f[n++] = 0x7D; f[n++] = 0x54;
                f[n++] = (uint8_t)(tele_max >> 7); f[n++] = (uint8_t)(tele_max & 0x7Fu);
                f[n++] = tele_lates;
                f[n++] = (uint8_t)(tele_maxgap >> 7 & 0x7Fu); f[n++] = (uint8_t)(tele_maxgap & 0x7Fu);
                {
                    uint32_t j;
                    for (j = 0; j < 4u; j++) {              /* each engine's voice: % of a block */
                        uint32_t c = eng_cost[j] * 100u / BLK_TICKS;
                        f[n++] = (uint8_t)(c > 127u ? 127u : c);
                    }
                    f[n++] = (uint8_t)(tele_trims > 127u ? 127u : tele_trims);
                    tele_trims = 0;
                    {                                        /* ANIMA: its control and its samples, % of a block */
                        uint32_t nn = an_t_n ? an_t_n : 1u, c = an_t_ctl / nn * 1000u / BLK_TICKS, m = an_t_smp / nn * 1000u / BLK_TICKS;
                        an_t_ctl = an_t_smp = an_t_n = 0;
                        f[n++] = (uint8_t)(c > 16383u ? 127u : c >> 7);
                        f[n++] = (uint8_t)(c & 0x7Fu);
                        f[n++] = (uint8_t)(m > 16383u ? 127u : m >> 7);
                        f[n++] = (uint8_t)(m & 0x7Fu);
                    }
                    for (j = 0; j < 9u; j++) {              /* the building blocks: ticks for 256, / 4 */
                        uint32_t b = bench_ticks[j] / 4u;
                        f[n++] = (uint8_t)(b > 16383u ? 127u : b >> 7);
                        f[n++] = (uint8_t)(b & 0x7Fu);
                    }
                }
                tele_max = 0;
                tele_lates = 0;
                tele_maxgap = 0;
                while (tele_r != tele_w && e < 5u) {
                    const tele_t *g = &tele_log[tele_r % 16u];
                    f[n++] = (uint8_t)(g->pct >> 7); f[n++] = (uint8_t)(g->pct & 0x7Fu);
                    f[n++] = g->late;
                    f[n++] = (uint8_t)(g->gap_10us >> 7 & 0x7Fu); f[n++] = (uint8_t)(g->gap_10us & 0x7Fu);
                    f[n++] = (uint8_t)(g->ev & 0x7Fu); f[n++] = (uint8_t)(g->ev >> 7 & 0x7Fu);
                    f[n++] = (uint8_t)(g->ev >> 14 & 0x7Fu); f[n++] = (uint8_t)(g->ev >> 21 & 0x7Fu);
                    f[n++] = g->non; f[n++] = g->voices; f[n++] = g->sounding;
                    f[n++] = (uint8_t)(g->half & 0x7Fu); f[n++] = (uint8_t)(g->half >> 7 & 0x7Fu);
                    RING_PUBLISH();
                    tele_r++;
                    e++;
                }
                f[n++] = 0xF7;
                if (so_w - so_r + (n + 2u) / 3u < SXQ)  /* (room: else this report is dropped, never waited for) */
                    ota_wire_send(f, n);
            }
        }
#endif
        {   /* nothing sounding, no key down, no loop going: the sessions' flash may be blanked a sector */
            uint32_t l, quiet = !ui.keys && !ui.save_down;
            for (l = 0; l < NLAYER && quiet; l++)
                quiet = !layer_sounding(l) && !loop[l].play && loop[l].rec != LR_REC;
            ses_idle((int)quiet);
        }
        while ((uint32_t)(fm1_ms - m) < 15u)        /* ~60 frames a second at most */
            ui_input();
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt, boot_mode;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))
        bootguard_clear(&bootguard);                /* a power-on: no failed boot to count */
    boot_mode = bootguard_begin(&bootguard);
    if (boot_mode == BOOT_ROM)
        fm1_enter_uboot();
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;                                     /* the flash driver, which runs from RAM */
    for (s = _ft_load, d = _ft_start; d < _ft_end; s++, d++)
        *d = *s;                                     /* the audio's hottest code, run from RAM too */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    if (boot_mode == BOOT_RECOVERY || recovery_key())
        recovery_main();                             /* two failed boots, or OCT- held at power-on */
    fm1_main();
    for (;;)
        ;
}
