/* Splash timing and geometry (altscreen_render/splash.c, the GL-free part). */
#include <math.h>
#include <stdio.h>
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

    /* Host build: no GL, so no splash object. */
    CHECK(altr_splash_create() == NULL);
    altr_splash_restart(NULL);
    altr_splash_draw(NULL, 1, 1);
    altr_splash_destroy(NULL);

    if (fails) return 1;
    puts("ok");
    return 0;
}
