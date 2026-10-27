/* lili_demo.c - see lili_demo.h. Coordinates are logical px (480). The walks go through nav.c (the platform
 * calls nav_init first). */
#include "lili_demo.h"
#include "asset_ids.h"
#include "nav.h"

enum { D_WALK, D_TROT, D_DO, D_SET };
typedef struct { uint8_t kind; int16_t x, y; uint16_t anim; int8_t dir; float t; int8_t view, season, daypart; } demo_step_t;
static const demo_step_t DEMO[] = {
    { D_SET, 240, 290, 0, 0, 0, VIEW_ROOM, 2, 1 },                                     /* the room, autumn, afternoon */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 2.5f, -1, -1, -1 },
    { D_WALK, 130, 272, 0, 0, 0, -1, -1, -1 },                                          /* to the rug */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 3.0f, -1, -1, -1 },
    /* the bowls: her eating and drinking frames carry a bowl 15.5 px left of her paws (the room's 72 px set);
       she stands so that its bottom lands on the room's bowl (the room's own steps aside meanwhile, lili_world.c prop_in_use) */
    { D_WALK, 273, 450, 0, 0, 0, -1, -1, -1 },                                          /* east of the food bowl */
    { D_DO, 0, 0, ANIM_EAT_DRINK, DIR_W, 3.5f, -1, -1, -1 },
    /* from one bowl to the other she walks off and comes back round, not sideways across the bowls (the owner, 2026-10-23) */
    { D_WALK, 320, 392, 0, 0, 0, -1, -1, -1 },                                          /* away from the food bowl */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SE, 1.0f, -1, -1, -1 },
    { D_WALK, 392, 451, 0, 0, 0, -1, -1, -1 },                                          /* round to the water bowl's east side */
    { D_WALK, 351, 451, 0, 0, 0, -1, -1, -1 },                                          /* and up to it, heading west */
    { D_DO, 0, 0, ANIM_DRINK, DIR_W, 3.0f, -1, -1, -1 },
    { D_WALK, 300, 400, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },                                          /* towards the door */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_BARK_HEAD_FLIP, DIR_S, 1.8f, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 2 },                                               /* evening: the door's light fades */
    { D_DO, 0, 0, ANIM_IDLE_STAND, DIR_SW, 3.0f, -1, -1, -1 },
    { D_WALK, 135, 262, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, -1, 3 },                                               /* night: only the lamp's short shadows */
    { D_DO, 0, 0, ANIM_SLEEP_CURL, DIR_SW, 4.0f, -1, -1, -1 },
    { D_SET, 236, 456, 0, 0, 0, VIEW_YARD, 0, 1 },                                      /* out to the yard: spring, afternoon */
    { D_WALK, 260, 300, 0, 0, 0, -1, -1, -1 },
    { D_WALK, 404, 246, 0, 0, 0, -1, -1, -1 },                                          /* half into the right bush (spot_bush_2) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.6f, -1, -1, -1 },                          /* peeking out of it */
    { D_WALK, 446, 236, 0, 0, 0, -1, -1, -1 },                                          /* deep in (hide_bush_2): she fades out */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 380, 300, 0, 0, 0, -1, -1, -1 },                                          /* and out again, fading in */
    { D_SET, -1, -1, 0, 0, 0, -1, 1, -1 },                                              /* summer */
    { D_WALK, 250, 250, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PLAY_BOW, DIR_SW, 2.0f, -1, -1, -1 },
    { D_WALK, 130, 210, 0, 0, 0, -1, -1, -1 },
    { D_TROT, 286, 340, 0, 0, 0, -1, -1, -1 },
    { D_DO, 0, 0, ANIM_PANT_SMILE, DIR_S, 2.5f, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 2, -1 },                                              /* autumn */
    { D_WALK, 186, 122, 0, 0, 0, -1, -1, -1 },                                          /* half behind the top bush (spot_bush_3) */
    { D_DO, 0, 0, ANIM_LOOK_AROUND, DIR_S, 2.2f, -1, -1, -1 },
    { D_WALK, 250, 330, 0, 0, 0, -1, -1, -1 },
    { D_SET, -1, -1, 0, 0, 0, -1, 3, -1 },                                              /* winter */
    { D_DO, 0, 0, ANIM_IDLE_SIT_BLINK, DIR_S, 2.5f, -1, -1, -1 },
    { D_WALK, 236, 456, 0, 0, 0, -1, -1, -1 },
};
enum { DEMO_N = (int)(sizeof DEMO / sizeof DEMO[0]) };

static int s_demo_i = -1, s_demo_rounds; static float s_demo_t;

void demo_start(void) { if (nav_ready()) { s_demo_i = 0; s_demo_t = 0; s_demo_rounds = 0; } }
void demo_stop(void) { s_demo_i = -1; }
bool demo_active(void) { return s_demo_i >= 0; }
int  demo_step(void) { return s_demo_i; }
int  demo_rounds(void) { return s_demo_rounds; }

void demo_tick(world_t *w, float dt) {
    if (s_demo_i < 0) return;
    actor_t *l = &w->actors[0]; body_t *b = &w->bodies[0];
    const demo_step_t *s = &DEMO[s_demo_i];
    bool done = false;
    if (s->kind == D_SET) {
        nav_stop(b);
        if (s->view >= 0) { w->view = (view_t)s->view; l->view = (view_t)s->view; }
        if (s->season >= 0) w->season_force = s->season;
        if (s->daypart >= 0) w->daypart_force = s->daypart;
        if (s->x >= 0) { l->x = s->x; l->y = s->y; }
        done = true;
    } else if (s->kind == D_DO) {
        if (s_demo_t == 0) { l->anim = s->anim; l->dir = (dir8_t)s->dir; l->anim_t = 0; }
        s_demo_t += dt; done = s_demo_t >= s->t;
    } else {                                                                 /* the path search and the steering: nav.c (M4.1) */
        if (s_demo_t == 0) { b->style = s->kind == D_TROT ? STYLE_TROT : STYLE_WALK; nav_plan(w->view, l->x, l->y, s->x, s->y, b); s_demo_t = 1e-6f; }
        done = nav_follow(l, b, dt);
    }
    if (done) { s_demo_i = (s_demo_i + 1) % DEMO_N; s_demo_t = 0; if (!s_demo_i) s_demo_rounds++; }
}
