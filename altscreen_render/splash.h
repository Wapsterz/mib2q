/*
 * altscreen_render — start-up splash ("Navi Paslaugos").
 *
 * When a phone session starts, the software (GLES) presenter draws an orbit
 * spinner with a turning green dot and the text "Navi Paslaugos" on black for
 * ALTR_SPLASH_HOLD_US, then fades into the phone's picture over
 * ALTR_SPLASH_FADE_US. Decoding carries on underneath, so the phone sees no
 * difference. It is drawn after the video in the same frame.
 *
 * GLES path only. The hardware decoder (hw_decode.c) blits straight into the
 * cluster window without GL and shows no splash. ALTR_SPLASH=0 turns it off.
 *
 * Artwork: splash/splash.svg, rendered into splash_art.h by
 * tools/make_splash_art.py.
 *
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ALTR_SPLASH_H
#define ALTR_SPLASH_H

#include <stdint.h>

#define ALTR_SPLASH_NO_DATA 1
#include "splash_art.h"      /* SPLASH_W/H, SPLASH_CX/CY, SPLASH_ORBIT, SPLASH_DOT */
#undef ALTR_SPLASH_NO_DATA

#define ALTR_SPLASH_HOLD_US 2500000ULL
#define ALTR_SPLASH_FADE_US 400000ULL
#define ALTR_SPLASH_TURN_US 1500000.0   /* one turn of the dot */

/* Where and how opaque the splash is `elapsed_us` after it started, in a window
 * of win_w x win_h pixels (origin top-left). Pure maths: no GL. */
typedef struct altr_splash_layout {
    int done;                 /* 1 once the fade has finished: draw nothing */
    float alpha;              /* 1 during the hold, then 1 -> 0 over the fade */
    float x0, y0, dw, dh;     /* the artwork rectangle */
    float cx, cy, r;          /* the dot: centre and radius */
} altr_splash_layout_t;

void altr_splash_layout(uint64_t elapsed_us, int win_w, int win_h, altr_splash_layout_t *out);

typedef struct altr_splash altr_splash_t;

/* Needs a current GLES2 context. NULL on failure (no splash, nothing else affected). */
altr_splash_t *altr_splash_create(void);

/* Start again from the beginning on the next draw (a new phone session). */
void altr_splash_restart(altr_splash_t *s);

/* Draw over what was just drawn, before eglSwapBuffers. No-op once finished. */
void altr_splash_draw(altr_splash_t *s, int win_w, int win_h);

void altr_splash_destroy(altr_splash_t *s);

#endif /* ALTR_SPLASH_H */
