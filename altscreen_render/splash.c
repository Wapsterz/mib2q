/*
 * altscreen_render — start-up splash. See splash.h.
 *
 * Copyright (c) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "splash.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

void altr_splash_layout(uint64_t elapsed_us, int win_w, int win_h, altr_splash_layout_t *out)
{
    float scale = 1.0f, ang;
    if (!out) return;
    out->done = elapsed_us >= ALTR_SPLASH_HOLD_US + ALTR_SPLASH_FADE_US;
    out->alpha = elapsed_us < ALTR_SPLASH_HOLD_US
        ? 1.0f
        : 1.0f - (float)(elapsed_us - ALTR_SPLASH_HOLD_US) / (float)ALTR_SPLASH_FADE_US;
    if (out->alpha < 0.0f) out->alpha = 0.0f;
    if (scale > 0.8f * win_w / SPLASH_W) scale = 0.8f * win_w / SPLASH_W;
    if (scale > 0.8f * win_h / SPLASH_H) scale = 0.8f * win_h / SPLASH_H;
    out->dw = SPLASH_W * scale;
    out->dh = SPLASH_H * scale;
    out->x0 = (win_w - out->dw) / 2.0f;
    out->y0 = (win_h - out->dh) / 2.0f;
    ang = (float)((double)elapsed_us / ALTR_SPLASH_TURN_US) * 6.2831853f;
    out->cx = out->x0 + (SPLASH_CX + SPLASH_ORBIT * sinf(ang)) * scale;
    out->cy = out->y0 + (SPLASH_CY - SPLASH_ORBIT * cosf(ang)) * scale;
    out->r = SPLASH_DOT * scale;
}

#ifdef PLATFORM_QNX

#include <GLES2/gl2.h>

#include "splash_art.h"      /* kSplashArt */

#define SPLASH_DOT_TEX 32
#define SPLASH_UNIT 3        /* texture units 0..2 hold the video planes */

struct altr_splash {
    GLuint prog, artTex, dotTex, blankTex;
    GLint tLoc, tintLoc;
    uint64_t start_us;       /* 0 = starts at the next draw */
    int done;
};

static const char *VS =
    "attribute vec2 p; attribute vec2 uv; varying vec2 v;\n"
    "void main() { v = uv; gl_Position = vec4(p, 0.0, 1.0); }\n";
static const char *FS =
    "precision mediump float; varying vec2 v; uniform sampler2D t; uniform vec4 tint;\n"
    "void main() { gl_FragColor = texture2D(t, v) * tint; }\n";

static uint64_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
}

static GLuint compile(GLenum type, const char *src)
{
    GLint ok = 0;
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(s); return 0; }
    return s;
}

static GLuint texture(int w, int h, const unsigned char *px)
{
    GLuint t = 0;
    glGenTextures(1, &t);
    glActiveTexture(GL_TEXTURE0 + SPLASH_UNIT);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glActiveTexture(GL_TEXTURE0);
    return t;
}

altr_splash_t *altr_splash_create(void)
{
    static unsigned char dot[SPLASH_DOT_TEX * SPLASH_DOT_TEX * 4];
    static const unsigned char blank[4] = { 255, 255, 255, 255 };
    altr_splash_t *s = (altr_splash_t *)calloc(1, sizeof(*s));
    GLuint vs, fs;
    GLint ok = 0;
    int x, y;
    if (!s) return NULL;

    vs = compile(GL_VERTEX_SHADER, VS);
    fs = compile(GL_FRAGMENT_SHADER, FS);
    if (!vs || !fs) { free(s); return NULL; }
    s->prog = glCreateProgram();
    glAttachShader(s->prog, vs);
    glAttachShader(s->prog, fs);
    glBindAttribLocation(s->prog, 0, "p");
    glBindAttribLocation(s->prog, 1, "uv");
    glLinkProgram(s->prog);
    glDeleteShader(vs); glDeleteShader(fs);
    glGetProgramiv(s->prog, GL_LINK_STATUS, &ok);
    if (!ok) { glDeleteProgram(s->prog); free(s); return NULL; }
    s->tLoc = glGetUniformLocation(s->prog, "t");
    s->tintLoc = glGetUniformLocation(s->prog, "tint");

    /* The dot: #5DCAA5, edge smoothed over one texel. */
    for (y = 0; y < SPLASH_DOT_TEX; ++y) {
        for (x = 0; x < SPLASH_DOT_TEX; ++x) {
            float dx = x + 0.5f - SPLASH_DOT_TEX / 2.0f, dy = y + 0.5f - SPLASH_DOT_TEX / 2.0f;
            float a = SPLASH_DOT_TEX / 2.0f - 1.0f - sqrtf(dx * dx + dy * dy) + 0.5f;
            unsigned char *p = dot + (y * SPLASH_DOT_TEX + x) * 4;
            a = a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a;
            p[0] = 0x5D; p[1] = 0xCA; p[2] = 0xA5; p[3] = (unsigned char)(a * 255.0f + 0.5f);
        }
    }
    s->artTex = texture(SPLASH_W, SPLASH_H, kSplashArt);
    s->dotTex = texture(SPLASH_DOT_TEX, SPLASH_DOT_TEX, dot);
    s->blankTex = texture(1, 1, blank);
    fprintf(stderr, "altscreen_render: splash ready (%dx%d art)\n", SPLASH_W, SPLASH_H);
    return s;
}

void altr_splash_restart(altr_splash_t *s)
{
    if (!s) return;
    s->start_us = 0;
    s->done = 0;
}

/* One textured rectangle, in window pixels from the top left. */
static void quad(altr_splash_t *s, float x0, float y0, float x1, float y1, int w, int h,
                 GLuint tex, float c, float a)
{
    float l = x0 / w * 2.0f - 1.0f, r = x1 / w * 2.0f - 1.0f;
    float t = 1.0f - y0 / h * 2.0f, b = 1.0f - y1 / h * 2.0f;
    const GLfloat pos[8] = { l, t,  r, t,  r, b,  l, b };
    static const GLfloat uv[8] = { 0, 0,  1, 0,  1, 1,  0, 1 };
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform4f(s->tintLoc, c, c, c, a);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, pos);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void altr_splash_draw(altr_splash_t *s, int win_w, int win_h)
{
    altr_splash_layout_t L;
    uint64_t now = now_us();
    if (!s || s->done || win_w <= 0 || win_h <= 0) return;
    if (s->start_us == 0) { s->start_us = now; fprintf(stderr, "altscreen_render: splash shown\n"); }
    altr_splash_layout(now - s->start_us, win_w, win_h, &L);
    if (L.done) {
        s->done = 1;
        fprintf(stderr, "altscreen_render: splash done\n");
        return;
    }

    glUseProgram(s->prog);
    glActiveTexture(GL_TEXTURE0 + SPLASH_UNIT);
    glUniform1i(s->tLoc, SPLASH_UNIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    /* A black sheet over the video fades out with the rest. The art is
     * straight (not premultiplied) alpha, so the blend above is right for it. */
    quad(s, 0, 0, (float)win_w, (float)win_h, win_w, win_h, s->blankTex, 0.f, L.alpha);
    quad(s, L.x0, L.y0, L.x0 + L.dw, L.y0 + L.dh, win_w, win_h, s->artTex, 1.f, L.alpha);
    quad(s, L.cx - L.r, L.cy - L.r, L.cx + L.r, L.cy + L.r, win_w, win_h, s->dotTex, 1.f, L.alpha);
    glDisable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0);
}

void altr_splash_destroy(altr_splash_t *s)
{
    if (!s) return;
    if (s->artTex) glDeleteTextures(1, &s->artTex);
    if (s->dotTex) glDeleteTextures(1, &s->dotTex);
    if (s->blankTex) glDeleteTextures(1, &s->blankTex);
    if (s->prog) glDeleteProgram(s->prog);
    free(s);
}

#else /* !PLATFORM_QNX — host stubs so the maths stays testable */

struct altr_splash { int unused; };
altr_splash_t *altr_splash_create(void) { return NULL; }
void altr_splash_restart(altr_splash_t *s) { (void)s; }
void altr_splash_draw(altr_splash_t *s, int w, int h) { (void)s; (void)w; (void)h; }
void altr_splash_destroy(altr_splash_t *s) { (void)s; }

#endif
