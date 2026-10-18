/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR: the sessions and presets over USB MIDI, for the web page that keeps them on a computer and puts them
 * back (docs/backup.html, js/backup.js). The installer's framing (ota.c: F0 pack7(00 59 cmd len24 body ~sum) F7),
 * commands of its own:
 *   0x50  list                       -> 0x50  count, then a record each: number (u16), length (u32)
 *   0x51  read  number off(u32) n(u16, <= 256)   -> 0x51  number off n, then n bytes of the record
 *   0x52  write number len(u32)      -> 0x52  status: room made for it (erased as needed)
 *   0x53  data  off(u32) bytes (<= 256, in order) -> 0x53  status
 *   0x54  commit                     -> 0x54  status: its head and body checked, it is number's now
 * A record goes back whole and as it was (its head, its CRCs), only its sequence number new -- so it is the copy
 * that counts. Statuses: 0 done, 1 no flash, 2 not as asked, 3 no room, 4 not a whole record (head, length, CRC),
 * 5 the flash failed. Numbers: sessions 1 .. 64; presets PRE_NUM, PRE_NUM + 1. All in the main loop. */

static int bk_wire(const uint8_t *p, uint32_t n);   /* one F0 .. F7 message to the host (main.c; host.c) */
static void bk_done(uint32_t num);                  /* a record put back (ui.c: the screen) */

#define BK_DATA 256u
static uint8_t bk_msg[7u + 1u + SES_MAX * 6u], bk_out[2u + (sizeof bk_msg * 8u + 6u) / 7u];
static struct {
    uint8_t on;                  /* a write under way */
    uint16_t num;
    uint32_t len, got;           /* its length; bytes taken */
    int s;                       /* its first sector, sectors */
    uint32_t ns;
    uint16_t crc;                /* its body's CRC, as it comes */
    ses_head_t h;
} bk;

static uint32_t bk_rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void bk_wr32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}
static void bk_send(uint32_t cmd, uint32_t n)        /* the body already in bk_msg + 6 */
{
    uint32_t i, s = 0, o = 1, acc = 0, nb = 0;
    bk_msg[0] = 0;
    bk_msg[1] = 0x59;
    bk_msg[2] = (uint8_t)cmd;
    bk_msg[3] = (uint8_t)n;
    bk_msg[4] = (uint8_t)(n >> 8);
    bk_msg[5] = (uint8_t)(n >> 16);
    for (i = 0; i < n; i++)
        s += bk_msg[6u + i];
    bk_msg[6u + n] = (uint8_t)~s;
    bk_out[0] = 0xF0;
    for (i = 0; i < 7u + n; i++) {                   /* pack7, as ota.c's */
        acc |= (uint32_t)bk_msg[i] << nb;
        nb += 8u;
        while (nb >= 7u) {
            bk_out[o++] = (uint8_t)(acc & 0x7Fu);
            acc >>= 7;
            nb -= 7u;
        }
    }
    if (nb)
        bk_out[o++] = (uint8_t)(acc & 0x7Fu);
    bk_out[o++] = 0xF7;
    bk_wire(bk_out, o);
}
static void bk_status(uint32_t cmd, uint32_t st)
{
    bk_msg[6] = (uint8_t)st;
    bk_send(cmd, 1u);
}
/* the bytes taken so far that fill a sector (or end the record): written to the flash -- the head checked and
 * given a new sequence number first */
static int bk_flush(uint32_t upto)
{
    uint32_t sec = (upto - 1u) / SES_SECT, from = sec * SES_SECT, n = upto - from;
    if (sec == 0u) {
        memcpy(&bk.h, ses_chunk, sizeof bk.h);
        if (!ses_head_ok(&bk.h) || bk.h.len != bk.len || bk.h.num != bk.num)
            return 4;
        bk.h.seq = ++ses.seq;
        bk.h.head_crc = ses_crc((const uint8_t *)&bk.h, 30u, 0);
        memcpy(ses_chunk, &bk.h, sizeof bk.h);
        bk.crc = ses_crc(ses_chunk + sizeof bk.h, n - sizeof bk.h, 0);
    } else
        bk.crc = ses_crc(ses_chunk, n, bk.crc);
    return ses_flash_prog(SES_LO + ((uint32_t)bk.s + sec) * SES_SECT, ses_chunk, n) ? 5 : 0;
}
/* a message from the host (decoded: 00 59 cmd len24 body sum) */
static void bk_handle(const uint8_t *m, uint32_t d)
{
    uint32_t cmd = m[2], n = d - 7u, i;
    const uint8_t *b = m + 6;
    if (cmd == 0x50u) {                              /* the list */
        uint32_t k = 0;
        for (i = 0; i < ses.n; i++) {
            ses_head_t h;
            ses_flash_read(SES_LO + ses.sect[i] * SES_SECT, &h, sizeof h);
            bk_msg[7u + k * 6u] = (uint8_t)ses.num[i];
            bk_msg[8u + k * 6u] = (uint8_t)(ses.num[i] >> 8);
            bk_wr32(bk_msg + 9u + k * 6u, h.len);
            k++;
        }
        bk_msg[6] = (uint8_t)k;
        bk_send(0x50u, 1u + k * 6u);
    } else if (cmd == 0x51u && n == 8u) {            /* a piece of a record */
        uint32_t num = b[0] | (uint32_t)b[1] << 8, off = bk_rd32(b + 2), cnt = b[6] | (uint32_t)b[7] << 8, len = 0;
        int at = ses_find(num);
        if (at >= 0) {
            ses_head_t h;
            ses_flash_read(SES_LO + ses.sect[at] * SES_SECT, &h, sizeof h);
            len = h.len;
        }
        cnt = at < 0 || off >= len ? 0u : cnt > BK_DATA ? BK_DATA : cnt;
        cnt = off + cnt > len ? len - off : cnt;
        memcpy(bk_msg + 6, b, 6);
        bk_msg[12] = (uint8_t)cnt;
        bk_msg[13] = (uint8_t)(cnt >> 8);
        if (cnt)
            ses_flash_read(SES_LO + ses.sect[at] * SES_SECT + off, bk_msg + 14, cnt);
        bk_send(0x51u, 8u + cnt);
    } else if (cmd == 0x52u && n == 6u) {            /* a record to come */
        uint32_t num = b[0] | (uint32_t)b[1] << 8, len = bk_rd32(b + 2);
        int s;
        bk.on = 0;
        if (!ses.ok) {
            bk_status(0x52u, 1u);
            return;
        }
        if (len < sizeof(ses_head_t) || len > SES_BUF || !((num >= 1u && num <= SES_SLOTS) || num == PRE_NUM || num == PRE_NUM + 1u)) {
            bk_status(0x52u, 2u);
            return;
        }
        s = ses_place(len, &bk.ns);
        if (s < 0) {
            bk_status(0x52u, s == -SES_FULL ? 3u : 5u);
            return;
        }
        bk.on = 1;
        bk.num = (uint16_t)num;
        bk.len = len;
        bk.got = 0;
        bk.s = s;
        bk_status(0x52u, 0u);
    } else if (cmd == 0x53u && n >= 4u) {            /* its bytes, in order */
        uint32_t off = bk_rd32(b), cnt = n - 4u, st = 0;
        if (!bk.on || off != bk.got || off + cnt > bk.len || cnt > BK_DATA) {
            bk.on = 0;
            bk_status(0x53u, 2u);
            return;
        }
        for (i = 0; i < cnt && !st; i++) {
            ses_chunk[bk.got % SES_SECT] = b[4u + i];
            if (++bk.got % SES_SECT == 0u || bk.got == bk.len)
                st = (uint32_t)bk_flush(bk.got);
        }
        if (st)
            bk.on = 0;
        bk_status(0x53u, st);
    } else if (cmd == 0x54u) {                       /* whole: its body checked, then it is the record */
        uint32_t st = !bk.on || bk.got != bk.len ? 2u : bk.crc != bk.h.body_crc ? 4u : 0u;
        if (!st && ses_commit(bk.num, bk.s, bk.ns, &bk.h) != SES_OK)
            st = 5u;
        bk.on = 0;
        bk_status(0x54u, st);
        if (!st)
            bk_done(bk.num);
    }
}
