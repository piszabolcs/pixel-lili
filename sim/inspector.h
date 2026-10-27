/* inspector.h - the simulator's right-hand panel (spec 6.3): buttons that push the same
 * events a finger, the IMU or the microphone would, the world's state at a glance, and a
 * console that prints every event that reaches the world. Simulator only (LVGL widgets):
 * nothing here is compiled into the firmware. */
#ifndef LILI_INSPECTOR_H
#define LILI_INSPECTOR_H
#include <stdbool.h>
#include "lvgl/lvgl.h"
#include "lili_world.h"

#define INSPECTOR_W 360

/* build the panel at x = LILI_PHYS_W on the active screen; buttons push into `q`, the
   Debug button flips *debug, the cycle buttons read the world */
void inspector_init(world_t *w, ev_queue_t *q, bool *debug);
void inspector_bind_canvas(lv_obj_t *canvas);       /* the mouse on the picture is the finger (inspector_finger) */
bool inspector_finger(int *x, int *y);               /* the finger is down on the picture, and where (logical px) */
void inspector_log(const char *line);                /* one console line (newest at the bottom, the last 9 kept) */
void inspector_update(float fps, float frame_ms);   /* refresh the state readout (call a few times a second) */

#endif
