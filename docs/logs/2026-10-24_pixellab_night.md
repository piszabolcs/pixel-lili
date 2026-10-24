# PixelLab night batch log (2026-10-23)

## Rules learned (the hard way)
- Max 3 jobs at once (Tier 1). NEVER queue a 4th: a queued job binds to whatever project tab is ACTIVE when it starts.
- Launch 3 (each in its own new tab, the tab name = the job), wait until the Generations badge is gone, export each tab, then next 3.
- Never click the Generations list's buttons while the Animate panel shows "Generation complete" (its Try again / Reuse seed sit under the list).
- Godot text fields: after typing, WAIT 2 s before clicking another field, or the text lands in the next field.
- The import dialog OK (716,564) may appear late: zoom-check it before going on, else keystrokes become editor shortcuts.
- v3 + "sitting down onto her haunches" / "standing up from sitting" -> a person / a bipedal dog. Transitions skipped; the 8 missing poses go to Gemini (PROMPTS.md section 08) in the morning.
- Export: File (19,12) > Exportálás másként (80,170) > Képsorozat (587,132) > path (735,545) > Exportálás (672,608). The typed name sometimes does not take: the file is then saved as the project name (rename <tag>.png -> <tag>_sheet.png). If __saved does not grow: the receiver popup closed, click the "lili recv" button (find) again, pending files flush.
- PixMiniMax panel: first frame x (173,149) after hovering (147,180), Click to Set (147,180); description (233,300); subject (235,384); starting pose (235,428) (2-line description); scrollbar drag (424,450)->(424,100) top, (424,200)->(424,720) bottom; Generate (235,660); 1 gen per 8 frames at 128.

## Done (assets/pixellab/*_sheet.png, previews docs/img/anim/pixellab_<tag>.gif)
- trot_N (v3, salvaged from a mixed tab) - OK
- tug_rope_S (v3) - OK, white motion lines to strip, ears flip a little
- eat_treat_S (v3) - so-so: smoke specks over the tail, the folded ear swaps side
- carry_trot_S (v3) - poor: no leg motion, a white wisp over the head
- PixMiniMax OK: carry_trot_S_pmm, tug_rope_SW, eat_treat_SW, tug_rope_SE, eat_treat_SE, chew_rope_S, chew_rope_SE, chew_treat_S
- also OK: chew_treat_SE, carry_trot_SW, carry_trot_W, carry_trot_NW
- also OK: carry_trot_NE, carry_trot_E, carry_trot_SE, eat_W, drink_W (laps, tail wag), pee_SW (still, tiny white specks under the tail to strip), poop_SE, poop_SW, idle_stand_S
- If a project tab has empty placeholder frames selected, click frame 1 in the timeline (690,657 with the panel open) BEFORE Click to Set, else the first frame is blank.
- A server error 'QueuePool limit ... timed out' can refuse a start: just retry later on the same tab.
- failed tries kept as *_try1: sit_down_E (person), sit_down_W (person), stand_up_NW (bipedal)

## DECISION (23:40): PixMiniMax for everything
carry_trot_S_pmm and tug_rope_SW (PixMiniMax, subject "a small fawn chihuahua dog" + starting pose) are clearly better than v3:
real leg motion, the folded ear stays, no white wisps; 1 gen each. v3 sheets stay as alternatives.
The server sometimes allows only 2 concurrent (a ghost job): a refused 3rd shows "maximum number of concurrent jobs".
Closing the Animate panel shifts the tab bar left (tabs start at x~55 instead of ~428): click tabs with the panel CLOSED, re-read positions.

## Next (priority)
carry_trot SW/W/NW/NE/E/SE, tug_rope_SE, eat_treat_SE, chew_rope S/SE, chew_treat S/SE, eat_W, drink_W, pee_SW, poop SE/SW,
idle_stand S/W/N/NE/E/SE, walk_E, trot S/SW/W/NE/E, play_bow S/W/NW/N/NE/E/SE, idle_sit_blink SW/NW/N/NE, sleep_curl_S, pet_enjoy S/SW/SE,
run x7, pant x4, bark x7, head_tilt S/SW, look_around SW/SE, ask_pet S/SW, sun_sit S/SW, sniff/sneak/scurry/startle/pounce.
Inputs: scratchpad/in/<tag>.png (start frames under the job name).

## Late night changes
- The server got fast (~3 min per job). Only 2 run at once.
- The editor got sluggish with ~25 project tabs (a screenshot timed out): reloaded in a NEW Chrome tab (98240841), the old tab
  (98240824) could not be closed (a "Leave site?" prompt, beforeunload). After a reload: run the hooks again, click "lili recv",
  decline "restore previous session", pick the hand tool (the pencil is the default: a stray canvas click would draw).
- New viewport is 1568x698: import OK (717,541), Generate (235,615) after dragging the scrollbar (424,200)->(424,690).
  Generations icon (25,585). Advanced options must be ticked once after a reload (80,582) for Subject / Starting pose.
- Close a finished tab: its X, then "Nem mentett kép" Ok (698,393) (no save).
- Seed: the field keeps the last seed after a run; set 0 (random). Return in the seed field adds a newline: BackSpace it.

## Done after the reload (all PixMiniMax, 1 gen each)
- idle_stand S W N NE E SE (N: the folded ear swaps side in a few frames)
- play_bow S W NW N NE E SE (owner's prompt; back views without "happy face"): legs stay planted, tail and rear wag; S is the best
- trot S SW W NE E (S bobs up and down a lot: re-anchor the feet later), walk_E and idle_sit_blink_SW running
- Flow that avoids accidents: start 2 back to back, wait for both, export both, close both, then the next 2.
  If a job finishes while I type into the form, the panel flips to "Generation complete" and the letters become shortcuts
  (the tool switched to the pencil once, no pixels changed). Check the tab for (*) before using its canvas.
- 01:50: also done: walk_E, idle_sit_blink SW NW N NE, sleep_curl_S, pet_enjoy S SW SE, pant_smile_SW, head_tilt S SW,
  look_around SW SE, ask_pet S SW, sun_sit S SW, bark SW W N NE E SE. Running: run S SW.
- Skipped on purpose: pant_smile NW / N / NE (from behind they would be the same as idle_sit_blink NW / N / NE).
- Keep the description to 2 lines: a 3-line one pushes Subject / Starting pose down and the typed text lands in the description.
- The server slowed after ~01:15 (5-6 min per job).
- 02:15 BIG: PixMiniMax does transitions right. sit_down_S and sit_down_E: a real dog sits down, no person, no bipedal.
  sit_down_E's last frame became assets/startframes/sit_E.png (the "makes" rule) -> idle_sit_blink_E and pant_smile_E inputs made.
  So the missing poses (sit W/E/SE, stand NW) can come from PixMiniMax transitions; Gemini sheet 08 may only be needed for
  carry N, lie_rope SW, lie_treat SW, pee SE.
- 02:30: sit_down W SE also good -> sit_W, sit_SE made. stand_up_NW good (stands on 4 paws at the end) -> stand_NW made.
  Inputs made for idle_sit_blink W/SE, pant_smile W/E/SE, head_tilt_SE, ask_pet_SE, idle_stand/trot/run/bark/walk NW.
- Caught one mistake before it cost anything: the first-frame swap silently failed once (old W pose stayed). Always zoom the
  First frame thumbnail before Generate.
- 03:05 incident (no harm): while I filled bark_NW the run_NW job finished; the panel jumped, my typing went into the canvas as
  shortcuts (ctrl+a = select all, letters = tool switches, the panel ended on "Animation to Animation"). Nothing was generated.
  Fix used: discarded that project tab without saving and re-imported bark_NW clean. Rule: if a tab shows (*) before its job,
  discard and re-import; never press Generate unless the zoom shows the PixMiniMax form with the right fields.
- 03:40: Click to Set (new first frame) CLEARS Subject and Starting pose: always retype both after it. Generate closes the panel;
  the Generations icon (25,585) shows a badge with the running count. sneak_S, sneak_SW done (SW: steps, not very crouched).
- 04:05: sneak all 8 done (steps in place, only slightly crouched). The server is fast now (~1.5-2 min per job).
- scurry try1 (S, SW) with "scurries away quickly in place": the dog TURNS AROUND and runs off (S ends in a back view, SW turns
  to the right). Kept as scurry_S_try1 / scurry_SW_try1. Retry: no "away", "keeps facing the same direction".
- 04:20: scurry all 8 done with "the dog scurries in place, fast low steps, ears flat, tail tucked, does not turn around":
  keeps facing, quick steps, ears go flat in some directions (W, NE). Tail stays up mostly.
- 04:35: startle S, SW good (hop up, then settle). startle W and NW turned to face the viewer (head and body swing
  to S/SE): kept as *_try1, retrying with "seen from the side / from behind ... keeps facing away".
- 04:45: startle_NE also turned to the viewer (try1 kept). Retry startle_NW with "seen from behind, the dog flinches and hops
  up in place, startled, ears up, keeps facing away": WORKS (stays a back view, hops up). Side views get "seen from the side
  ... keeps facing left/right".
- 04:55: startle all 8 done (W, NE retried with the side/behind wording; first tries kept as *_try1). pounce_S (small paw
  bounce) and pounce_SE (a real crouch-lunge-land) done. A start was refused once with "maximum number of concurrent jobs"
  while only one job ran: the refused tab gets empty placeholder frames, discard it and re-import.
- 05:10: pounce all 8 done (front: "the dog pounces forward playfully in place with both front paws, then lands, tail wagging";
  side: "seen from the side ... keeps facing left/right"; back: "seen from behind ... keeps facing away"). SW, SE, W, E are real
  crouch-lunge-land jumps (E drifts right a little: re-anchor later); S is a small paw bounce; back views rise up and land.
- 05:35: sit_down SW NW N NE done (back views with "seen from behind ... keeps facing away": all sit down correctly).
  lie_down SW SE done (SE great; SW: when lying, the upright ear also folds down: both ears look floppy, check).
  squat_pee_SE done (lowers the rear into a squat; last frame -> startframes/pee_SE.png) and pee_SE done (tiny movements).
  pant_smile NW/N/NE generated after all (back views, "seen from behind, the dog sits and pants happily, quick breathing,
  keeps facing away", starting pose "sitting on her haunches"). One job (pant_smile_NW) ran much slower (50% after 4 min).
