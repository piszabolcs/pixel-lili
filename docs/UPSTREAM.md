# UPSTREAM: mit vettünk át a pocket-tank-ból

| | |
|---|---|
| Forrás | github.com/mediacutlet/pocket-tank (helyi klón: `~/dev/pocket-tank`) |
| Fork commit | `a3184fc9eb052367de21ce88447202fce9ad0e48` |
| Commit dátuma | 2026-10-18 08:55 -0400, "v0.3.8 (alpha): pellets reach the bottom on every board" |
| Verzió | v0.3.8, alpha (`common/version.h`: `PT_RELEASE "0.3.8"`, `PT_RELEASE_STAGE "alpha"`) |
| Licenc | MIT, "Copyright (c) 2026 MediaCutlet" (`LICENSE`). `model/llama2.c/` és `sim/lvgl/` külön klón, saját MIT licenccel (README, "License") |
| Cél panel | Waveshare ESP32-S3-Touch-AMOLED-2.16, a pocket-tank-ban "square", `TANK_SQUARE` |

Minden út a repó gyökeréhez képest. Ami nincs a doksikban: "nincs dokumentálva". `[forrás]` = nem a kötelező doksikból, hanem a forrásfájlból néztem ki.

## 1. Mit veszünk át, hol van

| Modul | Fájlok | Belépési pontok | Mit csinál |
|---|---|---|---|
| Kijelző port (CO5300, square) | `firmware/main/display_port.h`, `display_port_sh8601.c` (SH8601/CO5300 közös driver, a square-ág benne van), `display_port_stub.c`, `board_pins.h` | `display_port_init`, `display_port_flush(fb)`, `display_port_set_brightness`, `display_port_sleep/wake`, `display_port_deep_standby`, `display_port_set_madctl`, `board_is_square()` | RGB565 keret QSPI-n (GPIO 4/5/6/7, clk 38, CS 12, reset 39), soronként, 24 soros csíkokban; az init sorozat a Waveshare BSP-ből (`square_init_cmds[]`) [forrás]. Boot-kor board-felismerés: nincs expander + ES7210 + RTC = óra vagy square; a CST92xx válasza a 0x5A címen dönt |
| Érintés (CST9220, cst9217 család) | `firmware/main/touch_port.h`, `touch_port_ft3168.c` (a név csal: az FT3168 és a CST9217/CST9220 ág is itt van), `touch_port_stub.c` | `touch_port_init`, `touch_port_poll(tank_t*)`, `touch_port_read_raw`, `touch_port_set_mirror/_bias/_log`, `touch_port_deep_sleep`; belül `cst9217_init/_read` [forrás] | CST9220 @0x5A, reset GPIO 40, INT GPIO 11 (pollozva). A nyers jelentés felcserélve és x-tükrözve (`touch mirror 5`), kalibráció `CAL_S_*`. A fájl a pocket-tank oldalak routingját is tartalmazza (`touch_port_shop` stb.), ezt Lilinél cserélni kell |
| AXP2101 | `firmware/main/battery_port.h`, `battery_port_axp2101.c`, `batlog.c/.h`, `common/battery.c/.h` | `battery_port_init`, `_read(&frac,&charging)`, `_state`, `_poweroff`, `_key_poll`, `_set_charge_ma`, `_trim_rails`, `_set_rail`; `board_charge_ma` | Töltésmérés (cache ~1 s), boardonkénti töltőáram (square 500 mA), nem használt sínek lekapcsolása, PWR gomb, PMIC soft power-off. `batlog`: 5 percenként SoC/VBAT napló RTC_NOINIT-ben |
| RTC (PCF85063 @0x51) | `firmware/components/tankcore/rtc_port.h`, `rtc_port_pcf85063.c` (NEM `firmware/main`-ben) | `rtc_port_init(bus)`, `rtc_port_seed(not_before)`; a `main.c` 909. sora hívja | Az óra átvészeli a kikapcsolást; az "eltelt idő" szabályok (éhség) ezt használják |
| IMU (QMI8658 @0x6B) | `firmware/main/imu_port.h`, `imu_port_qmi8658.c` | `imu_port_init`, `_poll(now_us)`, `_inverted`, `_handled`, `_moving`, `_last`, `_sleep/_wake/_power_down` | Fejjel lefelé detektálás (kép flip), mozgás/kézben tartás jelzés |
| ES8311 + ES7210 | `firmware/main/audio_port.h`, `audio_port_es8311.c`, `audio_port_stub.c`, `codec_port.h`, `codec_port.c` | `audio_port_init`, `_play(cue,pitch_q8)`, `_stop`, `_set_volume`, `_sleep`, `_deep_sleep_pins`; `codec_port_init/_down/_mic_adc_down` | ES8311 DAC + NS4150B erősítő (GPIO 46); MCLK a 2.16-on GPIO **42** (az 1.8-on 16). ES7210 boot-kor felfüggesztve, mikrofon nincs használatban |
| Advisor ütemező (Core 1) | `firmware/main/advisor_llm_esp.c/.h`; ask-döntés `common/tank.c` (~2766. sor `state_signature`), konstansok `common/tank.h`; `common/advisor.c/.h` | `advisor_llm_esp_init(model,len,tok,len)`, `advisor_llm_esp(t,fish,request)`, `advisor_llm_esp_stats` | FreeRTOS taszk "advisor", core 1, prio 5, 16384 B stack [forrás]; pending halmaz + egyhelyes postaláda. A "tank" taszk (core 0, prio 4, 12288 B, `main.c`) soha nem blokkol a modellre. Újrakérdezés: `state_signature` változott ÉS `ADVISOR_MIN_INTERVAL 2.0f` s eltelt, vagy `ADVISOR_IDLE_CEILING 45.0f` s óta nem kérdeztük [forrás] |
| Director soros konzol | `firmware/main/director.c/.h`, `tools/director.py` | `director_early`, `director_init`, `director_poll(tank_t*)` (kerettenként), `help()` (`director.c` 189. sor) | USB-Serial-JTAG-on soronként olvasott parancsok, a log ugyanazon a porton. `director.py`: 115200 baud; a DTR/RTS lehúzása RESETELI a chipet. Parancslista: 7. fejezet |
| Flash scriptek | `tools/flash_square.sh`, `tools/flash.sh`, `tools/preflight.py`, `tools/ota_key.sh`, `tools/model_trailer.py`; `tools/boards.local.sh` (nincs a repóban, saját) | lásd 3. fejezet | Build + flash a square-re, a board USB soros száma alapján |
| LLM motor | `common/llm/q4_model.c/.h`, `word_tok.c/.h`, `advisor_core.c/.h`, `firmware/components/llm/CMakeLists.txt` | `q4_model_open/_prefill/_forward/_generate/_bench`; `word_tok_init/_encode/_piece`; `advisor_core_init/_encode/_infer/_last_probs` | 4 bites (GS 64) llama2.c-stílusú motor: int8 aktivációk, fp16 skálák, fp32 normák, RoPE, KV cache, batch prefill; a súlyok mmap-pel a flash-ről. `advisor_core_sample`/`_temp`: mintavétel T=1-gyel vagy greedy |
| `model/` scriptek | `model/gen_traces.py`, `fold_traces.py`, `train_tokenizer.py`, `train.py`, `export_q4.py`, `eval.py`, `prompt_check.py` (megvan), `probe_dist.py`, `teacher_vs_student.py`, `convert_to_v4.py`, `runw.c`, `runq4.c`, `run_v4_overnight.sh`, `schema.md` | lásd 6. fejezet | Teljes desztillációs lánc. A repóban van `model/out/model_q4.bin` (7 557 640 B) és `model/out/tokenizer.bin` (737 B) |
| Sim | `sim/main.c`, `advisor_llm.c`, `persist_port_sim.c`, `Makefile`, `lv_conf.h`, `lvgl/` (v9.2.2), `testdata/saves/` (+ `net_port_sim.c`, OTA, kihagyjuk) | `make SQUARE=1` -> `./fishsim-square`; `make SQUARE=1 check`; `make check-all` | Ugyanaz a `common/` kód SDL2 ablakban. Selftestek (Makefile): `--selftest --selftest-hunger --selftest-shop --selftest-battery --selftest-pop --selftest-card --selftest-fry --selftest-sleep --selftest-tend --selftest-llm --selftest-saves --selftest-update`; `--bench` (renderköltség); `--snapshot <prefix>` (PPM képek); `--fresh --fast N --greedy --narrate --battery N`; `make check-nomodel` |
| `audio.c` + hangbank | `common/audio.c/.h`, `common/sounds.c/.h` (generált), `assets/sounds/*.wav`, `gains.csv`, `sounds.bin`, `tools/make_sounds.py`; host teszt `firmware/host_test/audio_host.c`, `q4_host.c` | `audio_init(bank,n)`, `audio_play(cue,pitch_q8,now_ms)`, `audio_render(out,n)`, `audio_stop`, `audio_set_volume`, `audio_cue_by_name` | Szoftveres keverő, `AUDIO_VOICES 4`, `AUDIO_MAX_PER_S 6`, `AUDIO_MASTER_DB (-12)` a square-en |
| Mentés | `common/progression.c/.h` (SAVE LAYOUT LOCK blokk), `firmware/components/tankcore/persist_port_esp.c`, `sim/persist_port_sim.c` | `persist_port_load(buf,max,&got)`, `progression_save(t)` | Egy NVS blob, ns `tank`, kulcs `save`; lásd 5. fejezet |

Mappák (ls alapján):
- `firmware/main/`: `main.c`, `director.c`, `brightness.c`, `batlog.c`, `model_trailer.c`, `psram_plan.h`, `tokenizer.bin`, `Kconfig.projbuild`, `idf_component.yml`, `CMakeLists.txt`, a portok fent; `update_mode.c`, `net_port_esp.c` (OTA).
- `firmware/`: `partitions.csv`, `sdkconfig.defaults`, `sdkconfig.square` (+ `.round .watch .qemu`), `run_qemu.sh`, `host_test/`, `components/{llm,tankcore}`.
- `common/`: `tank.c/h`, `render.c/h`, `progression.c/h`, `icons.c/h`, `audio.c/h`, `sounds.c/h`, `advisor.c/h`, `battery.c/h`, `notice.c/h`, `setup.c/h`, `update.c/h`, `tank_events.h`, `version.h`, `llm/`.
- `common/llm/`: `advisor_core.c/h`, `q4_model.c/h`, `word_tok.c/h`.
- `model/`: a fenti scriptek + `train_tokenizer_char.py.bak`, `run_v3_overnight.sh`, `run_v3m_mixed.sh`, `out/`.
- `sim/`: `main.c`, `advisor_llm.c`, `persist_port_sim.c`, `net_port_sim.c`, `Makefile`, `lv_conf.h`, `lvgl/`, `testdata/`, `build-square/`, `fishsim-square`.
- `tools/`: `flash.sh`, `flash_square.sh`, `flash_round.sh`, `flash_watch.sh`, `preflight.py`, `director.py`, `ota_key.sh`, `model_trailer.py`, `make_sounds.py`, `gen_icons.py`, `touch_calib.py`, `board_sheet.py`, `pt_boards.py`, `make_installer.py`, `make_ota_manifest.py`, `publish_site_installer.sh`.

## 2. Mért adatok a 2.16-on

| Mérés | Érték | Forrásfájl |
|---|---|---|
| FPS (két fry) | 20.3 - 25.5 fps | `docs/board-amoled-2.16.md` ("First boot") |
| Flush | 15 - 21 ms (fill 12 - 18, a vezeték nem várt) | uo. |
| Szabad belső heap tank futás közben | 38 KB (1.8: ~54, óra: ~45); dirty mask 28.8 KB (1.8: 20.6) | uo. |
| Token/mp | 9.6 - 10.1 tok/s | uo. |
| Döntési idő | 4.25 - 4.44 s, "at the 4.2 s limit's edge" | uo. |
| Döntés, 1.8 board (nem square) | ~3.7 s, 12 tok/s, 25-30 fps | `README.md` ("The numbers") |
| Döntés QEMU-ban | ~2.3-2.5 s, ~19 tok/s, ~28 fps | `docs/stats.md` |
| Host teszt | 83 ms/döntés, azonos döntések 30/30 | `docs/stats.md` |
| Cella | 3.7 V, 1000 mAh (Waveshare opcionális csomag) | `docs/board-amoled-2.16.md` |
| Fogyasztás ébren | 4.3 h, 60% fényerő, 100 -> 66%, tanult 8.5 %/h = ~12 h kijelzőidő, ~79 mA, VBAT 4127 -> 3841 mV | uo. (`docs/batlog/2026-10-12_square-4h-awake.txt`) |
| Kikapcsolva éjszaka | 77% -> 77% 10 h 10 perc alatt, 3936 -> 3968 mV, kb. 1 mA alatt | uo. (`docs/batlog/2026-10-13_square-night-off.txt`) |
| Alvás (deep sleep) a square-en | "Nothing measured" - nincs mérve | uo. ("Sleep") |
| Töltés | 500 mA (0.5 C) az alap; VBAT 4097 mV 300 mA-nél, 4150 500-nál, 4192 700-nál (~0.25 ohm); a lapka 46 C (300 mA) / 51 C (1000 mA); végáram 100 mA | uo. |
| Teljes töltés alacsonyról | nincs mérve ("NOT measured yet") | uo. |
| Érintés kalibráció | két fogás (hüvelyk, mutatóujj), 18-18 érintés, EGY térkép: `CAL_S_*` x gain 1.052, y gain 1.025, offset 13.1 / 6.8; ellenőrzés (hüvelyk): y gain 1.008 offset -0.4, x gain 1.010 offset -7.4. Egy fogás ~7 px eltérés a bal szélső oszlopnál. Az ujj ~20 px-szel lejjebb (`CAL_LAND` + bias), ~8 px-szel a koppintó kéz felé (`CAL_S_LAND_X`) landol; a hüvelyk 14 px-szel messzebb ér a távoli oszlopnál | uo.; `docs/touch/2026-10-12_square-B-thumb.log`, `-B-index.log`, `-B-check.log` |
| IMU nyers (USB lent, hátradöntve) | g = [1166 13033 10739], gravitáció +Y, "fel" = +Y (az 1.8-on -Y) | uo. |
| Modell | 72% egyezés a tanárral (43/60, seed 777; a tanár önmagával 82%); 4 bit vs fp32: 97% cél-egyezés (58/60) | `docs/stats.md`, `README.md` |

## 3. Build és flash parancsok

IDF: v5.4.1 (`docs/bringup.md`; a README "v5.4"). `. ~/esp/esp-idf/export.sh`

```bash
. ~/esp/esp-idf/export.sh
tools/ota_key.sh        # kulcs firmware/keys/ota_signing_key.pem, nem írja felül; KELL a buildhez
cd firmware
idf.py -B build-square -DSDKCONFIG=build-square/sdkconfig "-DSDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.square" build
```
(README, "Run it on real hardware"). `firmware/sdkconfig.square` érdemi tartalma: `CONFIG_POCKET_TANK_SQUARE=y`. A `sdkconfig.defaults` kulcssorai: `CONFIG_SPIRAM_MODE_OCT=y`, `CONFIG_SPIRAM_SPEED_80M=y`, 16 MB flash, QIO 80 MHz, 240 MHz CPU, `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384`, `CONFIG_ESP32S3_DATA_CACHE_64KB=y`, `..._LINE_64B=y`, `CONFIG_POCKET_TANK_DISPLAY_SH8601=y`, `..._TOUCH_FT3168=y`, `..._AUDIO_ES8311=y`, és az OTA/aláírás sorok (lásd 9.).

Modell flash (README, `docs/bringup.md`, `docs/memory_budget.md`):
```bash
esptool.py --chip esp32s3 -p /dev/cu.usbmodem* write_flash 0x290000 ../model/out/model_q4.bin
```

Általános (1.8-ra írt) lépések `docs/bringup.md`-ből: `cd firmware && idf.py build`, `idf.py -p /dev/cu.usbmodem* flash`, `idf.py -p /dev/cu.usbmodem* monitor`. Sikerjelek: `PSRAM plan OK`, `model partition 8192 KB mmap'd, advisor LLM`, `sh8601: panel up`, `touch: FT3168 ready`.

`tools/flash_square.sh`:
- `tools/flash_square.sh` : build + app flash (`write_flash 0xA90000 ota_data_initial.bin 0x10000 pocket_tank.bin`)
- `--build-only`
- `--full` : üres board: `erase_region 0x9000 0x7000`, modell `0x290000`, `python tools/model_trailer.py --check --build-dir "$B"`, trailer `0xA8F000`, majd `write_flash @flash_args` (bootloader, tábla, app)
- `SQUARE_SERIAL` (kötelező, a board USB soros száma, vagy `tools/boards.local.sh`), `TANK_BUILD` (alap `~/.cache/pocket-tank/fw-build-216`)
- belül: `SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.square"`, `python -m esptool --chip esp32s3 -p $PORT -b 460800`; a portkereső `/dev/tty.` -> `/dev/cu.` cserét tesz (macOS feltételezés). Windows/WSL USB-elérés: nincs dokumentálva.
- megköveteli: `firmware/keys/ota_signing_key.pem`, és `--full`-hoz `model/out/model_q4.bin`.

Partíciók (`firmware/partitions.csv`): `nvs 0x9000/0x6000`, `phy_init 0xf000/0x1000`, `ota_0 0x10000/0x280000`, `model 0x290000/0x800000` (raw, subtype 0x40), `otadata 0xA90000/0x2000`, `ota_1 0xAA0000/0x280000`, `storage 0xD20000/0x2E0000`. A modell utolsó szektora (`0xA8F000`) a trailer. A `memory_budget.md` még az OTA előtti táblát írja (factory app 2.5 MB, storage 5.4 MB); a `partitions.csv` az érvényes.

Sim (README): `git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git sim/lvgl`, `apt install libsdl2-dev`, `cd sim && make SQUARE=1 && ./fishsim-square`. QEMU: `cd firmware && ./run_qemu.sh` (stub kijelző/érintés).

## 4. Memóriakeret

Forrás: `docs/memory_budget.md` (512 KB SRAM, 8 MB oktális PSRAM, 16 MB flash); a `firmware/main/psram_plan.h` `assert_plan()` ellenőrzi boot-kor.

| Flash | Méret | Tartalom |
|---|---|---|
| nvs | 24 KB | mentés |
| app | 2.5 MB (`ota_0`) | ESP-IDF + tank + LVGL/display, ~1-1.5 MB használt (doksi) |
| model | 8 MB; `model_q4.bin` = 7.56 MB | 14.3M param, 4 bit, GS 64, fp16 skálák; `esp_partition_mmap`, soha nem másolják RAM-ba |

| PSRAM tétel (448x368-ra számolva) | Bájt |
|---|---|
| framebufferek 2 x 448x368x2 | 659 KB |
| KV cache 2 x 8 réteg x 64 seq x 384 x fp32 | 1.5 MB |
| aktivációk | < 64 KB |
| tank állapot | ~10 KB |
| LVGL heap (ha be van kapcsolva) | ~256 KB |
| tartalék (asserted) | 2 MB |
| összesen | ~4.5 MB a 8-ból |

A square 480x480 framebufferére nincs dokumentált szám. Saját számolás (nem a doksiból): 2 x 480 x 480 x 2 = 921 600 B.

Belső SRAM: FreeRTOS, taszk stackek (tank 12 KB, advisor 16 KB), hot-loop scratch; "no large SRAM residents" (`memory_budget.md`). Az `advisor_llm_esp.c` szerint az int8 aktivációs pufferek (~70 KB) belső SRAM-ban vannak (`q4_fast_alloc`), hogy a dot kernel csak flash súlyokat és SRAM aktivációkat streameljen [forrás]. Square-en tank futása közben **38 KB** szabad belső heap.

Szűk keresztmetszet:
- Inferencia: flash-cache sávszélesség; minden token egy teljes menet a 7.56 MB súlyon. `memory_budget.md` "Latency model": 40 MB/s -> ~5 tok/s (~9 s/döntés), 80 MB/s -> ~10 tok/s (~4.6 s), 160 MB/s -> ~21 tok/s (~2.2 s). A square 9.6-10.1 tok/s a 80 MB/s sorhoz illik. Emeltyűk: batch prefill (megvan), ESP-DSP SIMD + kétmagos matmul, kisebb (8.8M, 4.7 MB) modell.
- Belső SRAM: 38 KB szabad.
- Lili különböző modellméretéhez új keret: nincs dokumentálva.

## 5. Mentés (NVS blob, csak a végén nő)

- Namespace `tank`, kulcs `save`, `SAVE_MAGIC 0x50544b32u` (új magic = minden mentés elveszik).
- NVS partíció 0x9000-en, 0x6000 B = 6 lap; 1 lap szabad a GC-nek, 5 x 126 = 630 entry használható; `nvs_set_blob` az újat írja, utána törli a régit, és ott a director parkolt "bk" másolata is (`progression.c`, "NVS budget").
- `save_t` jelenleg >= 1912 B, `fish_save_t` 68 B, max 6 hal (`N_FISH_MAX`). Régebbi méretek, mind betölthetők: 448, 1112, 1304, 1408, 1432, 1440, 1456, 1480, 1608, 1616, 1624, 1640, 1656 (`sim/testdata/saves`). Új mező csak a végére, saját `SAVE_AT(...)` + `_Static_assert(sizeof(save_t) >= ...)` párral; meglévő sort szerkeszteni tilos.
- Betöltés (`persist_port_esp.c`): hosszabb (újabb build) blobból az első `max` bájt töltődik; ha a blob létezik, de nem olvasható, a mentés LETILTVA (`s_save_blocked`), hogy a tárolt tank ne íródjon felül. A `saved_release` mező a `PT_RELEASE_NUM`-ot tárolja.
- Lilinek: a mezők mások lesznek, a minta (magic, tail-only növekedés, static assert zárak, blocked-save védelem) átvehető.

## 6. LLM pipeline

Forrás: `docs/pipeline.md`, `model/schema.md`, `docs/stats.md`, `model/export_q4.py`, `model/train_tokenizer.py`.

Tokenizer: szószintű, zárt szókészlet, egy token = egy szóköz-határolt szó. v4 sémánál vocab **54** (`train_tokenizer.py --schema 4`); id 0 `<unk>`, id 1 BOS `"\n<s>\n"`, id 2 EOS `"\n</s>\n"`, id 3.. a szavak vezető szóközzel. Kódolás C-ben sima split-and-lookup (`word_tok_encode`), nem BPE. A firmware a `firmware/main/tokenizer.bin`-t ágyazza be (`EMBED_FILES`). Sémafelismerés: ` bored` token a tokenizerben = v4, ` trust` = v3 (`advisor_core.c`). Tanító dokumentum: `<BOS>state -> goal` (~46 token); inferencia: prompt `state ->`, a modell kiegészíti, BOS-sal áll meg.

Prompt (v4 állapotsor, egy sor, fix mezősorrend, `model/schema.md`):
```
zone <1-6> hunger <0-9> energy <0-9> stress <0-9> curiosity <0-9> bold <0-9> social <0-9> stage fry|juv|adult|elder trust <0-9> bored <0-9> food <dist> <clock>|none friend <dist> <clock>|none bubble <dist> <clock>|none reef <dist> <clock>|none wall <dist> <clock>|clear last <goal> time day|night
```
Távolság `none/near/mid/far`, irány óraállás `1`-`12` a haladási irányhoz képest. A séma "FROZEN": mezősorrendet, szókészletet nem szabad változtatni újragenerálás és újratanítás nélkül.

Kimenet: `<goal> urgency <0-9>`; goalok: `seek_food flee_shadow visit_bubbles follow_friend explore rest dart_play inspect_reef`. Tolerans parser az eszközön: ismeretlen goal -> előző marad, hiányzó urgency -> 5. A `flee_shadow` token v4-ben bent maradt, de sosem címke.

Lili-szempont: más mezők és goalok -> új séma, új tokenizer, újragenerált adat; a "séma + tokenizer + traces együtt" elv átveendő.

Tanító lánc (`docs/pipeline.md`):
```bash
python3 -m venv .venv && .venv/bin/pip install torch
git clone https://github.com/karpathy/llama2.c model/llama2.c
cd model/llama2.c && make run && cd ../..                # 1. native llama2.c build
python3 model/gen_traces.py --count 10 --dry-run         # 2a. minta állapotok, hálózat nélkül
python3 model/gen_traces.py --count 3000 --max-minutes 45 --seed 1 \
    --out model/out/traces_x1.jsonl                      # 2b. tanár-címkézett trace-ek (több worker, külön seed/out)
python3 model/fold_traces.py                             #     dedup + szókészlet-ellenőrzés + címke-eloszlás
python3 model/train_tokenizer.py                         # 3. szószintű tokenizer (v4: --schema 4)
.venv/bin/python model/train.py --data model/out/clean.jsonl \
    --dim 384 --n-layers 8 --n-heads 8 --max-seq-len 64 \
    --batch 64 --iters 4000 --lr 6e-4                    # 4. tanítás
.venv/bin/python model/llama2.c/export.py model/out/model.bin --checkpoint model/out/ckpt.pt   # 5. fp32
python3 model/export_q4.py                               #    -> 4 bites model_q4.bin
cc -O3 -o model/runw model/runw.c -lm                    # 6a. szavas futtató
./model/runw model/out/model.bin -z model/out/tokenizer.bin -t 0 -i "<state line> ->"           # 6b.
python3 model/eval.py --count 60 --teacher               # 7. egyezés a tanárral
```
Megjegyzések: a `pipeline.md` az 5. lépésben `export_q4.py`-t argumentum nélkül írja, a script docstringje szerint viszont `export_q4.py out/model_q4.bin --checkpoint out/ckpt_v2w.pt` (a `filepath` és `--checkpoint` kötelező) - ellentmondás, ellenőrizni kell. A v3 körhöz `--schema 3` és `POCKET_SCHEMA=3 train.py`; v4: `--schema 4`, runbook `docs/retrain-v4.md` (ezt nem olvastam). `model/prompt_check.py`: 7 perces tanár/prompt sanity check, **minden hosszú generálás előtt futtatni**. `probe_dist.py`: a student teljes goal-eloszlása; `teacher_vs_student.py`: egymás melletti döntések.

Modell fájlformátum (`model/export_q4.py` fejléc):
- 256 bájtos fejléc: magic `"ak42"`, version=3, 7 config int, shared_classifier u8, group_size i32, nullákkal feltöltve.
- fp32 normák (att, ffn, végső), majd tensoronként (emb, wq*, wk*, wv*, wo*, w1*, w2*, w3*[, wcls]): `uint8 nibbles [numel/2]`, majd `fp16 scales [numel/GS]`. Nibble: 2i -> alsó, 2i+1 -> felső, érték q+8 (0..15, q = -8..7). **GS = 64**, szimmetrikus kvantálás (skála = max|w|/7).
- Méret: 57 MB fp32 -> 15.2 MB int8 -> 7.56 MB 4 bit. Config: dim 384, 8 réteg, 8 fej, seq 64, 14.3M param.

Modell trailer (`firmware/main/model_trailer.h`): a modell partíció utolsó szektora (0xA8F000, 0x1000 B), a törzs UTÁN írva, törléskor ELŐSZÖR törölve. `model_trailer_t`: `magic 0x54524d50` ("PMRT"), `version` (1), `model_len`, `sha256[32]`, `tag[16]`, `crc` (FNV-1a). Hiányzó trailer + a build által ismert SHA256 (`PT_MODEL_SHA256`) -> a firmware boot-kor beírja; ismeretlen modell -> fut tovább és logol. Trailer nélkül (pl. kábeles telepítés) a tank szabálymódban fut.

Tanár: `gemma4:26b` (Q4_K_M, ~18 GB) Ollama-n át; `--host`/`HOST`, alap `http://localhost:11434`. A `gen_traces.py` strukturált kimenetet kér (`{"goal": <enum>, "urgency": 0-9}`) és ebből rendereli a cél-sort. Adatgenerálás: ~36-51 döntés/perc 2-3 workerrel (Mac Mini M4 Pro), ~19 500 pár 8 órás éjszaka alatt. v4 ciklus: 23 366 új + 28 247 konvertált = 51 613 pár, 5000 iteráció, best val loss 0.791. Tanítás ~30 perc a 14M modellre Apple MPS-en. Tanulság ("prompt is DNA"): egy mondat a tanár promptban 45%-ról 14%-ra vitt egy célt (`docs/stats.md`).

## 7. Director soros parancsok

A `docs/DEVICE.md` NEM sorolja fel a parancsokat (flash szabályok + folyamatban lévő mérések). A lista a `firmware/main/director.c` `help()`-jéből van [forrás]. Használat: `tools/director.py <parancs>` (`-p PORT`, `-t SEC`, `--watch`).

- Állapot: `help`, `state`, `batlog [clear]`, `clock`, `codec`, `imu`
- Drive-ok: `hungry [N] [level=9]`, `fed [level=1]`, `set <hunger|energy|stress|curiosity|trust> <0-10> [hal]`
- Etetés: `feed [n=3] [x]`, `trickle on|off`, `pellets clear`
- Környezet: `algae <steps|clear>`, `veg <bed 0-2|all> <0.03-1>`, `light`, `auto`, `sleep <hours>`, `save`
- Staged tank: `fresh`, `stages`, `stage <hal|all> <fry|juv|adult|elder>`, `stash`, `restore`, `age <hal> <órák>`
- Oldalak: `milestones [off]`, `shop [off]`, `settings [off]`, `setup [off]`, `kbd [wheel|grid|pages]`, `battery page [off]`
- Bolt/gazdaság (halas, Lilinél nem kell): `dollars`, `buy`, `place`, `coral`, `cluster`, `sell`, `fry`, `fish`, `order`, `pumpkin`, `seahorse`
- Reset: `reset` (kérdés), `reset yes|no` (a YES MINDEN MENTÉST TÖRÖL)
- Fényerő: `bright <0-255>` (nem mentett), `level 100|60|30` (mentett)
- Akku/energia: `battery <pct> [charging|full|plugged]|real`, `charge [mA]`, `pmic`, `pmic on|off <sín>`, `pmic trim`, `sleepcfg [mask]`, `deepsleep [N]`, `poweroff`, `keytime [N]`, `pwrpin`
- Érintés: `touch [bias <px>] [log on|off] [lift <ms>] [said on|off]`, `touch mirror <n>` (bit0 x, bit1 y, bit2 csere)
- Kijelző: `madctl <hex>` (square, élő), `view fit|full` (kerek board)
- Hang: `snd list`, `snd <id>`, `snd off|quiet|normal`, `snd stop <id>`, `snd settle`, `snd idle <n>`, `snd battery`
- Szimulált események: `overgrown`, `court`, `arrive`, `name <hal> <név>`
- Hálózat/OTA (kihagyjuk): `wifi`, `wifi set ssid=.. pass=..`, `wifi forget`, `ota`, `ota check|url|tap|calib|kbd|flip|page`, `improv`

## 8. Hang (`docs/AUDIO.md`)

- Készülék-formátum: **16 kHz, mono, 16 bit signed PCM** (32 KB/s); ~25 cue, ~0.4 s átlag ~300 KB; az első batch ~600 KB, 16 kHz s16. A `sounds.bin` `EMBED_FILES`-szel a flash-mapped `.rodata`-ba kerül, nincs RAM-költsége.
- Forrás: WAV (előnyben), AIFF vagy FLAC; név a doksi szerint `snd_<event>[_<n>].wav` az `assets/sounds/`-ban, az 5a szakasz viszont `sound-<cue>.wav` és `_v2` revíziót ír, a repóban pedig `eat.wav`, `feed.wav` stb. van - elnevezési ellentmondás, a `tools/make_sounds.py` fejléce az irányadó.
- `tools/make_sounds.py`: `import <dir>` (mono, 600 Hz felüláteresztő - a 12x10 mm hangszóró alsó határa -, 7 kHz aluláteresztő, 16 kHz resample, -60 dBFS alatti csend levágva, 5 ms fade, -1 dBFS csúcsnormalizálás; loopnál 20 ms egyenlő teljesítményű crossfade), majd `build` (`assets/sounds/*.wav` + `gains.csv` -> `sounds.bin` + `common/sounds.h` + `sounds.c`). Példa a fejlécből: `~/.venvs/pocket-tank/bin/python tools/make_sounds.py import "<exports mappa>"`.
- Hangerő: `0 off, 1 quiet (-12 dB), 2 normal`; NVS `tank/snd`; alap normal; settings oldalon, `snd off|quiet|normal`-lel vagy a sim `V` billentyűjével. `AUDIO_MASTER_DB`: -6 (1.8), **-12 a square-en** (-6-nál "a little loud, slightly blown out").
- Keverő (`common/audio.c`): a doksi "legfeljebb 3 hang", a `audio.h` `AUDIO_VOICES 4`; int32 akkumuláció, clip, 5 ms fade-in/out, pitch fixpontos lépéssel (`AUDIO_PITCH_ONE 256`), egy loop hang (`bubbles_loop`), globális limit 6 cue/mp (a többit eldobja, nem sorba állítja), cue-nkénti cooldown. Lejátszó taszk: core 1, prio 6, DMA 2 x 10 ms; a tank taszk csak egy eseményt tesz a gyűrűbe, hang soha nem dobhat frame-et.
- Energia (kötelező szabályok): az erősítő (GPIO 46) csak lejátszás alatt + 2 s idle ablak; alvás előtt kodek teljesen le és ALDO1 ki (`audio_port_sleep()`); költségvetés < 2% az ébrenléti fogyasztásból; kodek ~4 mA játszás közben, erősítő ~3 mA üresjáratban. Felébresztés: ALDO1 be -> ES8311 -> I2S nullák ~20 ms -> GPIO46 magas; cél < 60 ms. Kikapcsolt kodek regiszterek: `0x00=1F 0x01=00 0x0D=F8 0x12=02` (`codec_port.c`).
- Pinek a 2.16-on (`board-amoled-2.16.md`): DSDIN 8, BCLK 9, LRCK 45, MCLK **42**, amp 46; ES7210 @0x40. Az AUDIO.md pin-táblája az 1.8-é (MCLK 16).
- AUDIO.md állapota: "BUILT, NOT FLASHED" (2026-09-15), a kodek init padon nem ellenőrzött; a 2.16 doksi nyitott pontja: "the other cues by ear over days".

## 9. Amit NEM veszünk át, és miért

| Elem | Fájlok | Ok |
|---|---|---|
| OTA | `firmware/main/update_mode.c/.h`, `net_port_esp.c`, `common/update.c/.h`, `docs/OTA.md`, `tools/make_ota_manifest.py`, `sim/net_port_sim.c`, `ota_1`/`otadata` partíciók | Lili nem frissít hálózaton. Figyelem: a `firmware/main/CMakeLists.txt` `REQUIRES` listája `esp_wifi esp_netif esp_http_client esp_https_ota app_update mbedtls ...`-t húz, és a `-u pt_board_marker` link flag a `net_port_esp.c`-re épül; a `sdkconfig.defaults` OTA-sorai (`BOOTLOADER_APP_ROLLBACK_ENABLE`, `SECURE_SIGNED_APPS_*`, `SECURE_BOOT_SIGNING_KEY`) - ezeket a Lili build-ből ki kell szedni vagy a kulcsot meg kell tartani (saját javaslat) |
| Wi-Fi, Improv | `net_port_esp.c`, `director.c` `wifi*`/`improv*`/`director_provision_*` | ugyanaz; a `director_early` is az Improv handshake miatt van |
| Böngészős telepítő | `installer/`, `tools/make_installer.py`, `publish_site_installer.sh`, `pt_boards.py` | nem kell; kábeles flash marad |
| Halas logika | `common/tank.c`, `render.c`, `progression.c`, `icons.c`, `setup.c`, `notice.c`, `assets/icons/`, bolt, oldalak | Lili kutya; cseréljük. A `tank.h` API-minta (TANK_W/H, PAGE_X/Y, `tank_emit`) hasznos referencia |
| Más boardok | `sdkconfig.round/.watch`, `flash_round.sh`, `flash_watch.sh`, kerek/óra ágak a portokban | csak a 2.16 kell; az ágak a forrásban maradnak, ha nem vágjuk ki |
| Közösségi portok | knoopx (ESP32-P4), lmoiseichuk (CYD ES3C28P) | külön forkok, nem ennek a repónak a része |

## 10. Buktatók, amiket a doksik említenek

1. **`tools/ota_key.sh` kell a buildhez**: `sdkconfig.defaults` aláírt binárist kér (`firmware/keys/ota_signing_key.pem`); kulcs nélkül `flash_square.sh` kilép ("no firmware/keys/ota_signing_key.pem - tools/ota_key.sh"). README: minden build aláírt, az első buildhez kulcs kell.
2. **Érintés-offset**: az ujj ~20 px-szel lejjebb, ~8 px-szel a koppintó kéz felé landol; a két fogás 14 px-szel különbözik a távoli oszlopnál; egy térkép (`CAL_S_*`), ~7 px maradék a bal szélen. A tükrözés (`touch mirror 5`) a MADCTL-hez kötött: 5 (0xA0), 6 (0x60), a kerek boardon 3.
3. **MADCTL**: a felfelé álló kép `0xA0` (USB a talpnál). 0x60-nál USB fent; az update mód, ami nem olvassa az IMU-t, fejjel lefelé mutatta a kereszteket. Director: `madctl <hex>`.
4. **PWR gomb**: a doksi szerint rövid nyomás = alvás, 1.5 s = kikapcs, "both tried on the glass", ugyanakkor az "Open" lista a "PWR key (sleep / wake / power-off)" és "a night's draw" pontot nyitottnak írja - a doksin belül ellentmondás, padon újra kell tesztelni. Mélyalvásból (README) csak ~1/8 s-ig tartott nyomás ébreszt, gyors koppintás nem. A square alvása: "Nothing measured".
5. **IMU tengelyek**: a hardver táblában "axes not read yet: the 1.8's default", a keretküldésnél "axes unverified"; a későbbi mérés szerint fel = +Y (az 1.8-on -Y). Az eszköz-forgatásra a live flip "correctly" követ (Strato).
6. **56 px sávok**: a 448x368 oldalak a 480x480-on PAGE_X 16 / PAGE_Y 56-ra vannak középre téve: felül és alul 56-56 px sáv. "Live with them first" (nyitott kérdés). A halak TANK koordinátában, az oldalak PAGE koordinátában élnek, átmenetkor `PAGE_X/PAGE_Y`-t kell levonni. Biztonságos doboz minden üvegen: PAGE x 56..392, y 60..300 (`docs/BOARDS.md`).
7. **Sarkok**: `TANK_CORNER_R 40` "by the maker's pictures, not measured"; a keret levágta a kártyát és az akku pill-t az 1.8 beállításaival (square: `RENDER_CARD_X/Y` 26/22, pill 44 oldalt, 22 lent).
8. **Belső heap**: 38 KB szabad; minden builddel összevetni a `.bss`-t (`docs/BOARDS.md`).
9. **Döntési idő a határon**: 4.25-4.44 s a 4.2 s-os limitnél. 4 halnál a core ~84%-ban foglalt (13.6 döntés/perc x 3.7 s, `docs/DEVICE.md`); karok: `ADVISOR_MIN_INTERVAL`, `ADVISOR_IDLE_CEILING`, a `state_signature` sávjai.
10. **Régi modell / hiányzó trailer**: az első boot rules módban futott, mert a cache-ből másolt modell régi volt; `flash_square.sh --full` modellt és trailert is ír.
11. **Mélyalvás áram**: `gpio_deep_sleep_hold_en()` híján ~15 mA (rosszabb a 4.7 mA light sleepnél); végül PMIC power-off (20 perc kegyelmi idő) lett; `docs/DEVICE.md`.
12. **Hangerő és amp**: -12 dB a square-en; az amp üresjárati 3 mA-je miatt alvás előtt `audio_port_sleep()` kötelező.
13. **Töltőáram boardonként**: a gyári 100 mA lassú volt (12.2 %/h); square 500 mA, végáram 100 mA. A hub-port 1000 mA-nél sem throttolt (4.82 V).
14. **Tesztek az 1.8-ra hangolva**: két selftest elbukott a square-en (urchin harapás, bed 1 frond-száma). A tesztek az oldal-layout függvényeit használják pixel-literálok helyett (`docs/BOARDS.md`, 3. szabály).
15. **Flash-szabály**: `docs/DEVICE.md` szerint csak `tools/flash.sh`-n át, ami előbb a `preflight.py`-t futtatja és archivál (`docs/batlog/`), mert egy reset egyszer elvitte az éjszakai akkumérést. A `flash_square.sh` ezt kihagyja ("No preflight").
16. **Forrásbeli ellentmondások**: `common/version.h` `PT_MODEL_TAG "v3m"` vs `docs/DEVICE.md` "Model partition: v4m" és a README v4; `docs/bringup.md` az 1.8-ról szól (SH8601 + FT3168, expander, IDF export), nem a 2.16-ról; az AUDIO.md pinjei az 1.8-éi; a `memory_budget.md` régi partíciós táblát és 448x368 puffert ír. Az érvényes: `partitions.csv`, `board_pins.h`, a forrás.
17. A `docs/bringup.md` 4. pontja szerint az I2C lábak 15/14 vagy 14/15 lehetnek (az 1.8-nál); a 2.16 doksi: SDA 15 / SCL 14.

## Módosított átvett fájlok (Pixel Lili)
1. `firmware/main/display_port.h`, `display_port_sh8601.c`: **új függvény** `display_port_flush_gen(fill, ctx)` (M2.6, 2026-10-21) a 2.16-ra: a hívó callbackje tölti a DMA csíkot (24 sor, a vezeték bájtsorrendjében), így nincs 460 KB-os RGB565 keretpuffer. A meglévő függvények nem változtak; a nem-square buildekben üres.
