# Állapot

Frissítve: 2026-10-24 (reggel)

## Kész
- **M0.1** Fejlesztői környezet: WSL2 Ubuntu 24.04.5 a `D:\WSL\Ubuntu-24.04`-en (tükrözött hálózat, systemd), build-essential (gcc 13.3), SDL2 2.30, cmake 3.28, ninja 1.11, ffmpeg 6.1, uv 0.13; ESP-IDF **v5.4.1** (`~/esp/esp-idf`, `install.sh esp32s3`, `idf.py --version` OK); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14); `.venv` Python 3.11 + **torch 2.11.0+cu128**, `torch.cuda.is_available()` = True, sm 12.0. Windowson: usbipd-win 5.3, uv, VS Code WSL bővítmény.
- **M0.2** A repó a pocket-tank `a3184fc` klónja, `upstream` remote beállítva; `CLAUDE.md`, `PIXEL_LILI_SPEC.md`, `docs/`, `reference/` bent. **Helye: `C:\Users\kaposzta\Desktop\Project\Lili_project\pixel-lili`** (WSL-ből `/mnt/c/Users/kaposzta/Desktop/Project/Lili_project/pixel-lili`). A gazdi döntése (2026-10-18): **GitHub nincs**, a repó csak helyi; a félig létrehozott üres privát `piszabolcs/pixel-lili` repó érintetlen, bármikor törölhető. A WSL-ben csak a fordító környezet él: `~/esp/esp-idf`, `~/dev/lili-venv` (PyTorch), `~/dev/pocket-tank` (referencia-klón).
- **M0.3** `docs/UPSTREAM.md`: a 10 kötelező pocket-tank doksi és a kulcsforrások összefoglalója (átvett modulok fájl- és függvényszinten, mért számok, build/flash, memóriakeret, mentés, LLM pipeline, Director parancsok, hang, kihagyott részek, 17 buktató).
- **M0.4** `make SQUARE=1` -> `fishsim-square` fordul; `make SQUARE=1 check` zöld (selftest, selftest-llm, selftest-saves, selftest-update). Headless pillanatkép: `--snapshot` PPM-eket ír (ffmpeg-gel PNG-vé alakítható).

## Kész (M1)
- **M1.1** gyári flash mentve (`Lili_project\backup\factory_216_2026-10-18.bin`), **M1.2** pocket-tank square fut a panelen, **M1.3** gombok (csak PWR él a pocket-tank alatt; BOOT = GPIO 0, KEY3 = GPIO 18).
- Diag firmware: `firmware/diag` (build: `bash tools/build_diag.sh` WSL-ben, flash: `py -3.12 tools/flash_216.py diag`, konzol: `py -3.12 tools/serial_log.py`). Vissza a pocket-tankra: `py -3.12 tools/flash_216.py tank`.
- **M1.4** QMI8658 INT1 = GPIO 17, a WoM 338 s alvás után felemelésre ébresztett (ébresztés az INT1 aktuális szintjének ellentétére).
- **M1.6** mikrofon (frekvencia 1 Hz-en belül), füttydetektor v3: két címkézett felvételen 24/24 fütty, 0 téves (`tools/whistle_sim.py`).
- **M1.7** CORNER_R = 72 fizikai px (36 logikai).
- **M1.8** RTC beállítás (`tools/rtc_sync.py`) és megtartás PMIC-kikapcsolás alatt.

- **M1.5** fogyasztás: ACTIVE ~80-90 mA, DIM ~40 mA, könnyű alvás 13-20 mA (stats.md); DESK türelmi idő javaslat: 8 óra, utána kikapcsolás (decisions.md). **Az M1 ezzel kész.**
- **M2.1-M2.4 (szimulátor):** `make LILI=1` -> `lilisim`: közös `common/lili` mag, 8 bites renderelő (LUT, 2x flush), kávamaszk (72 px sarok, vignetta), szoba és udvar `assets/maps/*.json`-ból, a gazdi szőnyegével, Y-rendezés, debug réteg (M).

- **M2.5:** esemény-sor (17 típus) és inspector panel a szimulátorban (`docs/img/m2_5_inspector.png`).
- **M2.6:** a Lili-világ saját firmware-ként fut a 2.16-on (`firmware/lili`, `tools/build_lili.sh`, `flash_216.py lili`): sávos flush keretpuffer nélkül, a statikus réteg gyorsítótárból; **62,5 fps** (cél >= 25), belső heap 270 KB szabad (stats.md). Az **M2 ezzel kész.** A panel billentyűi: KEY3 = szoba/udvar, BOOT = következő évszak (az érintés az M4-ben jön).

## Folyamatban
- **RTC drift:** `py -3.12 tools/rtc_sync.py --check` (beállítva 2026-10-19 17:23:09).

## Következő
- **M3.1 kész:** `tools/blender/lili_common.py` (a kamera, a fény, a toon anyag), `scene_setup.py` (kollekciók, 7 pontos kamera-ellenőrzés, 8 irányú helyőrző kutya), `tools/blender_run.py`, `tools/contact_sheet.py`; `docs/img/m3_1_check.png`. Élő Blender: `mcp-for-blender` 2.2.2 telepítve (telemetria tiltva), `tools/blender_live.py` (jelenet, nézetkép, kód futtatása a nyitott Blenderben); `docs/img/m3_1_live_viewport.png`.
- **Célképek jóváhagyva (2026-10-21):** `reference/concept/` (szoba, Lili karakterlap); spec 7.1 átírva hi-bit pixel artra; az ajtó/ablak/óra/képek és a felfelé lapozás döntései a decisions.md-ben.
- **M3.2 kész, jóváhagyva (2026-10-21):** `tools/palette_from_photos.py` -> `assets/palettes/master.gpl`, `docs/img/m3_2_palette.png`; a mag a `PAL_MASTER`-rel indul (`palette_gen.c`).
- **M3.3 kész:** egy parancs (`py -3.12 tools/build_assets.py`) végigviszi: paletta -> Blender jelenet -> 96 kocka render (helyőrző kutya, idle_stand + walk) -> pixelesítés (`tools/pixelize.py`, B7 szabályok) -> kontaktlapok (`docs/img/sheets/`) -> `assets/build/assets.pak` (42,6 KB) + `common/asset_ids.h`. A szimulátor betölti a pakot, Lili a sprite (`docs/img/m3_3_sim_sprite.png`); A = animáció, R = irány. A firmware a pak olvasóval fordul, a pak betöltése a panelen később (partíció, M4 után).
- **Natív 480 (2026-10-21):** a gazdi kérésére a 2x nagyítás megszűnt, a világ natív 480x480-ban rajzol (spec 7.2 és 7.3 frissítve); a panelen 38 fps. A térképek a 240-es tervezőrácson maradtak (`LILI_K`).
- **M3.4 elkészült, jóváhagyásra vár (natív 96 px-es cellával újragenerálva):** `tools/blender/lili_model.py` -> `assets/blender/lili.blend`; `docs/img/m3_4_lili_8dir.png` (8 irány, 4x) és `docs/img/sheets/idle_stand.png` (pixeles). A lánc már a modellből gyárt (pak 57,7 KB).
- **2D vonal (2026-10-23):** a Blender-modell helyett a jóváhagyott Lili-képekből tisztított 96 px-es sprite-ok (`tools/sprite_clean.py`, `assets/sprites/lili_src/`). **Animáció: PixelLab** (a gazdi döntése, decisions.md "animáció / PixelLab"): a szerkesztőből exportált sheetek `assets/pixellab/` (17 db), előnézetek `docs/img/anim/pixellab_*.gif` (`tools/pixellab_import.py`). Kész előnézetként: járás 6 irányból (S, SW, W, N, NE, SE), álló idle (SW), ülő idle (S), lihegő mosoly (S), körülnézés (S), alvás (2. változat), meghajlás (SW), evés és ivás (W, a tál a sprite része), ügetés (SE, a "futás" leírásra), ugatás (S, gyenge, újra kell). **2026-10-23 este: bekötve a szimulátorba** (a gazdi jóváhagyásával): a festett szoba és udvar 4 évszakkal, ajtókilátás, napszakos paletta, fény- és árnyéktáblák, Lili árnyéka, 11 PixelLab-animáció; bemutató videó `docs/img/anim/sim_demo.mp4` (`./lilisim --demo-record DIR`, az ablakban G). A firmware fordul (a pakot a panel még nem tölti be). Hiányzó irányok: NW, E (PixelLab Rotate-tel pótolható); evéshez új animáció kell a szoba táljaival. **Meghajlás v3** (a gazdi promptja, jóváhagyva, 2026-10-23): a mancsok mozdulatlanok, videó `docs/img/anim/sim_play_bow_v3.mp4`. **2026-10-23 este:** Lili a szobában 75 %-os (72 px), az evés és ivás újra a kockák táljával megy (a fej a tálban), a két tál között eltávolodik és visszaballag (videó `docs/img/anim/sim_bowls.mp4`); **a festett világ és a demó a panelen is fut** (a pak az appban, 32-33 fps).
- **PixelLab éjszakai gyártás (2026-10-24):** a `jobs.json` mind a 143 animációja kész (168 sheet a próbákkal, PixMiniMax), 8 irányban: járás, ügetés, futás, álló és ülő idle, meghajlás, lihegés, ugatás, szaglászás, lopakodás, surranás, megriadás, ugrás (pounce), fejbillentés, körülnézés, simiért kérés, napozás, simogatás, alvás, a kötél és a keksz, evés, ivás, pisilés, kakilás, és az átmenetek (leül, feláll, lefekszik, guggol). **Próbáld ki:** nyisd meg a `docs/review/index.html`-t (minden animáció GIF-ként, irányonként), és jelöld, mi mehet be. Napló és minőségi megjegyzések: `docs/logs/2026-10-24_pixellab_night.md`. **Semmi nincs bekötve** a jóváhagyásig.
- **M3.5 [G]** animációk 1. köre (A01-A09) a modellen, `--anim-viewer` a szimulátorban.
- **M3 Blender gyártósor** (a gazdi Blender 5.2.2 LTS-t telepített).
- Később (D-07): "Lili" névfelismerés.

## Próbáld ki (a gazdinak)
- **A demó a panelen (2026-10-23):** bekapcsolás után magától fut a bemutató kör (szoba, évszakok, udvar). BOOT = demó megállítása / újraindítása, KEY3 = szoba/udvar váltás (ez megállítja a demót). Újraflashelés: `py -3.12 tools/flash_216.py lili`. A kép 90 fokkal el van forgatva az óramutatóval ellentétesen (a kábel oldalt jön ki; a gazdi megerősítette, hogy jó irányba fordult).
- **Kezdőkockák (2026-10-23):** `docs/art/startframes/PROMPTS.md`: 7 Gemini-kép (6 lap, mindegyiken 8 póz, és egy kellékes lap), a kész képek ugyanebbe a mappába kerülnek `01.png` ... `07.png` néven.
- **A Lili-világ a panelen:** `py -3.12 tools/flash_216.py lili`, utána a panel a helyőrző szobát mutatja (KEY3 = udvar, BOOT = évszak). Vissza a diagra: `flash_216.py diag`.
- **A panelen (M1.3):** menj végig az üdvözlő oldalon (érintés), nézd meg, hogy úsznak-e a halak, működik-e az érintés és a 3 gomb. Írd meg, melyik gomb mit csinált (M1.3).
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Project/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
