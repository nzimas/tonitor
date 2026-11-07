/* SPDX-License-Identifier: GPL-3.0-only
 * The flash chips Tonitor writes to: the earlier units' P25Q80 and the newer UC25HQ80, nothing else. */
#include <stdio.h>
#include <stdint.h>
#include "fm1_flash_id.h"

int main(void)
{
    static const struct { uint32_t id; int ok; } t[] = {
        { 0x856014u, 1 }, { 0xB36014u, 1 },
        { 0x000000u, 0 }, { 0xFFFFFFu, 0 }, { 0x856015u, 0 }, { 0xB36015u, 0 }, { 0xEF4014u, 0 },
    };
    unsigned i, bad = 0;
    for (i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!FL_JEDEC_OK(t[i].id) != !t[i].ok) {
            printf("FAIL flash id 0x%06X: %s\n", (unsigned)t[i].id, t[i].ok ? "refused" : "accepted");
            bad++;
        }
    if (!bad) printf("flash id: P25Q80 and UC25HQ80 accepted, other IDs refused\n");
    return bad != 0;
}
