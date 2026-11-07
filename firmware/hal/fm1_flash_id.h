/* SPDX-License-Identifier: GPL-3.0-only */
/* The SPI NOR parts found in FM-1 units (JEDEC ID, command 0x9F). Both are 1 MiB and take the same
 * commands the driver in fm1_flash.h sends (0x05/0x35 status, 0x06 write enable, 0x20 4 KiB erase,
 * 0x02 page program, 0x0B fast read):
 *   0x856014  Puya P25Q80, earlier units
 *   0xB36014  UC25HQ80, units sold from autumn 2026
 * Any other ID: Tonitor does not write to flash (saves stay in RAM, the loader keeps the old firmware). */
#pragma once
#define FL_JEDEC_P25Q80   0x856014u
#define FL_JEDEC_UC25HQ80 0xB36014u
#define FL_JEDEC_OK(id)   ((id) == FL_JEDEC_P25Q80 || (id) == FL_JEDEC_UC25HQ80)
