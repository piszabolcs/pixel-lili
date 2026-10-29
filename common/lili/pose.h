/* pose.h - how an actor's animation changes (2026-10-28, the owner: "the animations are choppy, you can see they
 * are not matched to each other"). Three things made the jumps:
 *   1. a change of pose was one frame: sitting or lying, she was walking the next moment. Now each animation
 *      starts and ends in a pose (standing, sitting, lying), and a change plays its move first: sit_down,
 *      stand_up (sit_down backwards), lie_down, get_up (lie_down backwards), curl_up into the sleeping ball and
 *      uncurl (curl_up backwards); since 2026-10-29 the head into the bowls and out, the squats for the pee and
 *      the poop and up again, the bend to the treat and lying down with it. The plan and the walk wait for it.
 *   2. she turned in one frame (south to north). Now she turns 45 degrees at a time, POSE_TURN_S apart, and an
 *      animation that lacks her direction (the lie-down has three) starts once she has turned to its nearest one.
 *   3. the gaits ran at their full frame rate while she sped up and slowed down: her paws slid. Now a gait's
 *      animation runs at her speed's part of the gait's speed.
 * Platform-free, no malloc. */
#ifndef LILI_POSE_H
#define LILI_POSE_H
#include "lili_types.h"
#include "pak.h"

#define POSE_TURN_S 0.07f                                       /* the time per 45 degree step of a turn */

void pose_play(actor_t *a, uint16_t anim);                      /* show this animation, after the move to its pose if one is needed */
void pose_snap(actor_t *a, uint16_t anim);                      /* show it now, no move (a scene set up, a test) */
bool pose_settling(const actor_t *a);                           /* a move between poses plays: the plan waits, she does not walk */
bool pose_posed(const actor_t *a, uint16_t move);               /* another animation already has her in the pose the move ends in (sit_down when sitting) */
bool pose_resting(const actor_t *a);                            /* sitting or lying: the pose holds between decisions */
bool pose_lying(const actor_t *a);                              /* lying or curled up */
bool pose_curled(const actor_t *a);                             /* curled up asleep, or curling up */
int  pose_near_dir(uint16_t anim, int d);                       /* the direction it plays in when she faces d (the nearest it has) */
bool pose_turn(actor_t *a, dir8_t to, float dt);                /* one step towards to when it is time; true = facing it */
void pose_tick(actor_t *a, float dt);                           /* the animation's clock, and the next animation when a move ends */
float pose_once_len(uint16_t anim);                             /* a one-shot animation's length in seconds (a there-and-back counts both ways) */
int  pose_frame(const sprite_set_t *set, float t, bool reversed);   /* the frame at t s: once, round, there and back (pak loop 0..3) */

#endif
