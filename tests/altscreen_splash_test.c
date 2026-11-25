/* Splash timing and geometry (altscreen_render/splash.c, the GL-free part). */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "splash.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int near(float a, float b) { return fabsf(a - b) < 0.01f; }

int main(void)
{
    altr_splash_layout_t L;

    /* Holds fully opaque, then fades to nothing, then is done. */
    altr_splash_layout(0, 1440, 540, &L);
    CHECK(!L.done && near(L.alpha, 1.f));
    altr_splash_layout(ALTR_SPLASH_HOLD_US - 1, 1440, 540, &L);
    CHECK(!L.done && near(L.alpha, 1.f));
    altr_splash_layout(ALTR_SPLASH_HOLD_US + ALTR_SPLASH_FADE_US / 2, 1440, 540, &L);
    CHECK(!L.done && near(L.alpha, 0.5f));
    altr_splash_layout(ALTR_SPLASH_HOLD_US + ALTR_SPLASH_FADE_US, 1440, 540, &L);
    CHECK(L.done && L.alpha >= 0.f && L.alpha < 0.01f);
    altr_splash_layout(100u * 1000000u, 1440, 540, &L);
    CHECK(L.done && L.alpha >= 0.f);

    /* Cluster window 1440x540: the art keeps its size (scale 1) and is centred. */
    altr_splash_layout(0, 1440, 540, &L);
    CHECK(near(L.dw, SPLASH_W) && near(L.dh, SPLASH_H));
    CHECK(near(L.x0, (1440 - SPLASH_W) / 2.f) && near(L.y0, (540 - SPLASH_H) / 2.f));

    /* A small window shrinks it to 80% of the tighter side and keeps it inside. */
    altr_splash_layout(0, 200, 100, &L);
    CHECK(L.dw <= 0.8f * 200 + 0.01f && L.dh <= 0.8f * 100 + 0.01f);
    CHECK(L.x0 >= 0.f && L.y0 >= 0.f && L.x0 + L.dw <= 200.01f && L.y0 + L.dh <= 100.01f);

    /* The dot starts at the top of the orbit, a quarter turn later it is on the right,
     * and a full turn brings it back. */
    altr_splash_layout(0, 1440, 540, &L);
    {
        float cx0 = L.cx, cy0 = L.cy;
        float icx = L.x0 + SPLASH_CX, icy = L.y0 + SPLASH_CY;
        CHECK(near(cx0, icx) && near(cy0, icy - SPLASH_ORBIT));
        altr_splash_layout((uint64_t)(ALTR_SPLASH_TURN_US / 4), 1440, 540, &L);
        CHECK(near(L.cx, icx + SPLASH_ORBIT) && near(L.cy, icy));
        altr_splash_layout((uint64_t)ALTR_SPLASH_TURN_US, 1440, 540, &L);
        CHECK(near(L.cx, cx0) && near(L.cy, cy0));
    }
    CHECK(near(L.r, SPLASH_DOT));

    /* Pre-rendered images for the blitter path. */
    {
        enum { W = 1440, H = 540 };
        static uint8_t sheet[W * H * 4], dot[ALTR_SPLASH_DOT_PX * ALTR_SPLASH_DOT_PX * 4];
        const uint32_t *px = (const uint32_t *)(const void *)sheet;
        const uint32_t *dp = (const uint32_t *)(const void *)dot;
        int ox, oy, i;
        altr_splash_layout(0, W, H, &L);
        ox = (int)(L.x0 + 0.5f); oy = (int)(L.y0 + 0.5f);
        CHECK(altr_splash_render_sheet(sheet, W * 4, W, H) == 0);
        CHECK(px[0] == 0xFF000000u && px[W * H - 1] == 0xFF000000u);   /* black, opaque */
        for (i = 0; i < W * H; ++i) {
            CHECK((px[i] >> 24) == 0xFF);                              /* never transparent */
        }
        {   /* some artwork is lit, and only inside its rectangle */
            int x, y, inside = 0, outside = 0;
            for (y = 0; y < H; ++y)
                for (x = 0; x < W; ++x)
                    if (px[y * W + x] & 0xFFFFFF) {
                        if (x >= ox && x < ox + SPLASH_W && y >= oy && y < oy + SPLASH_H) inside++;
                        else outside++;
                    }
            CHECK(inside > 500 && outside == 0);
        }
        /* too small for the artwork at full size: refused, buffer untouched */
        memset(sheet, 0x7F, 64);
        CHECK(altr_splash_render_sheet(sheet, 200 * 4, 200, 100) == -1 && sheet[0] == 0x7F);
        CHECK(altr_splash_render_sheet(sheet, 100, W, H) == -1);       /* stride too small */

        altr_splash_render_dot(dot, ALTR_SPLASH_DOT_PX * 4);
        {   /* centre solid #5DCAA5, corner clear, premultiplied (rgb never above alpha) */
            uint32_t m = dp[(ALTR_SPLASH_DOT_PX / 2) * ALTR_SPLASH_DOT_PX + ALTR_SPLASH_DOT_PX / 2];
            int j;
            CHECK(m == 0xFF5DCAA5u);
            CHECK(dp[0] == 0u);
            for (j = 0; j < ALTR_SPLASH_DOT_PX * ALTR_SPLASH_DOT_PX; ++j) {
                unsigned a = dp[j] >> 24;
                CHECK(((dp[j] >> 16) & 255u) <= a && ((dp[j] >> 8) & 255u) <= a && (dp[j] & 255u) <= a);
            }
        }
    }

    /* Host build: no GL, so no splash object. */
    CHECK(altr_splash_create() == NULL);
    altr_splash_restart(NULL);
    altr_splash_draw(NULL, 1, 1);
    altr_splash_destroy(NULL);

    if (fails) return 1;
    puts("ok");
    return 0;
}
