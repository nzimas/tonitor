/* SPDX-License-Identifier: GPL-3.0-only
 * TONITOR -- a firmware for the M-VAVE FM-1 for acousmatic and electroacoustic music.
 * One compilation unit (the HAL is header-only); the order matters.
 *
 * Built on Felucca (Leo Kuroshita, Hügelton Instruments) and SLOOP (isod89), GPL-3.0: the HAL, the boot,
 * the USB stack, the update entry and loader, the rescue mode. FM is msfa (Google, Pascal Gauthier,
 * Apache-2.0) in Felucca's integer port. ANIMA is a fixed-point port of Vascular's anima. */
#include <stdint.h>
#ifndef TONITOR_ID
#define TONITOR_ID "FM-1_700"    /* the package identity (tools/build.py sets it) */
#endif
#ifndef TONITOR_VERSION
#define TONITOR_VERSION "0.41"
#endif
#define FELUCCA_ID TONITOR_ID
#ifndef TELEMETRY
#define TELEMETRY 0              /* 1: the audio's timing reported over USB-MIDI SysEx (a diagnostic build) */
#endif
#define FELUCCA_OTA 1            /* the update entry (the installer), and the rescue mode */
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_CDC 0
#define FELUCCA_UAC 0

#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "tonitor_tables.h"

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* a slot's store before the index's */
static volatile uint32_t fm1_ms;                           /* milliseconds since boot (TIMER5) */
#include "bootguard.h"
bootguard_t bootguard __attribute__((section(".noinit")));

#include "libc.c"
#include "lcd.c"
#include "dsp.h"
#include "gfx.c"
#include "fm.c"
#include "anima.c"
#include "cal.c"
#include "russolo.c"
#include "druid.c"
#include "usb.c"
#include "reverb.c"
#include "layers.c"
#include "ui.c"
#include "sessions.c"
#include "main.c"
