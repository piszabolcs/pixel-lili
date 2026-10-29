# Állapot

Frissítve: 2026-10-27

<!-- overview -->
## Áttekintés: mi van kész, mi nincs (2026-10-27, folyamatosan frissítve)

Jelek: **kész** = megvan és a tesztek zöldek; **[G] vár** = elkészült, a gazdi jóváhagyására vagy kipróbálására vár; **hiányzik** = nincs meg, és miért.

| Mérföldkő | Állapot | Megjegyzés |
|---|---|---|
| M0 környezet, fork | kész | |
| M1 hardver validáció | kész | |
| M2 váz (renderelő, maszk, térképek, inspector, panel) | kész | |
| M3.1-M3.4 gyártósor, paletta, Lili | kész | 2D vonal: PixelLab-animációk (decisions.md) |
| M3.5 A01-A09 | kész, [G] vár | `docs/review/m3_5.html`, `./lilisim --anim-viewer`; a panelen 32-33 fps |
| M3.6 A10-A27 | kész, [G] vár | `docs/review/m3_6.html`; 33 animáció a pakban (3,97 MB). **Hiányzik** (nincs PixelLab-sheet): A13 puffogás, A14 felvétel/leejtés, A21 kukucskálás; az A22 (ágy) tárgytalan, szőnyeg van |
| M3.7 szoba, udvar, évszakok, napszakok | kész, [G] vár | `docs/review/m3_7.html` (`--snapshot`). **Hiányzik:** évszakos díszek és a különleges napok díszei (új grafika kell) |
| M3.8 játékok, foltok, pelenka, UI ikonok | részben | **a kötél kész** a gazdi Gemini-kelléklapjáról (`docs/art/Lilik/07.jpg`, 4 állás, `tools/toy_sprites.py`), saját 6 játékszínnel, a szájában is színes; a buborékban is a képe. A plüss visszajött (2026-10-28, a gazdi Gemini-képe, `docs/art/Lilik/09.jpg`); a folt, a kaka, a keksz és a pelenka állapota kép. Még helyőrző: a csillogás, az ikonok |
| M4.1 navigáció | kész | `--selftest-nav`: 32 út ajtó és minden pont között, A*, simítás, kormányzás, hiszterézis |
| M4.2 Y-rendezés, takarók, bokor | kész, [G] vár | a bokor egyszer rezzen (`docs/img/anim/m4_2_bush_shake.gif`); `--selftest-render` referenciaképekkel. **Sarokülő (2026-10-27 este):** a pad nem járható, a kuckó a hátsó rész alatt van, a kivágott pad takarja (alvás, bújás: a szeme kilátszik); `--scene-shots`. A valódi kukucskáló A21 és a padon alvás (ugrás, A22) még nincs |
| M4.3 gesztusok | kész, [G] vár | mind a 7 gesztus, `--selftest-gesture`; a panelen az érintés él, **ujjal kipróbálni** (a soros konzol írja az eseményeket) |
| M4.4 terv-végrehajtó | kész | mind az 53 akció (`model/schema_lili.json`), `--selftest-plan`; az inspectorban akcióválasztó |
| M4.5 reflexek R1-R9 | kész | `--selftest-reflex`, prioritásokkal |
| M4.6 játékfizika | kész, [G] vár | `--selftest-toys`. **2026-10-28:** a kötél nem pörög, nem száll el, nem pattog: rövid ív, egy kis huppanás legfeljebb, rövid csúszás, a levegőben fordul, úgy marad, ahogy leesett; a panelen kipróbálandó |
| M4.7 gondozási koppintások | kész | tál, pelenka, folt, csillogás, az utolsó folt után R1 |
| M5.1-M5.5 igények, pelenka, naptár, távollét, mentés | kész | `--selftest-progress/-calendar/-offline/-save`; a panelen RTC és NVS-mentés |
| M5.6 energiaállapotok | kész, [G] vár | ACTIVE -> 60 mp -> DIM -> 10 perc az asztalon -> kijelző ki; zsebben (30 mp mozgás, függőleges, érintés nélkül) ki, **a felemelés utáni sötét kijelző javítva (2026-10-27 este)**; ujj, gomb, felvétel, fütty ébreszt, 15 perc után üdvözöl; PWR = kijelző ki/be (`motion.c`, `--selftest-motion`). A valódi könnyű alvás (WoM-ébresztés) még nincs; a panelen kipróbálandó |
| M6.1-M6.5 szabálymotor agy | kész | séma, kódoló (C = Python 1000/1000), 46 sablon (H1-H15), életciklus, habozás; P1-P12 mind teljesül; a panelen bekapcsoláskor Lili a saját agyával él (BOOT: demó) |
| M6.6 egy hét élet a panelen | [G] vár | a gazdi szerint "ez Lili"? a viselkedés a `model/teacher_rules.yaml`-ben hangolható |
| M7.1-M7.4 LLM tanítása | kész | 184 ezer pár (`lilisim --dump-traces`), egyik szokás sem 1 % alatt; S, M tanítva az RTX 5060-on (3-4 perc); S: A1 87 %, top-2 93,6 %, P1-P12 mind teljesül (`model/eval_lili.py`). **2026-10-27 este újratanítva az új térképpel** (a kuckó áthelyezése után a régi modell rohangálásba esett): 165 ezer pár, A1 83,6 %, top-2 93,4 %, P1-P12 teljesül, egy óra 12 fajta döntés. A vizualizáló Gradio-felület nem készült el |
| M7.5 B szintű tanár (Ollama) | hiányzik | opcionális |
| M8.1-M8.4 agy az eszközön | kész | S modell, 4 bit, 2,76 MB, sématrailer; C motor: 100 % érvényes válasz, Q4 = fp32 99,2 % a határozott esetekben; a panelen Core 1-en, **1,7 mp/döntés**, 33-37 fps; egy óra élet a modellel a szimulátorban (`--selftest-llm`). A panelen bekapcsoláskor a modell dönt, ha van a partíción, különben a szabálymotor |
| M9.1-M9.2 Lili hangja | hiányzik | [G] a gazdi videói kellenek; a lejátszás (ES8311) a mikrofonnal közös I2S buszon duplex beállítást kér, ezt a hanggal együtt csinálom |
| M9.3 fütty és hang a játékban | kész, [G] vár | a diag füttydetektora a panelen -> R2 (előjön), hangra fejbillentés (R8, a küszöb hangolandó); a panelen kipróbálandó (füttyents!) |
| M9.4 névfelismerés | hiányzik | kísérleti, alapból ki (D-07) |
| M10.1-M10.3, M10.5-M10.7 felület | kész | üveg állapotsáv (idő, akku, Lili 3 mutatója, hang, fogaskerék; 27,7 fps amíg látszik), beállítások lap (fényerő, hang, buborékok, keresési segítség, idő), magyar ékezetes pixelbetű, keresési segítség (mancsnyom), első indulás; `--selftest-ui` |
| M10.4 gondolatbuborékok | kész, [G] vár | 9 ikon a fej fölött 2,5 mp-ig; D-09: a panelen is be van kapcsolva, a beállításokban kikapcsolható; a gazdi dönt |
| M11.1 7 napos próba a panelen | [G] vár | |
| M11.2 egy év a szimulátorban | részben | 447 ezer döntés 13 mp alatt, hiba nélkül, minden évszakban változatos; a különleges napok képei hiányoznak (grafika) |
| M11.3 hibajavítás, README, v1.0 | részben | magyar README képernyőképekkel kész; a v1.0 címke az M11.1 után |

**Kérdések a gazdinak (este):**
1. Jók az M3.5, M3.6, M3.7 oldalak animációi és jelenetei, és a bokor rezzenése?
2. Elkészítsem PixelLabbal a hiányzó animációkat (puffogás, felvétel/leejtés, kukucskálás) és az M3.8 grafikáit (kötél, plüss, foltok, pelenka állapotai, ikonok)? Ez kreditet használ, és mindegyik előbb jóváhagyásra jön.
3. Az RTC-t a konzol `settime` parancsával téli időre állítottam (egy órával sietett).
4. A panelen: koppints Lilire, simogasd (oda-vissza húzás rajta), húzd fel az ujjad az alsó szélről; a soros konzol (`py -3.12 tools/serial_log.py --seconds 60`) kiírja, mit ismert fel. Jó helyen van-e az érintés?
5. Füttyents a panel közelében: Lilinek elő kell jönnie (R2). Rázd meg: megriad és ugat. Tedd le az asztalra 3 mp-re, aztán vedd fel: felébred. A BOOT gomb az állapotsáv, a PWR a kijelző ki/be.
6. A panelen a modell vezérli Lilit (a konzolon `brain rules` a szabálymotorra vált). A gondolatbuborékok maradjanak bekapcsolva a panelen (D-09)?
**A gazdi esti hibalistája (2026-10-27 este), javítva:**
1. Méretugrás (leülés, mozgás, meghajlás, kötél felvétele): a Gemini-lapok eltérő léptéke, lapanként visszaskálázva (`tools/pixellab_norm.py` SHEET_SIZE).
2. Kötél a szájában csak állókép volt: a járás és a hordozó ügetés minden képkockán felülírta egymást (`nav.c`); most animált.
3. A kötél a gazdi generált kötele (`docs/art/Lilik/07.jpg`), a padlón, repülés közben, a szájában és a buborékban; saját játékszínekkel.
4. Gondolatbuborék: kis négyzet a feje fölött, a pöttyök nélkül.
5. Sarokülő: nem sétál rajta, a kuckója alatta van (alvás, bújás), a pad eltakarja.
6. Panel: felemelés után sötét kijelző, gomb nem ébresztette: javítva (`body_216.c`), kézzel kipróbálandó.
7. A kuckó áthelyezése után a modell elromlott (rohangálás), ezért újratanítva az új térképpel.

**Nyitott (a gazdi dönt):** a padon alvás ugrás-animációt (A22) kér, és a kutya talán túl nagy hozzá.

**A gazdi listája (2026-10-28), javítva:**
1. Darabos, össze nem illő animációk: új réteg (`common/lili/pose.c`): pózváltáskor köztes mozdulat (leül, feláll, lefekszik, felkel), 45 fokos fordulás, a hiányzó irányú animáció előtt odafordul, a járás a sebességével megy, a szakadó hurkok oda-vissza játszódnak, az ijedt ugrás földet ér. Videó: `docs/img/fix_2026-10-28/moves.mp4` (`./lilisim --moves-record DIR`).
2. A plüss kivéve (játékosdoboz), az agyat nem kellett újratanítani.
3. A dobott kötél: nem pörög, nem száll el, nem pattog.
4. PixelLab utolsó kockával (a gazdi jóváhagyta, 56 generálás): a leülés öt irányban állásból ülésbe, új összegömbölyödés (fekvésből alvásba, visszafelé a kibomlás), simogatás a fekvés végéből, a járások (séta, ügetés, futás, osonás, settenkedés, kötélhordás) hurokként zárva, a fül nem vált, a méret nem ugrik. Videók: `docs/img/pilot_2026-10-28/` (`pilot_old_vs_new.mp4`, `wave1_old_vs_new.mp4`, `wave2_old_vs_new.mp4`).
5. A kötél a gumilabda és a szikla között (89 / 241 / 249 px gyenge, közepes és erős pöccintésre, egy kis pattanás), a szoba bal fala nem padló többé.
6. Tölcséres érintés: a játék, a folt, a tálak, a pelenka, Lili, az ajtó és a sáv körül nagyobb a nyomható terület, és ha több is elérhető, a legközelebbi nyer; az M réteg mutatja a játék körüli kört.
7. Beépítve (PixelLab, a gazdi jóváhagyta): puffogás (A13) 8 irányban, a kötél felvétele (A14) 5 irányban, a leejtés ugyanez visszafelé, kukucskálás a pad alól (A21) 2 irányban; videó `docs/img/pilot_2026-10-28/wave3_new.mp4`. A plüsst a gazdi Geminivel generálja (`docs/art/startframes/PROMPTS.md` 09).
8. A flash utáni kifagyás (a kijelző el sem indult, a régi kép maradt, az érintés halott): az I2C-busz felszabadítása induláskor, türelmesebb táblafelismerés, újraindítás, ha a kijelző mégsem indul; a napló kiírja az újraindítás okát.
9. Jutalomfalat: hosszú nyomás a padlón, odamegy, megeszi, fekve rágja. Kaka: evés után a pelenkára vagy rosszalkodáskor a földre, a kaka képe a kelléklapról, koppintásra eltűnik; a pelenkán sárga folt, a tócsa nedves folt. A plüss visszajött a gazdi képéből (rózsaszín malacos csörgő), saját színekkel. Videó `docs/img/pilot_2026-10-28/moves_care.mp4`.
10. A gondozás átmenetei (2026-10-29, PixelLab utolsó kockával, a gazdi jóváhagyta): a fej a tálba és ki (evés, ivás), guggolás pisiléshez és kakiláshoz, majd fel, lehajolás a kekszhez, lefekvés vele. Videók: `docs/img/pilot_2026-10-28/wave4_new.mp4` (előtte és utána), `care_moves.mp4` (a játékban, `./lilisim --care-record DIR`).
11. Udvar (2026-10-29): az ajtóhoz húzott játék átkerül a másik oldalra, Lili néha kiviszi vagy behozza a játékát.
12. Etetés (2026-10-29): napi 2-3 étkezés, evés után ivás, üres tálnál odamegy és kéreget; a tálak 4 fokozatú képe jóváhagyásra vár.
13. Hang (2026-10-29): ElevenLabs, a gazdi választása `assets/sounds/elevenlabs/picks.tsv` (meghallgató videók `docs/img/pilot_2026-10-28/hangok_1.mp4`, `hangok_2.mp4`); a panelen és a szimulátorban szól (`common/lili/sfx.c`, `firmware/lili/main/audio_216.c`). A második kör 6 hangja (horkolós légzés, puffogás, granula, víz, szövetjáték, 3. ugatás) a gazdi választására vár, addig néma.

<!-- /overview -->

## Kész
- **M0.1** Fejlesztői környezet: WSL2 Ubuntu 24.04.5 a `D:\WSL\Ubuntu-24.04`-en (tükrözött hálózat, systemd), build-essential (gcc 13.3), SDL2 2.30, cmake 3.28, ninja 1.11, ffmpeg 6.1, uv 0.13; ESP-IDF **v5.4.1** (`~/esp/esp-idf`, `install.sh esp32s3`, `idf.py --version` OK); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14); `.venv` Python 3.11 + **torch 2.11.0+cu128**, `torch.cuda.is_available()` = True, sm 12.0. Windowson: usbipd-win 5.3, uv, VS Code WSL bővítmény.
- **M0.2** A repó a pocket-tank `a3184fc` klónja, `upstream` remote beállítva; `CLAUDE.md`, `PIXEL_LILI_SPEC.md`, `docs/`, `reference/` bent. **Helye (2026-10-26 óta): `F:\AI\Project\Lili_project\pixel-lili`** (WSL-ből `/mnt/f/AI/Project/Lili_project/pixel-lili`; előtte az asztalon volt). A gazdi döntése (2026-10-18): **GitHub nincs**, a repó csak helyi; a félig létrehozott üres privát `piszabolcs/pixel-lili` repó érintetlen, bármikor törölhető. A WSL-ben csak a fordító környezet él: `~/esp/esp-idf`, `~/dev/lili-venv` (PyTorch), `~/dev/pocket-tank` (referencia-klón).
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
- **PixelLab, 1. jóváhagyási kör (2026-10-26):** a gazdi visszajelzése alapján: egy méret és egy talpvonal minden animációnak (`tools/pixellab_norm.py`, ellenőrzőlap `docs/img/anim/size_check.png`; az NW-k 20 %-kal nagyobbak voltak, a trot_S és a sniff_E felemelkedett), a `stand_N` farokgyűrűjének foltja kivéve, a 9 N animáció újra, az ugatás fejfelcsapással (S, SW, W, E, SE; hátulról nincs), trot_S helyben, eat_treat_S és a kötélrágás újra, a carry_trot_S a PixMiniMax-os, az elvetett változatok törölve. **Próbáld ki:** `docs/review/animaciok.html` (önálló, minden GIF benne). **Semmi nincs bekötve**; a beépítésre váró javaslat a gazdi döntésére vár (ivás: drink_W_pmm, ugatás S: bark_S_v2, a walk N foltja, az alvás SW cseréje).
- **PixelLab, javítások és udvari próba (2026-10-26 késő este):** trot_S, startle_N és a scurry mind a 8 irányban újra (a gazdi által jelzett gyengék). **Udvari próba:** szélben hajló fa, megrázkódó bokor, verebek, pillangó, `docs/img/anim/yard_preview_summer.gif` (`tools/yard_anim_preview.py`), a jóváhagyó oldal tetején. **Próbáld ki:** `docs/review/animaciok.html`. Semmi nincs bekötve.
- **M3.5 [G] (folyamatban, 2026-10-27):** animációk 1. köre (A01-A09) a jóváhagyott PixelLab-kockákból mind a 8 irányban, `--anim-viewer` a szimulátorban. **1. lépés kész:** a pak kerete mérve (`tools/pak_budget.py`, stats.md): az M3.5 pakja 2,3 MB, az app képébe (max. 2,27 MB) már nem fér, ezért a pak a spec 5.10 saját asset-partíciójára költözik (5,4 MB); a teljes jóváhagyott készlet 3,9 MB. **2. lépés kész:** a pak a saját asset-partícióján (0xA90000), a firmware memóriába leképezve olvassa (mmap), a panelen kipróbálva (32 fps, mint előtte); `py -3.12 tools/flash_216.py lili` = app + pak, `... pak` = csak a pak. A trot_SE elkészült, a gazdi jóváhagyta. **3. lépés kész:** a sprite-lánc a normalizált 128 px-es kockákból (`tools/pixellab_to_sprites.py`, mesterek `assets/sprites/lili_128`), az A01-A09 mind a 8 irányban a pakban, vágott cellákkal; a felállás a leülés visszafelé (`ANIM_REVERSE_OF`); pak 2,4 MB, a panelen 32 fps (`docs/img/m3_5_room_sizes.png`). **4. lépés kész:** a felállás kirajzolva (a leülés kockái visszafelé, `actor_frame`), a szimulátor A billentyűje a spec sorrendjében lépteti az animációkat és kiírja a nevüket, selftest a felállásra; az árnyék a készlet méretével skálázódik, nem a vágott cellával. **A gazdi (2026-10-27):** statikus hátterek, a fa animáció elvetve, a bokor csak egyszer rezzen, ha Lili belemegy (M4.2), madarak és pillangó később.
- **M3 Blender gyártósor** (a gazdi Blender 5.2.2 LTS-t telepített).
- Később (D-07): "Lili" névfelismerés.

## Próbáld ki (a gazdinak)
- **A demó a panelen (2026-10-23):** bekapcsolás után magától fut a bemutató kör (szoba, évszakok, udvar). BOOT = demó megállítása / újraindítása, KEY3 = szoba/udvar váltás (ez megállítja a demót). Újraflashelés: `py -3.12 tools/flash_216.py lili`. A kép 90 fokkal el van forgatva az óramutatóval ellentétesen (a kábel oldalt jön ki; a gazdi megerősítette, hogy jó irányba fordult).
- **Kezdőkockák (2026-10-23):** `docs/art/startframes/PROMPTS.md`: 7 Gemini-kép (6 lap, mindegyiken 8 póz, és egy kellékes lap), a kész képek ugyanebbe a mappába kerülnek `01.png` ... `07.png` néven.
- **A Lili-világ a panelen:** `py -3.12 tools/flash_216.py lili`, utána a panel a helyőrző szobát mutatja (KEY3 = udvar, BOOT = évszak). Vissza a diagra: `flash_216.py diag`.
- **A panelen (M1.3):** menj végig az üdvözlő oldalon (érintés), nézd meg, hogy úsznak-e a halak, működik-e az érintés és a 3 gomb. Írd meg, melyik gomb mit csinált (M1.3).
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Project/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
