/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: sessions -- the whole machine at a moment: every layer's engine, both its sounds, its octave,
 * level, pan and reverb send, its loop; the reverb; the layer chosen.
 *
 * In flash, in the area SLOOP keeps its projects in (0x97000 .. 0xE0000, 73 sectors of 4 KiB). A session is
 * one record, from the start of a sector over as many as it needs:
 *   head (32 B): magic, its number, its length, a sequence number (the newest copy wins), the format, the
 *                sizes of the sound structures it holds, a CRC of the head and one of the rest
 *   body: the globals, then per layer its settings, its FM patch and ANIMA design (each with the seed that
 *         made it: should the structures change in a later firmware, the sounds are made again from their
 *         seeds), its loops: their number, each one's length and events' count, then the events (4 bytes
 *         each). Format 1 (before the overdubs) had one loop: still read
 * The list is the records found at boot, sector by sector. Saving over a session writes the new copy into
 * free sectors first and only then kills the old one, by programming its magic to zero (no erase): a power
 * cut in between leaves the old or both, never neither. Erasing a sector silences the audio for a moment,
 * so a save takes sectors already blank if it can, and the main loop blanks free dirty sectors one at a
 * time while nothing sounds (ses_idle). */

#define SES_LO 0x00097000u
#define SES_HI 0x000E0000u
#define SES_SECT 4096u
#define SES_NSECT ((SES_HI - SES_LO) / SES_SECT)
#define SES_MAGIC 0x31534341u    /* "ACS1" */
#define SES_FORMAT 5u            /* 5: the generations; 4: the paths; 3: a CALIGULA design a layer; 2: up to ten loops
                                  * a layer (1: one) */
#define SES_MAX 73u              /* records in the list (one a sector at most) */
#define SES_SLOTS 64u            /* the slots: sessions 1 .. 64 */
/* the largest session: the head, the globals, six layers with their sounds and full loopers (~54 KiB) */
#define SES_BUF (32u + 8u + NLAYER * (16u + sizeof(fm_patch_t) + sizeof(an_design_t) + sizeof(cal_design_t) + \
                                      8u * LOOP_TRK + 4u * LOOP_N))

/* the flash, for the target (main.c) or the host test (host.c); offsets from the flash's start */
static int ses_flash_read(uint32_t off, void *p, uint32_t n);
static int ses_flash_erase(uint32_t off);   /* a 4 KiB sector */
static int ses_flash_prog(uint32_t off, const void *p, uint32_t n);

typedef struct {
    uint32_t magic;
    uint16_t num, format;
    uint32_t len;                /* head and body, bytes */
    uint32_t seq;
    uint16_t fm_size, an_size;
    uint16_t body_crc, cal_size; /* (cal_size: format 3) */
    uint32_t rsv2;
    uint16_t rsv3, head_crc;     /* CRC of the 30 bytes before it */
} ses_head_t;
typedef char ses_head_is_32[sizeof(ses_head_t) == 32u ? 1 : -1];

static struct {
    uint8_t n;                   /* sessions found */
    uint16_t num[SES_MAX];       /* sorted by number */
    uint8_t sect[SES_MAX];       /* the first sector of each */
    uint8_t nsect[SES_MAX];
    uint32_t seq;                /* the highest sequence number seen */
    uint8_t used[SES_NSECT];     /* a sector holds a live record */
    uint8_t dirty[SES_NSECT];    /* free, but not blank: erase before writing (1 = known dirty, 2 = unknown) */
    uint16_t current;            /* the session loaded or last saved, 0 = none */
    uint8_t ok;                  /* the flash is ours to use */
} ses;
static uint8_t ses_buf[SES_BUF];

static uint16_t ses_crc(const uint8_t *p, uint32_t n, uint32_t c)   /* CRC16, poly 0x1021 */
{
    uint32_t i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++)
            c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return (uint16_t)c;
}

static int ses_head_ok(const ses_head_t *h)
{
    return h->magic == SES_MAGIC && h->len >= sizeof(ses_head_t) && h->len <= SES_BUF &&
           h->head_crc == ses_crc((const uint8_t *)h, 30u, 0);
}

static void ses_list_add(uint32_t num, uint32_t sect, uint32_t nsect)
{
    uint32_t i = ses.n, k;
    if (ses.n >= SES_MAX)
        return;
    while (i > 0 && ses.num[i - 1u] > num)
        i--;
    for (k = ses.n; k > i; k--) {
        ses.num[k] = ses.num[k - 1u];
        ses.sect[k] = ses.sect[k - 1u];
        ses.nsect[k] = ses.nsect[k - 1u];
    }
    ses.num[i] = (uint16_t)num;
    ses.sect[i] = (uint8_t)sect;
    ses.nsect[i] = (uint8_t)nsect;
    ses.n++;
}
static int ses_find(uint32_t num)
{
    uint32_t i;
    for (i = 0; i < ses.n; i++)
        if (ses.num[i] == num)
            return (int)i;
    return -1;
}
/* kill a record in place: its magic programmed to zero (bits only go 1 -> 0: no erase) */
static void ses_kill(uint32_t sect, uint32_t nsect)
{
    uint32_t zero = 0, k;                   /* (in RAM: the flash is programmed from RAM only) */
    ses_flash_prog(SES_LO + sect * SES_SECT, &zero, 4u);
    for (k = 0; k < nsect && sect + k < SES_NSECT; k++) {
        ses.used[sect + k] = 0;
        ses.dirty[sect + k] = 1;
    }
}

/* boot: the records in the area. Two copies of a number (a save cut short): the newer one stays */
static void ses_scan(int flash_ok)
{
    uint32_t s = 0;
    memset(&ses, 0, sizeof ses);
    ses.ok = (uint8_t)flash_ok;
    if (!flash_ok)
        return;
    while (s < SES_NSECT) {
        ses_head_t h;
        uint32_t ns;
        ses_flash_read(SES_LO + s * SES_SECT, &h, sizeof h);
        if (h.magic == 0xFFFFFFFFu) {                   /* blank, or so its start says: checked when used */
            ses.dirty[s] = 2;
            s++;
            continue;
        }
        if (!ses_head_ok(&h) && !(h.magic == 0 && h.len >= sizeof h && h.len <= SES_BUF)) {
            ses.dirty[s++] = 1;                          /* anything else (SLOOP's projects): to erase */
            continue;
        }
        ns = (h.len + SES_SECT - 1u) / SES_SECT;
        if (h.magic == SES_MAGIC) {
            int dup = ses_find(h.num);
            if (h.seq > ses.seq)
                ses.seq = h.seq;
            if (dup >= 0) {                              /* keep the newer */
                ses_head_t o;
                ses_flash_read(SES_LO + ses.sect[dup] * SES_SECT, &o, sizeof o);
                if (o.seq > h.seq) {
                    ses_kill(s, ns);
                    s += ns;
                    continue;
                }
                ses_kill(ses.sect[dup], ses.nsect[dup]);
                for (; dup + 1 < ses.n; dup++) {
                    ses.num[dup] = ses.num[dup + 1];
                    ses.sect[dup] = ses.sect[dup + 1];
                    ses.nsect[dup] = ses.nsect[dup + 1];
                }
                ses.n--;
            }
            ses_list_add(h.num, s, ns);
            for (; ns && s < SES_NSECT; ns--, s++)
                ses.used[s] = 1;
        } else {                                         /* a dead record: dirty */
            for (; ns && s < SES_NSECT; ns--, s++)
                ses.dirty[s] = 1;
        }
    }
}

/* a sector is blank (all 0xFF) */
static int ses_blank(uint32_t s)
{
    uint32_t buf[64], o, i;
    for (o = 0; o < SES_SECT; o += sizeof buf) {
        ses_flash_read(SES_LO + s * SES_SECT + o, buf, sizeof buf);
        for (i = 0; i < 64u; i++)
            if (buf[i] != 0xFFFFFFFFu)
                return 0;
    }
    return 1;
}
static int ses_is_clean(uint32_t s)
{
    if (ses.dirty[s] == 2u)
        ses.dirty[s] = (uint8_t)(ses_blank(s) ? 0 : 1);
    return !ses.dirty[s];
}

/* where n sectors fit: a free run, a blank one if there is one (no erase, no gap in the sound) */
static int ses_alloc(uint32_t n)
{
    uint32_t s, k, best = SES_NSECT, best_dirty = 0xFFFFu;
    for (s = 0; s + n <= SES_NSECT; s++) {
        uint32_t d = 0;
        for (k = 0; k < n && !ses.used[s + k]; k++)
            d += !ses_is_clean(s + k);
        if (k < n)
            continue;
        if (d < best_dirty) {
            best = s;
            best_dirty = d;
            if (!d)
                break;
        }
    }
    return best < SES_NSECT ? (int)best : -1;
}

/* ------------------------------------------------------------ the body --- */
typedef struct {
    uint8_t engine;
    int8_t gr;                   /* the granulator (0 in sessions before it: off) */
    int8_t oct, level, pan, send;
    uint8_t loop_state;
    int8_t flt;                  /* the DJ filter (0 in sessions before it: off) */
    uint32_t loop_len, loop_n;
} ses_layer_t;

/* a sound kept as seed and level: its level (20 bits), its generation (4 bits, format 5) and the path it was drawn
 * on (the top byte, format 4) */
static int32_t ses_c2_level(const ses_head_t *h, uint32_t w)
{
    return (int32_t)(h->format >= 5u ? w & 0xFFFFFu : h->format >= 4u ? w & 0xFFFFFFu : w);
}
static uint32_t ses_c2_path(const ses_head_t *h, uint32_t w)
{
    return h->format >= 4u && (w >> 24) < PATH_N ? w >> 24 : PATH_OPEN;
}
static uint32_t ses_c2_gen(const ses_head_t *h, uint32_t w)
{
    return h->format >= 5u && (w >> 20 & 15u) < GEN_N ? w >> 20 & 15u : 0u;
}
static uint32_t ses_c2_word(int32_t level, uint32_t gen, uint32_t path)
{
    return ((uint32_t)level & 0xFFFFFu) | gen << 20 | path << 24;
}
/* the sound kept in full, should it have to be drawn again from its seed (another firmware's layout): its path
 * and generation, in loop_n's spare bits (format 5) */
static uint32_t ses_full_path(const ses_head_t *h, uint32_t loop_n)
{
    return h->format >= 5u && (loop_n >> 20 & 15u) < PATH_N ? loop_n >> 20 & 15u : PATH_OPEN;
}
static uint32_t ses_full_gen(const ses_head_t *h, uint32_t loop_n)
{
    return h->format >= 5u && (loop_n >> 16 & 15u) < GEN_N ? loop_n >> 16 & 15u : 0u;
}

static uint8_t *ses_put(uint8_t *p, const void *src, uint32_t n)
{
    memcpy(p, src, n);
    return p + n;
}

/* the machine now, into ses_buf; its length */
static uint32_t ses_pack(uint32_t num)
{
    ses_head_t *h = (ses_head_t *)ses_buf;
    uint8_t *p = ses_buf + sizeof *h;
    uint32_t l, i;
    uint8_t g[8];
    g[0] = ui.sel;
    for (i = 0; i < 4u; i++)
        g[1u + i] = (uint8_t)ui.rvp[i];
    g[5] = ui.scale;
    g[6] = ui.root;
    g[7] = 0;
    p = ses_put(p, g, sizeof g);
    for (l = 0; l < NLAYER; l++) {
        ses_layer_t y;
        const looper_t *lp = &loop[l];
        uint32_t nt = lp->ntrk, n = nt ? lp->trk[nt - 1u].first + lp->trk[nt - 1u].n : 0, k;
        memset(&y, 0, sizeof y);
        y.engine = ui.engine[l];
        y.oct = ui.oct[l];
        y.level = ui.level[l];
        y.pan = ui.pan[l];
        y.send = ui.send[l];
        y.flt = ui.flt[l];
        y.gr = ui.gr[l];
        y.loop_state = (uint8_t)(lp->play ? 1 : 0);    /* the loops kept (one still recording is not) */
        y.loop_len = nt | (uint32_t)(uint8_t)ui.cr[l] << 8 | (uint32_t)ui.path[l] << 16;   /* (the crush, the path in
                                                                                             * the spare bits) */
        {   /* (and, should the sound kept in full have to be drawn again: its generation and path) */
            uint32_t g = y.engine == ENG_FM ? fm_buf[l][fm_pub[l]].gen : y.engine == ENG_ANIMA ? an_buf[l][an_pub[l]].gen
                                                                                            : cal_buf[l][cal_pub[l]].gen;
            uint32_t pa = y.engine == ENG_FM ? fm_buf[l][fm_pub[l]].path : y.engine == ENG_ANIMA ? an_buf[l][an_pub[l]].path
                                                                                              : cal_buf[l][cal_pub[l]].path;
            y.loop_n = n | g << 16 | pa << 20;
        }
        p = ses_put(p, &y, sizeof y);
        /* the sounds: the engine in use in full; the other two as the seed that made them, their level and
         * (top byte, format 4) their path (drawn again from them: the same sound, at once) -- so a session
         * stays in one sector */
        {
            uint32_t c2[2];
            if (y.engine == ENG_FM)
                p = ses_put(p, &fm_buf[l][fm_pub[l]], sizeof(fm_patch_t));
            else {
                c2[0] = fm_buf[l][fm_pub[l]].seed;
                c2[1] = ses_c2_word(fm_buf[l][fm_pub[l]].gain, fm_buf[l][fm_pub[l]].gen, fm_buf[l][fm_pub[l]].path);
                p = ses_put(p, c2, sizeof c2);
            }
            if (y.engine == ENG_ANIMA)
                p = ses_put(p, &an_buf[l][an_pub[l]], sizeof(an_design_t));
            else {
                c2[0] = an_buf[l][an_pub[l]].seed;
                c2[1] = ses_c2_word(an_buf[l][an_pub[l]].out, an_buf[l][an_pub[l]].gen, an_buf[l][an_pub[l]].path);
                p = ses_put(p, c2, sizeof c2);
            }
            if (y.engine == ENG_CAL)
                p = ses_put(p, &cal_buf[l][cal_pub[l]], sizeof(cal_design_t));
            else {
                c2[0] = cal_buf[l][cal_pub[l]].seed;
                c2[1] = ses_c2_word(cal_buf[l][cal_pub[l]].out, cal_buf[l][cal_pub[l]].gen, cal_buf[l][cal_pub[l]].path);
                p = ses_put(p, c2, sizeof c2);
            }
        }
        for (k = 0; k < nt; k++) {
            uint32_t tl[2];
            tl[0] = lp->trk[k].len;
            tl[1] = lp->trk[k].n;
            p = ses_put(p, tl, sizeof tl);
        }
        for (i = 0; i < n && p + 4 <= ses_buf + SES_BUF; i++)   /* t (24 bits), note, on */
            p = ses_put(p, &lp->ev[i], 4u);
    }
    memset(h, 0, sizeof *h);
    h->magic = SES_MAGIC;
    h->num = (uint16_t)num;
    h->format = SES_FORMAT;
    h->len = (uint32_t)(p - ses_buf);
    h->seq = ++ses.seq;
    h->fm_size = sizeof(fm_patch_t);
    h->an_size = sizeof(an_design_t);
    h->cal_size = sizeof(cal_design_t);
    h->body_crc = ses_crc(ses_buf + sizeof *h, h->len - sizeof *h, 0);
    h->head_crc = ses_crc(ses_buf, 30u, 0);
    return h->len;
}

/* the record in ses_buf (len bytes, its head filled) written as number num: into free sectors first, read back,
 * and only then the old copy of num killed (a power cut leaves the old, or both: never neither) */
static int ses_write(uint32_t num, uint32_t len)
{
    uint32_t ns, k;
    int s, old;
    ns = (len + SES_SECT - 1u) / SES_SECT;
    s = ses_alloc(ns);
    if (s < 0)
        return SES_FULL;
    for (k = 0; k < ns; k++)
        if (!ses_is_clean((uint32_t)s + k)) {
            if (ses_flash_erase(SES_LO + ((uint32_t)s + k) * SES_SECT))
                return SES_FAIL;
            ses.dirty[(uint32_t)s + k] = 0;
        }
    for (k = 0; k < ns; k++)                            /* (any write marks them dirty if it fails) */
        ses.dirty[(uint32_t)s + k] = 1;
    if (ses_flash_prog(SES_LO + (uint32_t)s * SES_SECT, ses_buf, len))
        return SES_FAIL;
    {   /* read back */
        ses_head_t h;
        ses_flash_read(SES_LO + (uint32_t)s * SES_SECT, &h, sizeof h);
        if (memcmp(&h, ses_buf, sizeof h))
            return SES_FAIL;
    }
    for (k = 0; k < ns; k++) {
        ses.used[(uint32_t)s + k] = 1;
        ses.dirty[(uint32_t)s + k] = 0;
    }
    old = ses_find(num);
    if (old >= 0) {                                     /* the old copy goes now, the new one is safe */
        ses_kill(ses.sect[old], ses.nsect[old]);
        ses.sect[old] = (uint8_t)s;
        ses.nsect[old] = (uint8_t)ns;
    } else
        ses_list_add(num, (uint32_t)s, ns);
    return SES_OK;
}

/* save the machine as session num (a new one, or over the one of that number); SES_OK (ui.c) = done */
static int ses_save(uint32_t num)
{
    int rc;
    if (!ses.ok)
        return SES_NOFLASH;
    if (num < 1u || num > SES_SLOTS)
        return SES_FAIL;
    rc = ses_write(num, ses_pack(num));
    if (rc == SES_OK)
        ses.current = (uint16_t)num;
    return rc;
}

/* ------------------------------------------------------------- presets --- *
 * 64 a engine, P1 .. P64: each the sound as what draws it again bit for bit -- its seed, path and generation
 * (dsp.h GEN_NOW) and its measured level -- 12 bytes, so all 192 are one record of the store, number PRE_NUM,
 * written as a session is (a new copy, then the old one killed). Read a slot at a time from the flash; changed
 * in ses_buf. */
#define PRE_NUM 1000u
#define PRE_SLOTS 64u
#define PRE_FORMAT 1u
typedef struct {
    uint32_t seed;
    uint32_t word;               /* ses_c2_word: level, generation, path */
    uint8_t used, kind, rsv[2];  /* (kind: the family, kind or species, for the list's name) */
} pre_rec_t;
typedef char pre_rec_is_12[sizeof(pre_rec_t) == 12u ? 1 : -1];
#define PRE_LEN (32u + NENG * PRE_SLOTS * 12u)

/* engine e's slot (1 .. 64): 1 and *r if it holds a sound */
static int pre_get(uint32_t e, uint32_t slot, pre_rec_t *r)
{
    int i = ses_find(PRE_NUM);
    memset(r, 0, sizeof *r);
    if (i < 0 || e >= NENG || slot < 1u || slot > PRE_SLOTS)
        return 0;
    ses_flash_read(SES_LO + ses.sect[i] * SES_SECT + 32u + (e * PRE_SLOTS + slot - 1u) * 12u, r, 12u);
    return r->used == 1u;
}
static int pre_kind(uint32_t e, uint32_t slot)
{
    pre_rec_t r;
    return pre_get(e, slot, &r) ? (int)r.kind : -1;
}
/* layer l's sound on engine e into engine e's slot */
static int pre_save(uint32_t l, uint32_t e, uint32_t slot)
{
    ses_head_t *h = (ses_head_t *)ses_buf;
    pre_rec_t rec;
    int i = ses_find(PRE_NUM);
    if (!ses.ok)
        return SES_NOFLASH;
    if (e >= NENG || slot < 1u || slot > PRE_SLOTS)
        return SES_FAIL;
    memset(ses_buf, 0, PRE_LEN);                       /* the table as it is (or an empty one) */
    if (i >= 0) {
        ses_flash_read(SES_LO + ses.sect[i] * SES_SECT, ses_buf, PRE_LEN);
        if (h->len != PRE_LEN || ses_crc(ses_buf + 32u, PRE_LEN - 32u, 0) != h->body_crc)
            memset(ses_buf, 0, PRE_LEN);
    }
    memset(&rec, 0, sizeof rec);
    rec.used = 1;
    if (e == ENG_FM) {
        const fm_patch_t *s_ = &fm_buf[l][fm_pub[l]];
        rec.seed = s_->seed, rec.word = ses_c2_word(s_->gain, s_->gen, s_->path), rec.kind = s_->family;
    } else if (e == ENG_ANIMA) {
        const an_design_t *s_ = &an_buf[l][an_pub[l]];
        rec.seed = s_->seed, rec.word = ses_c2_word(s_->out, s_->gen, s_->path), rec.kind = s_->kind;
    } else {
        const cal_design_t *s_ = &cal_buf[l][cal_pub[l]];
        rec.seed = s_->seed, rec.word = ses_c2_word(s_->out, s_->gen, s_->path), rec.kind = s_->kind;
    }
    memcpy(ses_buf + 32u + (e * PRE_SLOTS + slot - 1u) * 12u, &rec, 12u);
    memset(h, 0, sizeof *h);
    h->magic = SES_MAGIC;
    h->num = (uint16_t)PRE_NUM;
    h->format = PRE_FORMAT;
    h->len = PRE_LEN;
    h->seq = ++ses.seq;
    h->body_crc = ses_crc(ses_buf + 32u, PRE_LEN - 32u, 0);
    h->head_crc = ses_crc(ses_buf, 30u, 0);
    return ses_write(PRE_NUM, PRE_LEN);
}
/* engine e's slot drawn again into layer l (the layer crossfades to it, as to a new sound) */
static int pre_load(uint32_t l, uint32_t e, uint32_t slot)
{
    pre_rec_t r;
    ses_head_t h;
    if (!pre_get(e, slot, &r))
        return SES_FAIL;
    memset(&h, 0, sizeof h);
    h.format = SES_FORMAT;
    return sound_from_seed(l, e, r.seed, ses_c2_path(&h, r.word), ses_c2_gen(&h, r.word), ses_c2_level(&h, r.word))
               ? SES_OK : SES_FAIL;
}

/* the first empty slot, 0 = none */
static uint32_t ses_next_number(void)
{
    uint32_t n;
    for (n = 1; n <= SES_SLOTS; n++)
        if (ses_find(n) < 0)
            return n;
    return 0;
}
static int ses_has(uint32_t num) { return ses_find(num) >= 0; }
static int ses_current(void) { return ses.current; }
/* an empty slot chosen: the session saves go to from now on (the machine as it is) */
static void ses_choose(uint32_t num)
{
    if (num >= 1u && num <= SES_SLOTS)
        ses.current = (uint16_t)num;
}
/* load the session in slot num */
static int ses_load_slot(uint32_t num)
{
    int i = ses_find(num);
    return i < 0 ? SES_FAIL : ses_load((uint32_t)i);
}

/* wait until the ISR has taken every event posted (a few ms) */
static void ses_events_drain(void)
{
    uint32_t t0 = fm1_ms;
    while (ev_r != ev_w && (uint32_t)(fm1_ms - t0) < 200u) {
#ifndef HOST
        fm1_wdt_feed();
#else
        host_pump();
#endif
    }
}

/* load session i of the list: the machine as it was saved; 0 = done */
static int ses_load(uint32_t i)
{
    ses_head_t h;
    const uint8_t *p;
    uint32_t l, k, len;
    if (i >= ses.n)
        return SES_FAIL;
    ses_flash_read(SES_LO + ses.sect[i] * SES_SECT, &h, sizeof h);
    if (!ses_head_ok(&h))
        return SES_FAIL;
    len = h.len;
    for (k = 0; k < len; k += 256u)
        ses_flash_read(SES_LO + ses.sect[i] * SES_SECT + k, ses_buf + k, len - k > 256u ? 256u : len - k);
    if (ses_crc(ses_buf + sizeof h, len - sizeof h, 0) != h.body_crc)
        return SES_FAIL;
    for (l = 0; l < NLAYER; l++) {                      /* silence, the loops emptied, the latches let go */
        ui.latch[l] = 0;
        ev_post(EV_STOP, l, 0, 0);
        ev_post(EV_LOOP_CLEAR, l, 0, 0);
    }
    ses_events_drain();
    p = ses_buf + sizeof h;
    ui.sel = (uint8_t)(p[0] < NLAYER ? p[0] : 0);
    for (k = 0; k < 4u; k++)
        ui.rvp[k] = (int8_t)clampi(p[1u + k], 0, 100);
    rv_param(ui.rvp[0], ui.rvp[1], ui.rvp[2], ui.rvp[3]);
    ui.scale = (uint8_t)(p[5] < NSCALE ? p[5] : 0);  /* (sessions before the scales: C major) */
    ui.root = (uint8_t)(p[6] < 12u ? p[6] : 0);
    p += 8;
    for (l = 0; l < NLAYER; l++) {
        ses_layer_t y;
        looper_t *lp = &loop[l];
        uint32_t fk, ak, ck;
        memcpy(&y, p, sizeof y);
        p += sizeof y;
        ui.engine[l] = (uint8_t)(y.engine < NENG ? y.engine : 0);
        ui.oct[l] = (int8_t)clampi(y.oct, -3, 3);
        ui.level[l] = (int8_t)clampi(y.level, 0, LEVEL_MAX);
        ui.pan[l] = (int8_t)clampi(y.pan, -PAN_MAX, PAN_MAX);
        ui.send[l] = (int8_t)clampi(y.send, 0, 100);
        ui.flt[l] = (int8_t)clampi(y.flt, -FLT_MAX, FLT_MAX);
        ui.gr[l] = (int8_t)clampi(y.gr, -FLT_MAX, FLT_MAX);
        ui.cr[l] = 0;                                   /* (read with the loops below, format 2; the path, 4) */
        ui.path[l] = PATH_OPEN;
        gran_profile(l, ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks());   /* a new character, as loaded */
        level_pan_apply(l);
        /* the sounds, into the buffers the ISR is not using, then published. Format 3: the engine in use in
         * full, the others as seed and level; formats 1 and 2: FM and ANIMA in full, no CALIGULA (drawn anew).
         * In full but of another layout (another firmware): made again from its seed and measured */
        sound_slot_free(&fm_pub[l], &fm_use[l]);
        sound_slot_free(&an_pub[l], &an_use[l]);
        sound_slot_free(&cal_pub[l], &cal_use[l]);
        fk = fm_use[l] ^ 1u;
        ak = an_use[l] ^ 1u;
        ck = cal_use[l] ^ 1u;
        {
            uint32_t c2[2], full = h.format < 3u || ui.engine[l] == ENG_FM;
            if (full && h.fm_size == sizeof(fm_patch_t))
                memcpy(&fm_buf[l][fk], p, sizeof(fm_patch_t));
            else if (full) {
                memcpy(c2, p + h.fm_size - 4u, 4u);
                fm_generate(&fm_buf[l][fk], -1, c2[0], ses_full_path(&h, y.loop_n), ses_full_gen(&h, y.loop_n));
                fm_measure(&fm_buf[l][fk]);
            } else {
                memcpy(c2, p, sizeof c2);
                fm_generate(&fm_buf[l][fk], -1, c2[0], ses_c2_path(&h, c2[1]), ses_c2_gen(&h, c2[1]));
                fm_buf[l][fk].gain = ses_c2_level(&h, c2[1]);
            }
            p += full ? h.fm_size : 8u;
            full = h.format < 3u || ui.engine[l] == ENG_ANIMA;
            if (full && h.an_size == sizeof(an_design_t))
                memcpy(&an_buf[l][ak], p, sizeof(an_design_t));
            else if (full) {
                memcpy(c2, p, 4u);
                an_design(&an_buf[l][ak], -1, c2[0], ses_full_path(&h, y.loop_n), ses_full_gen(&h, y.loop_n));
                an_measure(&an_buf[l][ak]);
            } else {
                memcpy(c2, p, sizeof c2);
                an_design(&an_buf[l][ak], -1, c2[0], ses_c2_path(&h, c2[1]), ses_c2_gen(&h, c2[1]));
                an_buf[l][ak].out = ses_c2_level(&h, c2[1]);
            }
            p += full ? h.an_size : 8u;
            if (h.format < 3u) {
                cal_design(&cal_buf[l][ck], -1, ui.seed = ui.seed * 1664525u + 1013904223u + fm1_ticks(), ui.path[l], GEN_NOW);
                cal_measure(&cal_buf[l][ck]);
            } else if (ui.engine[l] == ENG_CAL) {
                if (h.cal_size == sizeof(cal_design_t))
                    memcpy(&cal_buf[l][ck], p, sizeof(cal_design_t));
                else {
                    memcpy(c2, p, 4u);
                    cal_design(&cal_buf[l][ck], -1, c2[0], ses_full_path(&h, y.loop_n), ses_full_gen(&h, y.loop_n));
                    cal_measure(&cal_buf[l][ck]);
                }
                p += h.cal_size;
            } else {
                memcpy(c2, p, sizeof c2);
                cal_design(&cal_buf[l][ck], -1, c2[0], ses_c2_path(&h, c2[1]), ses_c2_gen(&h, c2[1]));
                cal_buf[l][ck].out = ses_c2_level(&h, c2[1]);
                p += 8u;
            }
        }
        RING_PUBLISH();
        cal_pub[l] = (uint8_t)ck;
        RING_PUBLISH();
        fm_pub[l] = (uint8_t)fk;
        an_pub[l] = (uint8_t)ak;
        ev_post(EV_ENGINE, l, 0, ui.engine[l]);
        /* the loops: into the emptied looper (the ISR leaves an empty one alone), then set going */
        {
            uint32_t nt, play, first = 0, n = h.format >= 5u ? y.loop_n & 0xFFFFu : y.loop_n;   /* (format 5: the sound's generation and path above) */
            if (h.format == 1u) {                       /* one loop: 3 playing, 4 stopped, else none */
                nt = (y.loop_state == 3u || y.loop_state == 4u) ? 1u : 0u;
                play = y.loop_state == 3u;
                lp->trk[0].len = y.loop_len;
                lp->trk[0].n = (uint16_t)(n > LOOP_N ? LOOP_N : n);
            } else {
                ui.cr[l] = (int8_t)clampi((int8_t)(y.loop_len >> 8), -FLT_MAX, FLT_MAX);
                ui.path[l] = (uint8_t)((y.loop_len >> 16 & 0xFFu) < PATH_N ? y.loop_len >> 16 & 0xFFu : PATH_OPEN);
                y.loop_len &= 0xFFu;
                nt = y.loop_len > LOOP_TRK ? LOOP_TRK : y.loop_len;
                play = y.loop_state;
                for (k = 0; k < y.loop_len; k++) {
                    uint32_t tl[2];
                    memcpy(tl, p, sizeof tl);
                    p += sizeof tl;
                    if (k < nt) {
                        lp->trk[k].len = tl[0] ? tl[0] : 2u;
                        lp->trk[k].n = (uint16_t)tl[1];
                    }
                }
            }
            for (k = 0; k < n; k++, p += 4)
                if (k < LOOP_N)
                    memcpy(&lp->ev[k], p, 4u);
            for (k = 0; k < nt; k++) {                 /* each loop's events, after the one before's */
                lp->trk[k].first = (uint16_t)first;
                if (first + lp->trk[k].n > LOOP_N || first + lp->trk[k].n > n)
                    lp->trk[k].n = (uint16_t)(first < n && first < LOOP_N ? (n < LOOP_N ? n : LOOP_N) - first : 0);
                first += lp->trk[k].n;
            }
            lp->nev = first;
            level_pan_apply(l);                         /* (the crush, now read) */
            if (nt) {
                lp->ntrk = (uint8_t)nt;
                RING_PUBLISH();
                ev_post(EV_LOOP_SET, l, 0, play);
            }
        }
        ses_events_drain();
    }
    ses.current = ses.num[i];
    return SES_OK;
}

/* the main loop, now and then: one free dirty sector blanked, while nothing sounds (the erase silences the
 * audio for some tens of ms) */
static uint32_t ses_idle_at;
static void ses_idle(int quiet)
{
    uint32_t s;
    if (!ses.ok || !quiet || (uint32_t)(fm1_ms - ses_idle_at) < 1000u)
        return;
    ses_idle_at = fm1_ms;
    for (s = 0; s < SES_NSECT; s++)
        if (!ses.used[s] && ses.dirty[s] == 1u) {
            if (!ses_flash_erase(SES_LO + s * SES_SECT))
                ses.dirty[s] = 0;
            return;
        }
}
