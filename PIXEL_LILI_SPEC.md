# Pixel Lili: Végleges Rendszerterv és Megvalósítási Specifikáció (v2.0)

Dátum: 2026-10-18
Állapot: a tervezés lezárva, a megvalósítás pontról pontra indítható (a 15. fejezet nyitott döntései alapértelmezéssel rendelkeznek, ezek nem blokkolják az indulást).
Alap: a [mediacutlet/pocket-tank](https://github.com/mediacutlet/pocket-tank) (MIT licenc) architektúrája és kódja.

---

## 0. Hogyan használd ezt a dokumentumot

**Neked (a gazdinak):**
1. Olvasd el az 1. fejezetet (mi változott a korábbi tervhez képest és miért).
2. Nézd át a 3. fejezetet (Lili karakterbibliája): ez a projekt "DNS-e", minden ebből származik.
3. Válaszolj a 15. fejezet nyitott kérdéseire, amikor időd engedi (mindegyiknek van alapértelmezése).
4. A 12. fejezet mérföldköveit sorban add oda a Claude Code-nak.

**A Claude Code-nak:**
- A `CLAUDE.md` erre a fájlra mutat. Ez a dokumentum az egyetlen igazságforrás. Ha valami ellentmond a régi (Gemini-féle) szövegeknek, ez a dokumentum érvényes.
- Mérföldkövenként dolgozz (12. fejezet). Egy munkamenet = egy mérföldkő vagy annak egy feladata. Minden feladatnak azonosítója van (pl. `M4.3`), és "Kész, ha" feltételei.
- Minden mérföldkő végén: működő build a szimulátorban, lefutó selftestek, frissített `docs/STATUS.md` és mért számok a `docs/stats.md`-ben.
- Becslés helyett mérés: minden teljesítményszámot mérj és naplózz.

---

## 1. Mi változott a korábbi tervhez képest (audit)

A korábbi tervet összevetettem a pocket-tank tényleges kódjával és mért adataival. Az alábbi javítások kötelezőek:

| # | Korábbi állítás | Valóság / javítás | Következmény |
|---|---|---|---|
| 1 | A BSP-t (kijelző, érintés, IMU, RTC, PMIC) nekünk kell megírni | A pocket-tank **már hivatalosan támogatja** a Waveshare ESP32-S3-Touch-AMOLED-2.16-ot (`sdkconfig.square`, `TANK_SQUARE`, 2026-10-12 óta), kalibrált érintéssel és mért fogyasztással | Az 1. réteg nagy része átvehető, nem kell nulláról írni. Óriási időnyereség. |
| 2 | Egy LLM döntés ~0,9 mp | Ezen a panelen mérve: **4,25-4,44 mp / döntés, 9,6-10,1 token/mp**, ~44 prompt + 3 kimeneti token mellett | A sémát tokenben takarékosra tervezzük: max. ~32 token / döntés, így ~3,2 mp (10. fejezet). |
| 3 | 320 szavas szókincs "hunger 7" formájú párokkal | Minden szó egy teljes súly-végigolvasás. A mező+érték párok duplázzák a promptot | Mező és érték egyetlen tokenben: `hunger_7`. A prompt felére rövidül. |
| 4 | 3 lépéses kimenet címkékkel (`step1 ... style ... face ...`) | A címkék is tokenek, mindegyik ~0,1 mp | Fix sorrendű slotok címkék nélkül: 7 kimeneti token. |
| 5 | Squircle sarok sugara 76 px | A pocket-tank a 2.16-on `TANK_CORNER_R 40` értéket használ, a kártyákat 26/22 px-re húzza be | 40 px a kiinduló érték, az M1-ben a valódi üvegen ellenőrizzük. |
| 6 | Modell partíció 0x500000-n, 8,5 MB | A valódi tábla: NVS 24 KB @0x9000, factory app 2,5 MB, **model 8 MB @0x290000**, storage 5,4 MB | A pocket-tank táblát tartjuk meg, a storage részt asset-partícióvá nevezzük át. |
| 7 | A tanító "tanár" a Claude vagy Gemini API | Az fizetős lenne. A pocket-tank helyi, ingyenes Ollama tanárt használt (gemma4:26b) | Nálunk elsődlegesen egy saját szabálymotor a tanár (ingyenes, determinisztikus), opcionálisan helyi Ollama modell (10.7). |
| 8 | Kézbevételre ébredés bármikor | A pocket-tank 20 perc "bóbiskolás" után a PMIC-kel teljesen lekapcsol, onnan csak a PWR gomb (kb. 1/8 mp nyomás) ébreszt | Az IMU-s ébredés csak a könnyű alvó állapotban működhet. Ezt az M1-ben mérjük ki (fogyasztás, az IMU INT láb bekötése). |
| 9 | A sprite-ok tükrözhetők (8 irány helyett 5 + tükör) | Lili füle aszimmetrikus (egyik áll, másik félig lekonyul). Tükrözéskor felcserélődne | **Tilos tükrözni.** Minden irányt külön renderelünk. |
| 10 | "A 2.5D térben közeledve megnő a mérete" | Sprite-skálázás tönkreteszi a pixel artot | Nincs skálázás. A mélységet az Y pozíció, a takarás és az árnyék adja. |
| 11 | KV cache és aktivációk a belső SRAM-ban | A KV cache 1,5 MB (fp32), az csak PSRAM-ba fér. A 2.16-on a pocket-tank futása mellett mindössze **~38 KB belső heap** marad | A belső SRAM a szűk keresztmetszet, minden nagy puffer PSRAM-ba megy (5.11). |
| 12 | PyTorch nightly kell az RTX 5060-hoz | A Blackwell (sm_120) támogatás már stabil CUDA 12.8+ buildekben elérhető | Stabil PyTorch, cu128 vagy újabb index. |
| 13 | A 4 bites modell "98-99%-ban" ugyanazt dönti | Mért adat: 97% cél-egyezés (58/60) a fp32 és a 4 bites között | A célunk: ≥95%, a vizualizálóban mérjük. |
| 14 | A pocket-tank LVGL-lel rajzol az eszközön | Az eszközön saját RGB565 szoftveres renderelő van (`common/render.c`), az LVGL csak a szimulátor ablakát adja | Saját 8 bites indexelt renderelőt írunk (7.2), ami jobban illik a Pokémon-stílushoz és gyorsabb. |
| 15 | Saját bináris modellfejléc ("LIL4") | A pocket-tank C motorja a saját exportformátumát olvassa (GS 64, fp16 skálák, tokenizer.bin, trailer) | A pocket-tank formátumát tartjuk meg, így a motor változtatás nélkül futtatható. |

---

## 2. Vízió és alapelvek

**Cél:** zsebben hordható, érintőképernyős emlék-kisállat Lili, a 2026. szeptember 12-én elhunyt chihuahua emlékére. A viselkedését egy teljesen helyben, internet nélkül futó kis nyelvi modell irányítja.

**Alapelvek (kötelezők minden döntésnél):**

- **P1 Örök élet:** Lili soha nem hal meg, nem lesz beteg, nincs "game over". Elhanyagolásra legfeljebb álmos, unott vagy rosszalkodik.
- **P2 Ingyenes és offline:** minden eszköz nyílt forráskódú vagy ingyenes. Futás közben nincs hálózat (kivéve az opcionális, egyszeri időszinkront).
- **P3 Közös kód:** a `common/` mappa változtatás nélkül fordul a PC-s szimulátorba és a firmware-be.
- **P4 A test soha nem vár az agyra:** a valós idejű réteg (Core 0) soha nem blokkol az LLM-re.
- **P5 Az agy dönt, a test végrehajt:** az LLM választja a célokat. A reflexek csak megszakítások (simogatás, hívás, dobás), utánuk az agyat újra megkérdezzük.
- **P6 Mérés, nem becslés:** minden számot mérünk és a `docs/stats.md`-be írunk.
- **P7 Emlékhűség:** Lili apró részletei (felemás fül, homlokcsík, kopott orr, puffogás) követelmények, nem díszítés.
- **P8 Adatbázis-szabály:** bármilyen adatbázis vagy SQL fájl kizárólag olvasható, módosítani csak a felhasználó adott beszélgetésben adott kifejezett engedélyével szabad, akkor is, ha az engedélyezési mód automatikus.
- **P9 Kis lépések:** minden mérföldkő végén működő, kipróbálható build.

---

## 3. Lili karakterbibliája (a projekt DNS-e)

### 3.1 Külső jegyek (grafika és 3D modell követelmények)

| Azonosító | Jegy | Követelmény |
|---|---|---|
| L1 | Testalkat | Kis termetű chihuahua, kölyökszerű arányok |
| L2 | Bunda | Krémes, világos vörösesszőke |
| L3 | Fehér részek | Mellkas, has, mancsok fehérek (kölyökkorában sokáig szőrtelen hasa volt: "kis csórehasú") |
| L4 | Homlok | Fehér csík (blaze) a két szem között |
| L5 | Szemek | Nagy, kerek, sötét, kicsit szétfelé állnak |
| L6 | Orr | Rózsaszínes-barna, világosra kopott |
| L7 | Fülek | **Az egyik egyenesen áll, a másik gyakran félig lekonyul.** Melyik oldal: lásd D-03. Tükrözés tilos. |
| L8 | Farok | Magasra felkunkorodó |
| L9 | Arc boldogan | Liheg, "mosolyog", kilóg a nyelve |

Becenevek: "kis csórehasú", "kis kattosom".

### 3.2 Szokások és személyiség (viselkedési követelmények)

Ezekre az azonosítókra hivatkozik a tanár szabálymotor (10.7), a próbák (10.11) és az animációk (7.4).

| ID | Szokás | Mikor | Mit csinál |
|---|---|---|---|
| H1 | Üdvözlés játékkal | A gazdi "hazaér" (ébresztés 15 percnél hosszabb szünet után) | Felkapja a kötéljátékot vagy a plüsst, eléd szalad, előtted rágcsálja, farokcsóválva |
| H2 | Követés | A gazdi jelen van | Szívesen jön a képernyő elejére, követi az ujjad |
| H3 | Incselkedés | Játékos kedv, játék a közelben | `play_bow` (fej a földön, popsi az égnek). Ha az ujjad közelít, elszalad a játékkal |
| H4 | Ugatás | Izgalom, hívás, rázás | Előre néz, majd a fejét függőlegesbe felcsapva vakkant |
| H5 | Puffogás | Figyelmet követel, valami nem tetszik | Orron át kifújt "puff" |
| H6 | Napozás | Délután, ha besüt a nap az ajtón | Beül a napfényfoltba, sütkérezik |
| H7 | Ágyra szökés | A gazdi régóta távol, Lili magányos és unatkozik | Körbenéz, lábujjhegyen odaoson, felugrik a "tiltott" nagy ágyra, ott alszik |
| H8 | Pelenka | Pisilni/kakilni kell, jó kedve van | A fehér, vékony kék szegélyű pelenkára megy. Ez nem bűn, nem bújik el |
| H9 | Rosszalkodás | Pisilni kell ÉS (unott vagy magányos vagy tele a pelenka) | A pelenka mellé, a padlóra csinál, egy vagy több helyre |
| H10 | Bűnös bújás | H9 után | Bebújik a jobb oldali sarokülő alá, csak a feje és a két nagy szeme látszik ki. Csak akkor jön elő, ha minden nyomot feltakarítottál, akkor boldogan előrohan |
| H11 | Kuckó | Alvás, pihenés, este | A sarokülő alatti kuckójában alszik összegömbölyödve |
| H12 | Alaptermészet | Mindig | Pörgős, boldog, sokat csóválja a farkát, lihegős mosoly |
| H13 | Udvari bújócska | Kint az udvaron | Bokor mögé bújik (teljesen eltűnik). Füttyre vagy a nevére előjön és odaszalad |
| H14 | Evés, ivás | Éhes, van étel | A sarokülő melletti tálakból eszik, iszik |
| H15 | Emléknap (szept. 12.) | Egész nap | Nem rosszalkodik, különösen bújós, a közeledben marad |

### 3.3 Bella (2. fázis, most NEM készül)

Pirosabb-barna bunda, fehér arc, fekete orr, mindkét füle áll. A rendszer adatszerkezetei már most több kutyásak (11.5), Bella a v2.0-ban költözik be.

### 3.4 Referenciaanyagok (`reference/` mappa)

Megvan: Lili fotói, `lili_3d_proba.gif`, `lili_v1.gif`, `sheet_dirs.png`.
Még kell (D-10): fotó a kötéljátékról, a plüssről, az ágyról a takaróval, a konyhai sarokülőről, a pelenkáról, és ha megvan, a korábbi Blender próbafájl (`.blend`).

---

## 4. Architektúra

### 4.1 Fork stratégia

A projekt a pocket-tank **privát forkja** (`pixel-lili`), a pocket-tank `upstream` remote-ként megmarad, hogy a meghajtó-javításokat át lehessen venni. A fork pillanatában a commit hash-t rögzítjük a `docs/UPSTREAM.md`-ben. Az MIT licenc szövegét és a forrásmegjelölést megtartjuk.

| Pocket-tank rész | Sorsa | Megjegyzés |
|---|---|---|
| `firmware/` portok (display CO5300 square út, touch `cst9217_*`, battery `axp2101`, RTC, IMU, audio ES8311) | **Átvesszük** | A 2.16-ra már kalibrált és mért |
| `firmware/` advisor ütemező, Director soros konzol, flash scriptek (`tools/flash_square.sh`) | **Átvesszük**, átalakítjuk | |
| `common/llm/` (4 bites motor, szó-tokenizáló, kódoló) | **Átvesszük** | A kódoló (encoder) Lili sémájára cserélődik |
| `model/` (train.py, export_q4.py, eval, prompt_check, llama2.c klón) | **Átvesszük**, átalakítjuk | |
| `sim/` (LVGL + SDL2 ablak, selftest keret, snapshot) | **Átvesszük**, bővítjük | Inspector panel kerül mellé |
| `common/audio.c`, hangbank építő eszköz | **Átvesszük** | |
| Mentési minta (a mentés csak a végén nő, minden régi verzió betölthető) | **Átvesszük** | |
| `common/tank.c`, `render.c`, `progression.c` tartalma, halas ikonok, bolt, születés, alga | **Lecseréljük** | Lili saját moduljaira |
| OTA, Wi-Fi frissítés, böngészős telepítő | **v1-ben kihagyjuk** | A Wi-Fi port csak az opcionális időszinkronhoz marad |

**M0 előtti kötelező olvasmány a Claude Code-nak** a pocket-tank repóból: `README.md`, `docs/board-amoled-2.16.md`, `docs/BOARDS.md`, `docs/bringup.md`, `docs/memory_budget.md`, `docs/DEVICE.md`, `docs/AUDIO.md`, `docs/pipeline.md`, `docs/stats.md`, `model/schema.md`.

### 4.2 A 7 réteg leképezése modulokra

| Réteg | Feladat | Modulok | Mag | Mappa |
|---|---|---|---|---|
| 1 Kernel / HAL / BSP | Chipek, gesztusok, energia, idő, mikrofon | `display_port`, `touch_port`, `imu_port`, `rtc_port`, `battery_port`, `audio_port`, `mic_port`, `power.c` | Core 0 (I2C, kijelző), Core 1 (I2S mikrofon DSP) | `firmware/` |
| 2 Szimulátor | PC-s futtatás, inspector, headless tesztek | `sim_main.c`, `inspector.c`, `sim_ports.c` | PC | `sim/` |
| 3 Grafika | Assetek gyártása és betöltése, renderelő, UI | `render8.c`, `assets.c`, `ui_glass.c`, `font.c`; Blender és Python eszközök | Core 0 | `common/`, `assets/`, `tools/` |
| 4 Világ és reflexek | Tér, navigáció, Y-rendezés, terv-végrehajtó, reflexek, játékfizika | `world.c`, `nav.c`, `actor.c`, `plan_exec.c`, `reflex.c`, `toys.c`, `gesture.c` | Core 0 | `common/` |
| 5 Igények és idő | Szükségletek, pelenka és bűnösség, napszak, évszak, dátumok, offline idő, mentés | `progression.c`, `calendar.c`, `offline.c`, `save.c` | Core 0 | `common/` |
| 6 Agy | Állapotkódoló, LLM, szabálymotor, tanítás | `llm/`, `brain.c`, `brain_rules.c`, `encoder.c` | Core 1 | `common/llm/`, `model/`, `tools/` |
| 7 Hang és bővítés | Hangeffektek, hangesemények, Bella-kész szerkezetek | `audio.c`, `sfx_events.c`, `voice_detect.c` | Core 0 (lejátszás), Core 1 (felismerés) | `common/` |

### 4.3 Futásidejű adatfolyam

```
             CORE 0 (valós idő, 25-30 fps)                      CORE 1 (háttér)
 ┌──────────────────────────────────────────────┐      ┌───────────────────────────┐
 │ touch/IMU/gomb -> gesture.c -> események      │      │ mic I2S -> voice_detect.c │
 │        │                                      │<-----│  (fütty, név, zaj)        │
 │        v                                      │ hang │                           │
 │ reflex.c  (megszakítások, prioritás)          │ ese- │                           │
 │        │                                      │ mény │                           │
 │        v                                      │      │                           │
 │ plan_exec.c (az aktuális terv lépései)        │      │ brain.c                   │
 │        │                ^                     │      │   encoder -> LLM (Q4)     │
 │        v                │ terv (seq id)       │<-----│   vagy brain_rules.c      │
 │ actor.c + nav.c + toys.c (mozgás, fizika)     │      │                           │
 │        │                                      │ kér- │                           │
 │        v                                      │ dés  │                           │
 │ progression.c (igények, idő, mentés)  ------->│----->│  (állapot pillanatkép)    │
 │        │                                      │      └───────────────────────────┘
 │        v
 │ render8.c (240x240, 8 bit) -> flush: paletta LUT + 2x nagyítás + üveg UI -> CO5300
 └──────────────────────────────────────────────┘
```

### 4.4 Magok közötti kommunikáció

- **Kérdés (Core 0 -> Core 1):** egy elemű postaláda, "a legfrissebb nyer". Tartalma: `AskRequest { uint32 seq; uint8 actor_id; uint8 reason; LiliStateSnapshot snap; }`.
- **Válasz (Core 1 -> Core 0):** `PlanReply { uint32 seq; uint16 actions[3]; uint8 style, face, bubble, urgency; float top2_margin; uint16 latency_ms; }`.
- **Elavulás:** ha a válasz `seq` értéke kisebb, mint az aktuális világgeneráció (közben reflex vagy nagy változás történt), a Core 0 eldobja.
- **Szinkronizáció:** FreeRTOS queue (hossz 1, `xQueueOverwrite`), nincs megosztott írható állapot a két mag között.
- **I2C busz:** kizárólag a Core 0 használja, mutex-szel (a kodekek konfigurálása is innen történik).

### 4.5 Mappastruktúra (végleges)

```
pixel-lili/
├── CLAUDE.md                    # rövid szabályok + mutató erre a dokumentumra
├── PIXEL_LILI_SPEC.md           # ez a dokumentum
├── LICENSE                      # MIT (pocket-tank) + saját kiegészítés
├── docs/
│   ├── STATUS.md                # mérföldkövek állapota, Claude Code frissíti
│   ├── stats.md                 # minden mért szám, dátummal
│   ├── UPSTREAM.md              # fork commit, átvett és módosított fájlok
│   └── decisions.md             # meghozott döntések naplója (D-xx)
├── reference/                   # Lili fotói, gifek, próbák (csak olvasásra)
├── common/
│   ├── lili_types.h  config.h
│   ├── world.c nav.c actor.c plan_exec.c reflex.c toys.c gesture.c
│   ├── progression.c calendar.c offline.c save.c
│   ├── render8.c assets.c ui_glass.c font.c
│   ├── audio.c sfx_events.c voice_detect.c
│   ├── brain.c brain_rules.c
│   └── llm/  (pocket-tank motor + Lili encoder.c, vocab.h)
├── firmware/                    # ESP-IDF, sdkconfig.square, partitions.csv, portok
├── sim/                         # SDL2 + LVGL szimulátor, inspector, selftestek
├── model/                       # tanítás, export, eval, llama2.c klón
├── assets/
│   ├── blender/                 # lili.blend, room.blend, yard.blend
│   ├── maps/                    # room.json, yard.json (járható tér, takarók, pontok)
│   ├── palettes/                # master.gpl, évszak és napszak paletták
│   ├── raw_renders/             # Blender kimenet (gitignore)
│   ├── sprites/                 # pixelesített, ellenőrzött PNG-k
│   ├── sounds/ raw_videos/ audio_clips/
│   └── build/assets.pak         # a csomagolt bináris
└── tools/
    ├── blender/ render_all.py scene_setup.py export_manifest.py
    ├── pixelize.py pack_assets.py palette_from_photos.py contact_sheet.py
    ├── extract_lili_audio.py
    └── llm_visualizer.py
```

### 4.6 Kódolási szabályok

- C99, a pocket-tank stílusát követve. A képkocka-ciklusban nincs `malloc`.
- Az S3 egyszeres pontosságú FPU-val rendelkezik: `float` rendben van, `double` tilos a forró ciklusokban.
- Minden hangolható szám a `config.h`-ban, név szerint. Fő kapcsolók: `CONFIG_LILI_THOUGHT_BUBBLES`, `CONFIG_LILI_SEARCH_HINT`, `CONFIG_LILI_MIC`, `CONFIG_LILI_NAME_WAKEWORD`, `CONFIG_LILI_BRAIN_DEFAULT` (rules/llm), `MAX_ACTORS`.
- Determinizmus: minden véletlen egy magolható (seed) PRNG-ből jön, hogy a selftestek reprodukálhatók legyenek.
- Hibakezelés: ha a modell hiányzik vagy érvénytelen, a szabálymotor fut (ahogy a pocket-tank is tette), és a naplóba figyelmeztetés kerül.

---

## 5. 1. réteg: Kernel, hardver, BSP

### 5.1 A panel (a pocket-tank mért adatai alapján)

| Rész | Adat |
|---|---|
| SoC | ESP32-S3R8, 2 mag 240 MHz, 8 MB octal PSRAM, 16 MB flash |
| Kijelző | CO5300, 480x480 AMOLED, QSPI: GPIO 4/5/6/7, CLK 38, CS 12, RESET 39 |
| Érintés | CST9220 @0x5A, RESET GPIO 40, INT GPIO 11 (lekérdezéses), a pocket-tank `cst9217_*` útja |
| I2C | SDA 15, SCL 14 |
| PMIC | AXP2101: DCDC1 = VCC3V3, ALDO1 = A3V3 (kodek + ES7210) |
| Hangszóró | ES8311: DSDIN 8, BCLK 9, LRCK 45, MCLK 42, erősítő 46. Fő hangerő -12 dB (a pocket-tank szerint ennél hangosabb torzít) |
| Mikrofonok | ES7210 @0x40 (a pocket-tank bootkor felfüggeszti, nekünk kell élesíteni) |
| RTC | PCF85063 @0x51, kikapcsolt állapotban is jár |
| IMU | QMI8658 @0x6B, "fel" irány = +Y (szokásos fogásban, USB lent) |
| Gombok | PWR a középső gomb (PMIC-en át, SYS_OUT GPIO 16); BOOT GPIO 0; KEY3 GPIO 18 |
| Akku | 3,7 V 1000 mAh. Mérve: ~12 óra bekapcsolt képernyő, ~79 mA; kikapcsolva 1 mA alatt |
| Egyéb | TF kártyahely (GPIO 1/2/3/41), v1-ben nem használjuk |

### 5.2 Kijelző

- MADCTL `0xA0` (USB lent, a pocket-tank szerinti normál fogás).
- A képet sávokban (stripe) küldjük, soronként, mint a pocket-tank `native_flush`.
- **Sarok:** `CORNER_R = 40` fizikai px kiinduló érték (logikailag 20). M1-ben a valódi kávához mérjük.
- **Biztonsági zóna:** interaktív elem (tál, pelenka, folt, játék, UI gomb) középpontja legalább 24 logikai px-re a sarokív belső pontjától. A renderelő debug módban kirajzolja a tiltott sarokzónát.
- **AMOLED perem:** a kép szélén 6 fizikai px-es vignetta feketébe, hogy a kép beleolvadjon a kávába.
- Fényerő: 3 szint (beállítások), DIM állapotban 30%.

### 5.3 Érintés és gesztusok

A kalibráció a pocket-tank 2.16 értékeiből indul (`CAL_S_*`, a landolási torzítással). Gesztusok a `gesture.c`-ben, fizikai px-ben:

| Gesztus | Feltétel (kiinduló érték, hangolható) | Esemény |
|---|---|---|
| TAP | érintés < 250 ms, mozgás < 12 px | `EV_TAP(x,y)` |
| LONG_PRESS | > 600 ms, mozgás < 12 px | `EV_LONG(x,y)` |
| RUB_PET | Lili kibővített hitboxa felett (+16 px), 1,2 mp alatt legalább 3 irányváltás, mozgás összesen > 60 px | `EV_PET_START / EV_PET_TICK / EV_PET_END` |
| FLICK | játékon indul, felengedéskor a sebesség > 600 px/s (utolsó 80 ms átlaga) | `EV_FLICK(toy, vx, vy)` |
| DRAG_TOY | játékon indul, lassú | a játék az ujjat követi, elengedve leesik |
| APPROACH | ujj lent, távolság Lilitől csökken, sebesség < 300 px/s, Lili `play_bow`-ban | `EV_APPROACH(dist)` |
| EDGE_SWIPE | indulás az alsó/felső széltől 24 px-en belül, függőleges elmozdulás > 120 px (2026-10-21: a váltás felfelé, az ajtó felé történik) | `EV_SWITCH_VIEW` |
| TAP az ajtón | koppintás a spot_door környékén (2026-10-21) | `EV_SWITCH_VIEW` |

### 5.4 Gombok

| Gomb | Rövid nyomás | Hosszú nyomás |
|---|---|---|
| PWR (középső) | alvás / ébresztés (a pocket-tank logikája) | 1,5 mp: teljes kikapcsolás |
| KEY3 (GPIO 18) | nézetváltás: szoba <-> udvar | (szabad) |
| BOOT (GPIO 0) | üveg állapotsáv be/ki | 1 mp: némítás be/ki |

Megjegyzés: melyik fizikai gomb a "+/KEY" és melyik a "BOOT/-" felirat, azt az M1-ben ellenőrizzük, a leképezés egy táblázatban cserélhető.

### 5.5 Mozgásérzékelő

| Esemény | Kiinduló feltétel |
|---|---|
| STILL (asztalon) | 3 mp-ig a gyorsulás szórása < 0,02 g és a dőlés közel vízszintes |
| PICKUP | STILL állapotból: > 0,15 g változás és dőlésváltozás > 10° fél másodpercen belül |
| SHAKE | 3 csúcs > 1,8 g 600 ms-en belül |
| IN_MOTION (zseb, séta) | 3 mp-ig ismétlődő 0,05-0,5 g mozgás |

### 5.6 Idő

- Az RTC helyi időt tárol (Europe/Budapest, `CET-1CEST,M3.5.0,M10.5.0/3`).
- Beállítás (D-11): (a) egyszeri Wi-Fi SNTP a beállításokban, (b) soros parancs: `settime 2026-10-18T14:05:00`, (c) a szimulátor a PC óráját használja.
- Ha az RTC érvénytelen (első indulás), a játék elindul, de a beállítások egy "állítsd be az időt" sort mutat.

### 5.7 Energiaállapotok

| Állapot | Mi működik | Belépés | Kilépés |
|---|---|---|---|
| ACTIVE | 25-30 fps, érintés, IMU, mikrofon, agy | ébredés, érintés | 60 mp tétlenség -> DIM |
| DIM | fényerő 30%, 10 fps, mikrofon és agy fut | 60 mp tétlenség | érintés, hang, mozgás -> ACTIVE; további 60 mp -> alvás |
| DROWSE_DESK | kijelző ki, könnyű alvás, IMU wake-on-motion élesítve, RTC | PWR rövid vagy DIM lejárt, és lezáráskor STILL | PICKUP -> ACTIVE + üdvözlés; a türelmi idő lejárta -> OFF |
| DROWSE_POCKET | kijelző ki, könnyű alvás, IMU ébresztés NINCS | lezáráskor IN_MOTION | csak PWR; türelmi idő -> OFF |
| OFF | csak a PMIC és az RTC | türelmi idő vége, vagy PWR 1,5 mp | PWR tartott nyomás (~1/8 mp) |

- Lezáráskor 3 mp-ig figyeljük az IMU-t, ebből dől el a DESK vagy POCKET mód (a gazdi kérése szerint).
- A türelmi idő a pocket-tankban 1200 s. A DESK módban ezt meghosszabbíthatjuk, ha az M1 mérés szerint a könnyű alvás fogyasztása elfogadható (cél: ≥ 24 óra asztali készenlét). A végleges érték a mérés után kerül a `config.h`-ba.
- **Kockázat:** ha a QMI8658 INT lába nincs az ESP-re kötve, az IMU-s ébredés csak periodikus felébredéssel és lekérdezéssel oldható meg (M1.4).
- Elalvás előtt mindig mentés (5. réteg).

### 5.8 Mikrofon (ES7210)

- Csak ACTIVE és DIM állapotban fut. 16 kHz, 16 bit, 1 csatorna (a második mikrofon későbbi irányérzékelésre tartalék).
- Feldolgozás a Core 1-en, 32 ms-os keretekben, az LLM-mel időosztásban (a detektor < 5% CPU).
- Detektorok (`voice_detect.c`):
  - **Fütty (biztos):** 512 pontos FFT, 1000-3500 Hz-es sáv. Ha a csúcs/sávátlag arány > 12 dB, a csúcs frekvenciája stabil (±15%), legalább 250 ms-ig: `VOICE_WHISTLE`.
  - **Hang / taps (biztos):** energiaküszöb a zajszint fölött 15 dB-lel, rövid (< 150 ms) impulzus = taps, hosszabb = beszéd: `VOICE_ACTIVITY`.
  - **"Lili" név (kísérleti):** lásd 11.4 és D-07. Kapcsoló: `CONFIG_LILI_NAME_WAKEWORD`.
- **Ön-némítás:** amíg a hangszóró szól, és utána 150 ms-ig, nincs hangesemény.
- **Adatvédelem:** a hang soha nem kerül tárolásra vagy továbbításra, csak a jellemzők.

### 5.9 Hangszóró (ES8311)

A pocket-tank `audio.c` keverőjét használjuk: 16 kHz mono, a kodek csak kézben lévő (ACTIVE/DIM) állapotban van táplálva, fő hangerő -12 dB, némítás a beállításokban és a BOOT hosszú nyomásával.

### 5.10 Flash partíciók (16 MB)

| Név | Típus | Offset | Méret | Tartalom |
|---|---|---|---|---|
| nvs | data/nvs | 0x9000 | 24 KB | mentés, beállítások |
| phy_init | data/phy | (a pocket-tank táblája szerint) | 4 KB | |
| factory | app | 0x10000 | 2,5 MB | firmware |
| model | data/0x40 (raw) | 0x290000 | 8 MB | `model_q4.bin` + trailer (mmap) |
| assets | data/raw | 0xA90000 | ~5,4 MB | `assets.pak` (sprite-ok, paletták, hangok) |

A Claude Code a pocket-tank `partitions.csv`-jét veszi alapul, és csak az utolsó partíciót nevezi át. Ha az OTA-s új tábla eltér, a fenti a mérvadó (OTA nélkül).

### 5.11 Memóriakeret

**PSRAM (8 MB):**

| Elem | Méret | Megjegyzés |
|---|---|---|
| Világ puffer 240x240x8 bit (x2) | 115 KB | dupla puffer |
| Kimeneti stripe pufferek (480x24x2 B x2) | 46 KB | ha a DMA PSRAM-ból megy (M2-ben mérjük), különben belső SRAM-ban kisebb sávokkal |
| Statikus háttér-gyorsítótár (nézetenként) | 115 KB | paletta-váltáskor újraépül |
| KV cache (8 réteg x 64 x 384 x 2 x fp32) | 1,5 MB | fp16-ra csökkenthető (0,75 MB), ha kell |
| LLM aktivációk, prefill pufferek | ~400 KB | |
| `assets.pak` betöltve | ~2,0 MB | bootkor a partícióról másoljuk, hogy ne versenyezzen az LLM flash-olvasásával |
| Hangbank | ~0,5 MB | |
| Állapot, egyéb | ~100 KB | |
| **Kötelező tartalék (bootkor assertálva)** | 2 MB | |
| Összesen | ~6,8 MB | a 8 MB-ból |

**Belső SRAM (512 KB):** FreeRTOS, taszk-stackek (világ 12 KB, agy 16 KB, mikrofon 8 KB), I2S DMA, FFT munkaterület (~8 KB), DMA leírók. **Figyelem:** a pocket-tank a 2.16-on futás közben ~38 KB szabad belső heappel fut. Minden új belső foglalást mérni kell, a boot napló kiírja a szabad belső heapet.

### 5.12 Hardveres validációs lista (az M1 tartalma)

1. Gyári flash teljes mentése (`esptool read_flash 0 0x1000000 factory_backup.bin`), mielőtt bármit írunk.
2. A pocket-tank `--full` square build fut a paneleden (látod a halakat).
3. Gombok fizikai helye és GPIO-ja.
4. QMI8658 INT láb bekötése (kapcsolási rajz) és wake-on-motion könnyű alvásból.
5. Fogyasztás: ACTIVE, DIM, DROWSE (a türelmi idő meghatározásához).
6. ES7210 élesítése, szintmérő a kijelzőn, fütty spektrum.
7. Sarok sugár ellenőrzése egy tesztmintával (koncentrikus ívek 32/36/40/44 px).
8. RTC beállítás soros parancsból.

---

## 6. 2. réteg: Fejlesztői környezet és szimulátor

### 6.1 Ajánlott gépbeállítás (D-01)

Alapértelmezés: **Windows 11 + WSL2 (Ubuntu 24.04), tükrözött hálózati móddal** (`networkingMode=mirrored`, így a WSL és a Windows ugyanazt a `localhost`-ot látja).

| Komponens | Hol fut | Miért |
|---|---|---|
| Claude Code | WSL2 | Linux eszközlánc, a pocket-tank Makefile-ok Linuxon mennek |
| Szimulátor (SDL2) | WSL2, az ablak WSLg-n jelenik meg | `make SQUARE=1` |
| ESP-IDF (build) | WSL2 | |
| Flashelés | WSL2 + `usbipd-win` USB átadás, vagy Windows-os esptool | |
| PyTorch + CUDA | WSL2 (az NVIDIA Windows driver adja a CUDA-t) | RTX 5060 |
| Ollama (opcionális tanár) | Windows vagy WSL2 | |
| Blender | Windows natívan | GPU és GUI |
| blender-mcp addon | a Blenderben | a WSL-ből a tükrözött `localhost`-on érhető el |

### 6.2 Telepítési lista (a Claude Code végigvezet rajta az M0-ban)

1. WSL2 + Ubuntu, `.wslconfig`: `[wsl2] networkingMode=mirrored`.
2. WSL-ben: `build-essential git cmake ninja-build libsdl2-dev python3-venv`, `uv`.
3. ESP-IDF: a pocket-tank által használt verzió (v5.4 a dokumentációja szerint), `install.sh esp32s3`.
4. `usbipd-win` a Windowson (USB a WSL-be).
5. NVIDIA driver (Windows), WSL-ben `nvidia-smi` működik.
6. Python környezet: `uv venv .venv --python 3.11`, majd stabil PyTorch CUDA 12.8+ builddel (`--index-url https://download.pytorch.org/whl/cu128` vagy újabb), továbbá `numpy pillow gradio plotly matplotlib librosa soundfile noisereduce scipy`.
7. Blender 4.x vagy újabb (Windows).
8. blender-mcp: [ahujasid/blender-mcp](https://github.com/ahujasid/blender-mcp) `addon.py` telepítése (Edit > Preferences > Add-ons > Install from Disk), a Blenderben N panel > BlenderMCP > Start. WSL-ben: `claude mcp add blender -- uvx blender-mcp`.
9. FFmpeg (WSL).
10. Opcionális: Ollama és egy 8 GB VRAM-ba férő modell (10.7).

### 6.3 Szimulátor

Ablak: bal oldalon a 480x480-as kijelző a kávamaszkkal (40 px sarok, fekete perem), jobb oldalon 360 px széles Inspector panel.

**Egér és billentyűk:**

| Bemenet | Hatás |
|---|---|
| Bal klikk | TAP |
| Bal gomb hosszan | LONG_PRESS |
| Húzás Lili felett oda-vissza | RUB_PET |
| Játék megfogása és gyors elrántása | FLICK |
| Húzás a szélről | EDGE_SWIPE |
| Tab | KEY3 (nézetváltás) |
| B | BOOT (állapotsáv), Shift+B némítás |
| P | PWR (alvás/ébresztés) |
| Enter | PICKUP |
| Space | SHAKE |
| W | fütty (`VOICE_WHISTLE`) |
| N | név (`VOICE_CALL_NAME`) |
| V | hang/taps |
| T | +1 óra alvás (offline idő), Shift+T +8 óra |
| 1/2/3/4 | évszak kényszerítése |
| D | napszak léptetése |
| X | különleges nap léptetése |
| L | agy váltása: szabálymotor <-> LLM |
| G | gondolatbuborékok be/ki |
| M | debug rétegek (járható tér, takarók, hitboxok, sarokzóna) |
| Q | kilépés |

**Inspector panel:** a fenti gombok kattintható változatai; csúszkák az igényekre (közvetlen állítás); élő LLM konzol (a kódolt prompt tokenekkel, a kimenet, a top-5 valószínűség slotonként, a késleltetés, a "habozás" jelzője); a terv-végrehajtó állapota (melyik lépés, miért bukott el); fps és képkocka-idő.

**Kapcsolók:** `--fresh`, `--seed N`, `--fast N` (idő gyorsítása), `--brain rules|llm`, `--greedy`, `--narrate`, `--headless --frames N --screenshot out.png`, `--snapshot <prefix>` (minden fontos jelenet képe), `--dump-traces out.jsonl --hours H` (tanítóadat, 10.8), `--anim-viewer` (sprite-ellenőrző), `--selftest-*`.

### 6.4 Selftestek (a `make check` mind lefuttatja)

| Teszt | Mit ellenőriz |
|---|---|
| `--selftest-render` | sarokzóna, paletta-váltás, Y-rendezés helyes sorrend, referenciakép-összevetés |
| `--selftest-nav` | minden kijelölt pont elérhető, nincs falon átjárás, ajtó-átmenet |
| `--selftest-reflex` | mind a 8 reflex kiváltható és a prioritás helyes |
| `--selftest-toys` | dobás, pattanás, falak, nem hagyja el a nézetet |
| `--selftest-progress` | igények idővel, pelenka, folt, bűnösség, takarítás utáni előrohanás |
| `--selftest-offline` | 1, 8, 24, 72 óra távollét jelenetei a várt eloszlással |
| `--selftest-calendar` | évszakok, szept. 12. tiltások, jan. és jún. ünnepek |
| `--selftest-save` | mentés és visszatöltés, régi verziók betöltése |
| `--selftest-llm [perc]` | valós modell, érvényes kimenetek, cél-eloszlás, habozások |
| `--bench` | renderelési idő elemenként |

### 6.5 Director soros konzol az eszközön (átvéve, bővítve)

`state`, `fps`, `heap`, `settime <ISO>`, `brain rules|llm`, `ask` (azonnali döntés), `mic level`, `imu raw`, `power`, `save now`, `madctl <hex>`, `corner <px>`.

---

## 7. 3. réteg: Grafika, animáció és felület

### 7.1 Művészeti irány

- **Világ (a gazdi döntése 2026-10-21, a célképek: `reference/concept/`):** modern "hi-bit" pixel art, top-down 3/4 nézet. Telített, meleg fő színek hideg árnyékokkal; puha, dinamikus fény (napfénycsík az ajtóból, esti lámpa, holdfény az ablakon); festői, részletes csempék; tiszta pixelrács. A Game Boy / Pokémon look elvetve.
- **Karakter és tárgyak:** selective outline: a kontúr nem fekete, hanem az anyag legsötétebb árnyalata (a kutyán sötét vörösesbarna, a fán sötétbarna); 3-4 tónusú árnyalás, fix fény bal felülről. Lili arányai a jóváhagyott karakterlap szerint (nagy fej, nagy szemek, rövid lábak, felkunkorodó farok), nem gömbökből.
- **Padló, fű, falak:** kontúr nélkül, csempés textúrával; a szoba fapadló deszkákkal, a hátsó fal tapéta és lambéria.
- **A szoba berendezése (a jóváhagyott célkép + a gazdi kérései):** ajtó hátul középen, animált (napközben nyitva, este vagy rossz időben csukva), mellette ablak, amin csukott ajtónál holdfény szűrődik be; a falon képek (nem tányérok) és a sarokülő felett egy óra; 3:2 háromszögmintás szőnyeg a célképnél nagyobb háromszögekkel; L alakú sarokülő kuckóval és takaróval; pelenka, tálak, virágváza.
- **Ütemezés:** az első kör az aktuális, őszi évszak; a többi évszak későbbi frissítésekben.
- **Felület:** modern glassmorphism (tejüveg) kapszula a pixeles világ fölött, natív 480 px felbontásban (7.8).

### 7.2 Technikai vászon és renderelés

- **Logikai felbontás (a gazdi döntése 2026-10-21, M3.4):** **natív 480x480, nagyítás nélkül** (1 logikai px = 1 fizikai px). A 2x nagyított 240-es rács túl durva volt a kívánt hi-bit képi világhoz. A térképek (`assets/maps/*.json`) és a layout-számok továbbra is a 240-es **tervezőrácson** íródnak, és a `gen_maps.py` szorozza őket `LILI_K` = 2-vel; a kézzel rajzoló kód is `LILI_K`-val szoroz.
- **8 bites indexelt színek:** a világ puffer (230 KB, PSRAM) bájtonként egy paletta-indexet tárol. A kijelzőre küldéskor (flush) egy 256 elemű RGB565 táblázat (LUT) alakítja színné, 24 soros csíkokban, közvetlenül a DMA pufferbe. Előnyök: fele akkora memória- és sávszélesség-igény, mint az RGB565, ingyenes napszak- és évszakváltás.
- **Paletta-szerkezet:** a 256 szín "rámpákba" rendezve (egy anyag 6-8 árnyalata egymás után). Fix helyek: 0 = átlátszó, 1-15 UI és kontúr.
- **Napszak:** 4 előre számolt LUT (reggel, délután, este, éjszaka) évszakonként, tehát 16 LUT x 512 B = 8 KB. Átmenet: 2 LUT közötti interpoláció 60 mp alatt.
- **Évszak:** a fű és a lombozat rámpáinak cseréje (ugyanaz a háttér, más színek), plusz évszakos díszítő sprite-ok és részecskék.
- **Helyi fények indexváltó táblákkal (remap LUT):** napfényfolt (melegebb és világosabb árnyalat ugyanabban a rámpában), esti lámpa (sugárirányú), árnyék (sötétebb). Ez 8 bites térben, pixelenként egy táblázat-olvasás.
- **Rajzolási sorrend képkockánként:**
  1. statikus háttér másolása a gyorsítótárból (padló, falak, fű),
  2. talajszintű elemek (pelenka, foltok, lábnyomok, talajárnyékok),
  3. Y szerint rendezett lista: Lili, játékok, takaró tárgyak (sarokülő ülőlapja, bokrok, fák, ágy eleje),
  4. fényrétegek (napfényfolt, lámpa),
  5. részecskék (szirmok, levelek, hó, pillangók, szentjánosbogarak),
  6. gondolatbuborék (ha be van kapcsolva),
  7. flush közben: vignetta, kávamaszk, üveg UI natív felbontásban.
- **Célsebesség:** 25-30 fps, minimum 20 fps (a pocket-tank a 2.16-on 20-25 fps-t mért a saját, nehezebb renderelőjével).

### 7.3 Kamera és méretarányok (Blender, minden assetre közös)

- Ortografikus kamera, **35° emelkedési szög**, a háttér és a karakter pontosan ugyanazzal a kamerával készül.
- A 8 irányt a **karakter forgatásával** állítjuk elő (nem a kamerával), így a fény iránya minden irányban azonos marad.
- **Méretarány:** a karakter a bútorokhoz képest nagyobb (1,4x). Lili állva kb. 80x60 px, a sprite cella **96x96 px** (natív), a pivot (48, 80). 1 px = 0,625 cm a talajon. Ez a 2.16"-os (~38 mm-es) képernyőn kényelmesen látható és simogatható.
- **Talajpont (pivot):** minden sprite cellában a mancsok közepe, ez a Y-rendezés kulcsa.
- **Renderfelbontás:** a célméret 4-szerese (384x384), utána pixelesítés (7.5, B6).

### 7.4 Asset lista

**Lili animációi** (fps = lejátszási sebesség; "irány" = hány irányból kell renderelni):

| ID | Név | Kocka | fps | Irány | Ismétlődik | Megjegyzés, esemény |
|---|---|---|---|---|---|---|
| A01 | idle_stand | 6 | 6 | 8 | igen | légzés, enyhe farokmozgás |
| A02 | idle_sit_blink | 8 | 6 | 8 | igen | pislogás, a lekonyuló fül billen |
| A03 | sit_down / stand_up | 4+4 | 12 | 8 | nem | átmenet |
| A04 | walk | 6 | 10 | 8 | igen | |
| A05 | trot | 6 | 14 | 8 | igen | vidám ügetés, farok fent |
| A06 | run | 6 | 18 | 8 | igen | futás, zoomies |
| A07 | tiptoe | 6 | 8 | 8 | igen | lapuló, lassú (H7) |
| A08 | scurry | 6 | 16 | 8 | igen | bűnös, lelapított fül (H10) |
| A09 | pant_smile | 6 | 8 | 8 | igen | ülve, nyelv kint (L9) |
| A10 | head_tilt | 4 | 8 | 3 (S, SE, SW) | nem | habozás, kíváncsiság |
| A11 | play_bow | 3+4 | 10 | 8 | a második rész | fenék fent, farokcsóválás (H3) |
| A12 | bark_head_flip | 6 | 12 | 8 | nem | 3. kockán hang: `bark` (H4) |
| A13 | huff | 5 | 10 | 8 | nem | 2. kockán hang: `huff` (H5) |
| A14 | pickup / drop | 4+3 | 12 | 8 | nem | szájpont (anchor) kockánként |
| A15 | chew_toy | 8 | 10 | 3 (S, SE, SW) | igen | fekve, játék a mancsok közt (H1) |
| A16 | pet_enjoy | 6 | 6 | 3 (S, SE, SW) | igen | behunyt szem, odabújás |
| A17 | eat_drink | 6 | 8 | 3 (E, NE, SE) | igen | fej a tálban (H14) |
| A18 | sleep_curl | 4 | 3 | 2 (S, SW) | igen | összegömbölyödve, lassú légzés (H11) |
| A19 | sun_sit | 4 | 4 | 2 (S, SW) | igen | hunyorogva napozik (H6) |
| A20 | squat | 6 | 8 | 4 (SE, SW, NE, NW) | nem | pisi/kaki (H8, H9) |
| A21 | guilty_peek | 6 | 6 | 1 (SW) | igen | csak a fej és a szemek a sarokülő alól (H10) |
| A22 | jump_up / jump_down | 6+6 | 14 | 4 (W, NW, E, SE) | nem | ágy (H7) |
| A23 | look_around | 6 | 8 | 3 | nem | "lát valaki?" (H7) |
| A24 | sniff | 6 | 8 | 8 | igen | szaglászás |
| A25 | startle | 4 | 14 | 8 | nem | rázásra |
| A26 | pounce | 5 | 14 | 8 | nem | levél, pillangó, játék elkapása |
| A27 | ask_pet | 6 | 8 | 3 (S, SE, SW) | igen | a képernyőhöz bújik, mancsot emel |

Becsült összesen: kb. 1 000 kocka. 96x96 cellában, 8 bites RLE-vel (mért: 1,4 KB/kocka, M3.4) kb. 1,4 MB; a pak 3 MB-os kerete tartható.

**Animációs metaadatok** (a Blenderből exportálva, `assets/sprites/lili/manifest.json`): kockánkénti szájpont (a játék rögzítéséhez), hangesemények, a fej befoglaló téglalapja (a peek és a buborék helyéhez), simogatási hitbox.

**Játékok:** kötéljáték (színes, csomózott) és pici plüss: földön 4 forgatási állás, repülés közben 4 kocka pörgés, szájban 8 irányú kis sprite (a szájpontra rajzolva). A pontos kinézet a fotók alapján (D-10).

**Szoba:** padló csempe, hátsó fal az ajtóval, nyitott ajtó (kilátás a zöldbe), nagy ágy (takaróval, eleje külön takaró réteg), konyhai sarokülő (háttámla, ülőlap mint takaró, árnyékos kuckó), etetőtál (tele, kevés, üres), vizestál (tele, üres), pelenka (tiszta, használt, tele), pisi és kaki folt (2-2 változat), csillogó takarítás animáció (6 kocka), esti lámpa fénye.

**Udvar:** fű csempe (3 változat), 2 nagy fa (törzs + lombkorona takaró), 3 bokor (rezzenés animáció 3 kocka), virágágyás, kerítés vagy sövény hátul, a ház fala és ajtaja alul.

**Évszakos díszek:** tavasz: virágos váza bent, virágzó bokrok kint, szálló szirmok. Nyár: erős napfény, pillangók, dinnyeszelet vagy nyári dísz bent. Ősz: tök bent, hulló levelek kint, avar a fű szélén. Tél: karácsonyi izzósor a sarokülő felett (villogó animáció), hó a talajon és a bokrokon, hóesés, lábnyomok.

**Különleges napok:** jan.: ünnepi finomság a tálban + kis szalag. Jún. 27.: torta vagy ajándékdoboz dísz. Szept. 12.: aranyló fénypontok, pitypangszirmok.

**UI ikonok:** idő, akku (töltés villámmal), éhség, energia, kedv, hang ki/be, fény, fogaskerék, buborék ikonok (szív, hangjegy, kérdőjel, felkiáltójel, izzadságcsepp, zzz, étel, kötél, plüss).

### 7.5 A Blender gyártósor

**Szerepek szétválasztása:** a **blender-mcp** az interaktív munkára való (a Claude Code a Blendert vezérelve modellez, beállít, finomít). A **gyártás** (render, pixelesítés, csomagolás) mindig reprodukálható, fejetlen (headless) scripttel fut, hogy bármikor újra lehessen generálni mindent egyetlen paranccsal.

| Lépés | Mit | Eszköz | Kimenet |
|---|---|---|---|
| B0 | Paletta-javaslat a fotókból (k-means a kijelölt területeken: bunda, has, orr, szem) | `tools/palette_from_photos.py` | `assets/palettes/master.gpl` (kézi jóváhagyással) |
| B1 | Jelenet: kamera (ortho, 35°), fény, render beállítások, kollekciók | `tools/blender/scene_setup.py` | `assets/blender/*.blend` |
| B2 | Lili modell. Kiindulás: a korábbi próba `.blend`, ha megvan. Ha nincs: CC0 licencű alap kutyamodell (licencet ellenőrizni), ezt formázzuk chihuahuára | blender-mcp | `lili.blend` |
| B3 | Rig (Rigify vagy egyszerű csontváz), külön bal és jobb fülcsont (a lekonyuló fül saját pozíciója), shape key-ek: szem csukva, száj nyitva, nyelv kint | blender-mcp | |
| B4 | Anyagok: lapos színek a palettából, toon árnyalás (Shader to RGB + Color Ramp, 3 sáv) | blender-mcp | |
| B5 | Animációk akciókként (Action), a 7.4 tábla szerint, szájpont és fejpont üres objektummal (Empty) | blender-mcp | |
| B6 | Render: minden animáció x irány, RGBA, 4x felbontás, `blender -b lili.blend -P tools/blender/render_all.py -- --anim walk` | headless | `assets/raw_renders/` |
| B7 | Pixelesítés (lásd lent) | `tools/pixelize.py` | `assets/sprites/` |
| B8 | Kontaktlap minden animációról + nézegető a szimulátorban (`--anim-viewer`) | `tools/contact_sheet.py` | **jóváhagyási kapu: te nézed meg** |
| B9 | Csomagolás | `tools/pack_assets.py` | `assets/build/assets.pak`, `common/asset_ids.h` |

**Pixelesítő algoritmus (B7):**
1. 4x4-es blokkonként a színt a blokk leggyakoribb (palettára kvantált) színe adja (nem átlagolás, így nem lesz maszatos).
2. Alfa: 50% feletti fedés = látszik, egyébként átlátszó (nincs félátlátszó pixel a sprite-on).
3. Paletta-leképezés a mester-palettára dithering nélkül (Lab térben legközelebbi szín).
4. Kontúr: a külső sziluett köré 1 px sötétbarna (8-szomszédság), a belső fontos éleken (fül, láb) opcionális sötétebb árnyalat.
5. Kézi javítás lehetősége: a script nem írja felül a `sprites/overrides/` mappában lévő, kézzel javított kockákat.
6. Az árnyékot nem sütjük bele: futásidőben rajzolt ellipszis a talajon (így az ágyon is helyes).

**Környezet:** ugyanazzal a kamerával a Blenderben épül, külön kollekcióban a háttér és minden takaró tárgy, így külön rétegekként renderelődnek. A járható teret és a pontokat **nem** képből számoljuk, hanem a `assets/maps/room.json` és `yard.json` írja le (sokszögek, pontok, takaró objektumok sortY értékkel), amit a szimulátor `M` módja ráhelyez a képre ellenőrzésre.

### 7.6 `assets.pak` formátum

```
Fejléc:   magic "LPAK", verzió, bejegyzések száma, a tartalomjegyzék offsetje
Tartalomjegyzék: { id(u16), típus(u8: SPRITE_SET, PALETTE, LUT, MAP, SOUND, FONT), offset(u32), méret(u32) }
SPRITE_SET: animáció id, irányok száma, kockák száma, cella méret, pivot,
            kockánként: RLE 8 bites adatok + szájpont + fejpont + hitbox
PALETTE:   256 x RGB888 (a LUT-okat bootkor számoljuk RGB565-re)
MAP:       járható rács (bitmaszk 60x60), pontok, takarók
SOUND:     16 kHz mono, IMA-ADPCM vagy PCM16
```

Bootkor a teljes pak a PSRAM-ba töltődik (~2 MB), CRC ellenőrzéssel. Ha hibás, a játék helyőrző grafikával indul és figyelmeztet.

### 7.7 Helyőrző grafika (hogy semmi ne várjon a művészetre)

Amíg a végleges sprite-ok nincsenek kész, a szimulátor és a firmware ugyanazzal a formátummal, színes egyszerű formákkal (kapszula-kutya irány-nyíllal, téglalap bútorok) fut. Így a 4-6. réteg teljesen fejleszthető és tesztelhető az M3 befejezése előtt.

### 7.8 Glassmorphism felület

**Állapotsáv (BOOT rövid nyomás vagy ébredés után 4 mp-re):**
- Lekerekített kapszula felül középen: 360x64 fizikai px, sugár 32, a felső kávaívtől 18 px-re.
- Hatás: a mögötte lévő terület elmosása (a logikai pufferből vett sáv, 3 px-es box blur kétszer, majd nagyítás), fehér kitöltés 22% átlátszósággal, 1 px-es fehér keret 50%-kal, a felső belső élen halvány fénycsík.
- Tartalom: idő (óó:pp), akku ikon %-kal (töltéskor villám), Lili 3 kis mutatója (éhség, energia, kedv), hang ikon (érintésre némít), fogaskerék (beállítások).
- Mozgás: lecsúszás 200 ms (ease-out), 4 mp tétlenség után felcsúszik.
- Érintési célterület ≥ 44 px.

**Beállítások lap** (fogaskerékről, ugyanilyen üvegkártya középen): fényerő (3 szint), hangerő és némítás, gondolatbuborékok be/ki, keresési segítség be/ki (D-06), idő beállítása / Wi-Fi időszinkron, újrakezdés (kétszeri megerősítéssel; a Lili-emlékek miatt alapból rejtett: BOOT nyomva + koppintás).

**Betűtípus:** pixeles bitmap betű a magyar ékezetekkel (á é í ó ö ő ú ü ű, nagybetűvel is), 12 px magas natív felbontásban, OFL vagy CC0 licencű forrásból (a licencet ellenőrizni), a `tools/` alatt bitmappá konvertálva. A felület nyelve magyar (D-08).

---

## 8. 4. réteg: Világ, mozgás, reflexek

### 8.1 Koordináták

- Nézetenként logikai 240x240-es sík. `x`: vízszintes, `y`: mélység (a mancsok képernyő-Y-ja), `z`: magasság a padló felett (az ágyon 16).
- Képernyő-pozíció: `(x, y - z)`. Rendezési kulcs: `y`, egyenlőség esetén a réteg-prioritás.
- Minden nézet mindig szimulálódik, csak a látható renderelődik.

### 8.2 Benti szoba elrendezése (tükrözött, a gazdi kérése szerint)

```
 x:0                    120                    240
 y:0  ┌──────────────── HÁTSÓ FAL ────────────────┐
      │                ┌──AJTÓ──┐   ┌─sarokülő──┐ │
 y:64 │                └──┬─────┘   │ (háttámla)│ │
      │ ┌────ÁGY────┐     │napfény  └───┐ ülő- │ │
      │ │ (takaró)  │      ╲  folt ╱    │ lap  │ │
      │ │  z = 16   │       ╲     ╱     │KUCKÓ │ │
      │ └───────────┘        ╲   ╱      └──────┘ │
      │                ┌PELENKA┐       [étel][víz]│
      │                └───────┘                 │
 y:210│              (képernyő eleje)             │
 y:240└───────────────────────────────────────────┘
```

| Pont | Koordináta (x, y, z) | Szerep |
|---|---|---|
| spot_door | (120, 70, 0) | átjáró az udvarra |
| sunbeam | sokszög (104,70) (136,70) (160,150) (84,150) | délután 13-17 óra, nyáron erősebb |
| bed (talpfelület) | x 14-90, y 84-168, z 16 | spot_on_bed (52, 120, 16); felugrási pont (96, 140) |
| bench (L alakú) | hátsó szár x 150-226, y 64-96; oldalsó szár x 196-226, y 64-176 | |
| spot_nook | (208, 150, 0), Lili nyugat felé néz | alvás, bűnös bújás; az ülőlap takarója sortY 178 |
| bowl_food / bowl_water | (178, 186) / (198, 196) | a sarokülő mellett (H14) |
| pad | x 92-136, y 166-196, közepe (114, 181) | pelenka |
| spot_center | (120, 140) | |
| spot_front | (120, 210) | a gazdi "elé" jön (H1, H2) |
| rope / plush kezdő helye | (70, 196) / (150, 120) | |

### 8.3 Kinti udvar elrendezése

Térbeli logika: a szobában Lili a hátsó falon lévő ajtón megy ki, az udvaron pedig **alul, a ház ajtaján** jelenik meg (mintha a ház felől néznénk az udvart, ahogy a fotón az ajtóból kinéz).

| Elem | Hely | Szerep |
|---|---|---|
| sövény / kerítés | y 0-30 | határ |
| fa 1 / fa 2 | törzs (50, 62) / (196, 50), lomb takaró y 8-70 | árnyék, levélhullás |
| bokor B1 / B2 / B3 | (40, 132) / (202, 142) / (128, 78) | mögöttük 12 px mély rejtekzóna: ott Lili teljesen eltűnik (H13) |
| virágágyás | (176, 196) | díszítés, szaglászás |
| nyílt fű | (120, 150) körül | játék, dobás, zoomies |
| ház fala + ajtó | y 214-240, ajtó x 104-136, spot_door_yard (120, 206) | átjáró a szobába |

### 8.4 Navigáció

- Járhatósági rács: 4x4 logikai px-es cellák (60x60), a `maps/*.json` sokszögeiből építve.
- Útkeresés: A* 8 szomszédsággal, utána húrhúzásos (string-pulling) simítás.
- Kormányzás: "arrive" viselkedés, gyorsulási korlát, irány 8-ra kvantálva hiszterézissel (ne villogjon két irány között).
- Sebességek stílus szerint (logikai px/mp): sleepy 18, tiptoe 20, curious 26, walk 32, trot 48, scurry 64, sprint 90.
- Nézetváltás: Lili a spot_door-hoz megy, az ajtóban "kilép", 0,5 mp múlva a másik nézet ajtópontján jelenik meg. **A kamera nem követi** (a gazdinak kell lapoznia). **A lapozás iránya (a gazdi, 2026-10-21): nem oldalra, hanem felfelé, az ajtón át** (a szoba képe felcsúszik, az udvar alulról jön), és **az ajtóra koppintás is vált**, nem csak a KEY3 és a húzás. Ha a `CONFIG_LILI_SEARCH_HINT` be van kapcsolva, a nézet szélén 8 mp-ig kis mancsnyom jelzi, merre ment.

### 8.5 Takarás és bújás

- Takaró tárgyak saját sortY értékkel (sarokülő ülőlapja, ágy eleje, bokrok, fatörzsek, lombok).
- **Sarokülő kuckó:** Lili a spot_nook-on áll, az ülőlap takarója utána rajzolódik, de a `guilty_peek` animáció fejpontja az ülőlap alsó éle alá lóg, így pont a feje és a szemei látszanak (H10).
- **Bokor:** ha Lili talppontja a rejtekzónában van, `hidden_behind_bush = true`, a bokor teljesen takarja. Belépéskor a bokor egyszer megrezzen (3 kocka).

### 8.6 Ugrás és magasság

Az ágyra fel- és leugrás 0,5 mp-es parabola, a `z` 0 és 16 között. Az ágyon a rendezési kulcs az ágy talpfelületéhez igazodik.

### 8.7 A terv-végrehajtó (az agy 3 lépésének "teste")

Minden akció egy primitív-sorozatra képződik le (`plan_exec.c` adattábla):

| Primitív | Jelentés |
|---|---|
| GOTO(cél) | célpont: pont, játék, gazdi eleje, véletlen hely a nézetben |
| ANIM(id, idő) | animáció lejátszása adott ideig vagy egyszer |
| PICKUP(játék) / DROP | szájpontra vétel, letétel |
| CHANGE_VIEW | ajtón át a másik nézetbe |
| JUMP(z) | fel / le |
| HIDE(hely) / UNHIDE | kuckó vagy bokor |
| WAIT(idő) | |
| FACE(kifejezés) | arc beállítása |
| SFX(id) | hang |

Szabályok:
- Minden akciónak van **előfeltétele** (pl. `hide_under_bench` csak a szobában, `eat` csak ha van étel). Ha nem teljesül, a lépés kimarad. Ha mind a 3 kimarad, azonnal új döntést kérünk.
- Lépésenkénti időkorlát (alapból 20 mp), utána a lépés lezárul.
- A sürgősség (1-9) a sebességet (stílus mellett) és a következő kérdés idejét befolyásolja.
- A terv végén vagy jelentős változáskor új kérdés megy az agynak (10.6). Amíg a válasz nem jön meg, Lili az utolsó akció "üresjárati" változatát játssza (ül, szaglászik, csóválja a farkát), soha nem fagy le.
- **Habozás:** ha a válaszban az első két jelölt közötti különbség kicsi (`top2_margin < 0,15`), Lili először `head_tilt`-et játszik, csak utána indul (a pocket-tank "torn" ötlete).

### 8.8 Reflexek (azonnaliak, 0 ms, az agy megkerülésével)

| Prio | ID | Kiváltó | Feltétel | Válasz | Utána |
|---|---|---|---|---|---|
| 1 | R1 Takarítás-feloldás | az utolsó folt feltakarítva | `guilty` | a kuckóból előrohan (trot), farokcsóválás, `pant_smile`, szív buborék | `guilty=0`, kedv +2, új kérdés |
| 2 | R2 Hívás | `VOICE_WHISTLE` vagy `VOICE_CALL_NAME` | nem alszik mélyen, nem bűnös | fülhegyezés, előbújik (bokorból, ágyról), a gazdi elé szalad (spot_front) | ha más nézetben van: az ajtón át jön, és a kamerát nem váltjuk |
| 3 | R3 Simogatás | `EV_PET_*` Lili hitboxán | nem rejtőzik | megáll, `pet_enjoy`, enyhe odabújás | kedv +, magány -, unalom -; 2 mp-cel a vége után új kérdés |
| 4 | R4 Dobás | `EV_FLICK` játékon | ébren van | `startle` 1 kocka, utána sprint a játékhoz, `pounce`, visszahozza a spot_front-ra | unalom -3, energia -0,3 |
| 5 | R5 Fogócska | `EV_APPROACH` < 70 px | `play_bow` + játék a szájában vagy előtte | felkapja, elszalad egy távoli pontra, ott visszafordul és újra `play_bow` | max. 4 kör, utána fáradtan leül |
| 6 | R6 Üdvözlés | ébredés, offline > 15 perc | `guilty == 0` | H1: játékot hoz, előtted rágcsálja | ha bűnös: a kuckóból néz (H10) |
| 7 | R7 Rázás | `SHAKE` | | `startle`, egyszer `bark_head_flip` | 3 mp |
| 8 | R8 Hang | `VOICE_ACTIVITY` | ébren | felkapja a fejét, félrebillenti (`head_tilt`) | |
| 9 | R9 Koppintás a padlóra | `EV_TAP` szabad padlón | ébren, nem bűnös | odanéz, 50%-ban odasétál | |

Alvó Lilit a R2, R3, R6, R7 felébreszti (álmos ébredés animációval), a többi nem.

### 8.9 Játékfizika (`toys.c`)

- Állapot: `x, y, z, vx, vy, vz`. A dobás sebessége az ujj sebességéből (skála 0,35, maximum 220 logikai px/mp), induló `vz` 60.
- Gravitáció 300 px/mp², pattanás 0,4-es visszaverődéssel, talaj-súrlódás: kötél 2,8/mp, plüss 3,5/mp.
- A járhatósági rács szélei falként visszaverik a játékot. Bokor és fa mögé is gurulhat, ott takarásban van.
- A játék nem hagyja el a nézetet, csak Lili szájában megy át a másikba.

### 8.10 Gondozási interakciók

| Koppintás | Hatás |
|---|---|
| üres vagy kevés tál | feltöltés (csillogás, hang), 3 adag |
| használt vagy tele pelenka | csere tisztára |
| folt a padlón | feltakarítás csillogással; az utolsó után R1 |
| Lilire | rövid örömreakció (farokcsóválás, buborék, ha be van kapcsolva) |

---

## 9. 5. réteg: Igények, idő, évszakok, mentés

### 9.1 Igények (0-10, a kiinduló értékek hangolhatók a `config.h`-ban)

| Igény | Nő / csökken óránként (ébren) | Offline óránként | Kielégíti | Hatás |
|---|---|---|---|---|
| hunger | +0,35 | +0,30 | evés (1 adag -> 0) | 5 felett enni megy (ha van étel) |
| energy | játék -1,5, alapjárat -0,4, alvás +2,5 | alvás +2,5 | alvás | 3 alatt álmos stílus, 2 alatt alvás |
| happy | 7-es alapérték felé húz 0,5/óra | magány > 7 esetén az 5 felé | simogatás, játék, üdvözlés | arckifejezés, játékkedv |
| bored | +1,2 interakció nélkül | +0,8 | játék -3, felfedezés -1 | felfedezés, rosszalkodás, ágyra szökés |
| lonely | -2 ha a gazdi aktív | +0,6 | simogatás, hívás, jelenlét | ágyra szökés, rosszalkodás |
| bladder | +0,45, evés után +2 | +0,45 | pelenka vagy padló | 8 felett mindenképp mennie kell |
| guilty | bool | | takarítás | bújás |

Ezekkel az értékekkel a gondozás ritka (a gazdi kérése szerint): nagyjából naponta egy tálfeltöltés és egy pelenkacsere elég.

### 9.2 Pelenka, rosszalkodás, bűnösség (állapotgép)

```
NORMAL --(bladder >= 8)--> DÖNTÉS (az agy választ: use_pad vagy mischief_floor)
   use_pad: pelenka -> használt (2. használat után "tele"), NEM bűnös, vissza NORMAL
   mischief_floor: 1-2 folt a padlón (max. 3 összesen) -> GUILTY_HIDING
GUILTY_HIDING: Lili a kuckóban, guilty_peek; csak R1 oldja fel (minden folt eltűnt)
FORGIVEN_RUSH: előrohanás (R1), utána NORMAL
```

A rosszalkodás esélye (a tanár szabálymotorban, 10.7):
`p = 0,05 + 0,08 * max(0, bored - 5) + 0,08 * max(0, lonely - 5) + (pelenka tele ? 0,5 : 0)`, legfeljebb 0,85. **Szept. 12-én p = 0** (H15), és ott az `mischief_floor` akció kódolási szinten is tiltott (10.5).

### 9.3 Napszakok

| Napszak | Idő | Világ | Lili |
|---|---|---|---|
| reggel | 07:00-12:59 | friss, világos | aktív |
| délután | 13:00-17:59 | napfényfolt az ajtónál | napozás (H6) |
| este | 18:00-21:59 | meleg lámpafény bent, szürkület kint | lassul |
| éjszaka | 22:00-06:59 | sötétkék kint, szentjánosbogarak nyáron | a kuckóban alszik; ha felébreszted, álmosan köszön, rövid simogatás után visszaalszik |

### 9.4 Évszakok (a dátumból)

Tavasz márc.-máj., nyár jún.-aug., ősz szept.-nov., tél dec.-febr. A tartalmat lásd 7.4. Télen kint a hóban lábnyomok maradnak (32 elemes körpuffer, lassan halványulnak).

### 9.5 Különleges napok

| Dátum | Név | Látvány | Viselkedés |
|---|---|---|---|
| jan. 15. (D-04, a 2019. januári befogadás) | Lili napja | finomság a tálban, szalag | extra játékos, több zoomies |
| jún. 27. | A gazdi születésnapja | torta / ajándék dísz | felváltva hozza a kötelet és a plüsst, play_bow |
| szept. 12. | Lili emléknapja | aranyló fény, lebegő fénypontok, pitypangszirmok | nincs rosszalkodás, bújós, a képernyő közelében marad (H15) |

### 9.6 Távollét-szimuláció (offline idő)

Ébredéskor az RTC-ből: `Δt = most - utolsó_mentés`.

1. `Δt` levágása 30 napra (azon túl minden telített).
2. 10 perces lépésekben az igények frissülnek (offline ütemekkel).
3. Események a szabálymotor statisztikai változatával (gyors, nem az LLM): pisilés (pelenka vagy padló a 9.2 képlet szerint), alvás, ágyra szökés (`lonely ≥ 7 és bored ≥ 6` -> lépésenként 8% eséllyel), evés, ha van étel.
4. Az ébredési jelenet prioritása:
   1. van folt a padlón -> **BŰNÖS JELENET** (a szoba üresnek tűnik, a kuckóból les),
   2. az ágyon van -> **RAJTAKAPÁS** (alszik, felkapja a fejét, leugrik),
   3. nappal van és jó idő (nem tél éjszaka) és energia > 5 -> 40% eséllyel **KINT VAN** (szaglászik vagy bokor mögött),
   4. éjszaka -> **ALSZIK A KUCKÓBAN**,
   5. különben **ÜDVÖZLÉS** (H1).
5. Determinisztikus magolt PRNG (a selftest ellenőrzi az eloszlásokat).

### 9.7 Mentés

- NVS blob `lili_save`, verziózott struktúra, **csak a végén bővülhet** (a pocket-tank szabálya), CRC-vel. A beállítások külön kulcsban.
- Tartalom: verzió, utolsó mentés ideje, igények, `guilty`, foltok helye, pelenka és tálak állapota, Lili nézete, helye és tevékenysége, játékok helye, beállítások, statisztikák (pl. hány simogatás, mióta él a kütyü), első indulás dátuma.
- Mentés: 5 percenként, alvás és kikapcsolás előtt, gondozási eseménykor (takarítás, etetés). Áramszünet esetén legfeljebb 5 perc vész el.

---

## 10. 6. réteg: Lili agya (LLM, tanítás, kvantálás)

### 10.1 Szerep

Az agy 3-8 mp-enként kap egy rövid "helyzetjelentést", és egy 3 lépéses tervet ad vissza mozgásstílussal, arckifejezéssel, buborékkal és sürgősséggel. A reflexek és a fizika nélküle is futnak. Fejlesztés közben és a távollét szimulációjában a szabálymotor ugyanezt a felületet szolgálja ki.

### 10.2 A valós késleltetés és a keret

- Mért adat ezen a panelen (pocket-tank, 14,3M modell): ~10 token/mp, azaz **minden token ~0,1 mp**, akár a prompt része, akár a válaszé.
- **Keretünk:** legfeljebb 34 token döntésenként, tehát ~3,4 mp a 14,3M modellel (a kisebb modellekkel arányosan kevesebb, 10.9).
- Ez bőven elég: egy terv 10-40 mp-ig tart, a reflexek azonnaliak.

### 10.3 Bemeneti séma v1 (minden mező egyetlen token)

Formátum: `<bos> F1 F2 ... F24 =>` (26 token)

| # | Mező | Tokenek |
|---|---|---|
| F1 | hely | `loc_room loc_yard` |
| F2 | pont | `spot_center spot_front spot_nook spot_bowls spot_pad spot_bed spot_on_bed spot_door spot_sunbeam spot_bush spot_tree spot_grass` |
| F3 | napszak | `tod_morning tod_afternoon tod_evening tod_night` |
| F4 | évszak | `season_spring season_summer season_autumn season_winter` |
| F5 | nap | `day_normal day_adopt day_bday day_memorial` |
| F6-F11 | igények (0-9 vödrök) | `hunger_0..9 energy_0..9 happy_0..9 bored_0..9 lonely_0..9 bladder_0..9` |
| F12 | bűnös | `guilty_0 guilty_1` |
| F13 | pelenka | `pad_clean pad_used pad_full` |
| F14 | folt | `mess_0 mess_1 mess_many` |
| F15 | tálak | `bowls_full bowls_low bowls_empty` |
| F16 | gazdi | `owner_here owner_back_short owner_back_long owner_idle` |
| F17 | nézet | `view_same view_other` (a gazdi ugyanazt a helyszínt nézi-e) |
| F18 | jelzés | `sig_none sig_whistle sig_name sig_voice sig_shake sig_pet sig_tap sig_toy_thrown` |
| F19 | kötél | `rope_mouth rope_near rope_far rope_other` |
| F20 | plüss | `plush_mouth plush_near plush_far plush_other` |
| F21 | nap | `sun_on sun_off` |
| F22 | előző tevékenység | `last_play last_rest last_eat last_explore last_greet last_mischief last_social last_none` |
| F23 | mióta csinálja | `since_short since_mid since_long` (unalom elleni, a pocket-tank boredom tanulsága) |
| F24 | szereplő | `actor_lili` (később `actor_bella`) |

### 10.4 Kimeneti séma v1 (7 fix slot, címkék nélkül)

`A1 A2 A3 STYLE FACE BUBBLE URG` (7 token), az `A2` és `A3` lehet `act_none`.

**Akciók (54):**
- Mozgás: `goto_front follow_finger wander sniff_around go_door go_nook go_bowls go_pad go_bed go_sunbeam go_bush go_tree go_toy`
- Nézetváltás: `enter_yard enter_room`
- Játék: `pickup_rope pickup_plush bring_toy drop_toy_front chew_toy play_bow zoomies run_with_toy toss_toy`
- Test és kommunikáció: `sit lie_down bark huff head_tilt wag beg ask_pet pant_idle stretch`
- Szükségletek: `eat drink use_pad mischief_floor sleep_nook sleep_bed nap_sunbeam`
- Lopakodás és bújás: `look_around jump_on_bed jump_off_bed hide_bench peek_bench hide_bush peek_bush`
- Évszakos: `chase_leaf chase_butterfly play_snow sniff_flowers`
- Kitöltő: `act_none`

**Stílus (7):** `st_walk st_trot st_sprint st_tiptoe st_scurry st_sleepy st_curious`
**Arc (10):** `fc_neutral fc_pant_smile fc_big_eyes fc_ears_perk fc_relaxed fc_head_tilt fc_guilty fc_sleepy fc_bliss fc_pout`
**Buborék (10):** `bb_none bb_heart bb_note bb_question bb_exclaim bb_sweat bb_zzz bb_food bb_rope bb_plush`
**Sürgősség (9):** `urg_1 .. urg_9`

**Szókincs összesen:** 5 speciális (`<pad> <bos> <eos> => <unk>`) + 134 bemeneti + 90 kimeneti = 229, **256-ra kerekítve** (27 hely tartalék Bellának és későbbi mezőknek). A szótár-mátrix 4 biten ~50 KB.

**Egyetlen igazságforrás:** `model/schema_lili.json`. Ebből generálódik a `common/llm/vocab.h`, a `tokenizer.bin`, a Python kódoló és a vizualizáló listái, így a C és a Python soha nem csúszhat el.

### 10.5 Kényszerített dekódolás és mintavétel

- Slotonként csak az odaillő tokenek engedélyezettek (A1-ben csak akció, a 4. helyen csak stílus stb.), a többi logit `-inf`. Így **100% az érvényes kimenet**, és a 8. kimeneti tokent (`<eos>`) sem kell kiszámolni.
- Környezeti maszk (ez nyelvtan, nem felülbírálás): a nézethez nem illő akciók tiltva (pl. `hide_bench` az udvaron, `hide_bush` a szobában), `day_memorial` mellett a `mischief_floor` tiltva, tél nélkül a `play_snow` tiltva.
- Mintavétel: hőmérséklet 0,8 (a pocket-tank igazolta, hogy a mintavétel változatosabbá teszi a viselkedést a biztonság rovása nélkül). `--greedy` kapcsolóval kikapcsolható.
- A `top2_margin` az A1 slotból jön vissza (habozás, 8.7).

### 10.6 Mikor kérdezünk

Új kérdés, ha: a terv véget ért vagy elbukott; egy igény vödre átlép egy küszöböt (pl. hunger 5, bladder 8, energy 3); jelzés érkezett; Lili nézetet váltott; folt keletkezett; a gazdi elkezdte vagy abbahagyta a figyelést. Legkorábban 3 mp-cel az előző után, legkésőbb 20 mp-enként.

### 10.7 A tanár (ingyenes, két szinten)

**A szint: szabálymotor tanár (elsődleges, kötelező).**
- `model/teacher_rules.yaml`: ~40 tervsablon, mindegyik egy H-szokáshoz kötve (3.2). Példa: `T_greet_rope (H1)`: feltétel `owner_back_* és guilty_0`, pontszám `6 + 0,4 * happy`, terv `pickup_rope / bring_toy / chew_toy`, `st_trot fc_pant_smile bb_heart`, sürgősség 9.
- Döntés: minden érvényes sablon pontszámot kap, softmax (τ = 0,6) szerinti mintavétel, a véletlen elemek (melyik játék, A3 kitöltése) súlyozottan.
- A YAML-ből a `tools/gen_rules.py` C táblát generál (`common/brain_rules_table.h`), így ugyanez fut a szimulátorban, a távollét szimulációban és tartalékként az eszközön.
- Előny: ingyenes, azonnali, pontosan azt tanítja, amit leírtál, és te is olvasni, hangolni tudod.

**B szint: helyi LLM tanár (opcionális gazdagítás).**
- Ollama a gépeden, egy 8 GB VRAM-ba férő modell (a gemma4:26b a pocket-tankban ~18 GB volt, ez nem fér el, csak lassú CPU-s kiszervezéssel).
- Csak a "kétes" helyzeteket címkézi újra (ahol a szabálymotor első két sablonja közel van), és **csak a meglévő sablonok közül választhat** (feleletválasztós prompt Lili leírásával), így a kimenete mindig érvényes.
- A pocket-tank tanulsága: **a prompt (nálunk a szabályok és a prompt) a teljes adathalmaz DNS-e.** Egy mondat 45%-ról 14%-ra vitt le egy célt. Ezért minden hosszabb generálás előtt kötelező a 7 perces `prompt_check` és az eloszlás-ellenőrzés a vizualizálóban.

Őszinte megjegyzés: ha a tanár szabálymotor, a modell ennek a szabálymotornak a valószínűségi eloszlását tanulja meg. A nyereség: simább, általánosító viselkedés olyan helyzetekben is, amiket a szabályok nem írtak le pontosan, a saját eloszlásból mintavételezett változatosság, a habozás jelensége, és az, hogy végigtanulod a teljes tanítási láncot. A B szinttel pedig a szabályokon túli árnyalatok is bekerülnek.

### 10.8 Tanítóadat

- Forrás: a fejetlen szimulátor a valódi játék állapot-eloszlásából: `lili_sim --headless --brain rules --dump-traces model/out/traces_s1.jsonl --seed 1 --hours 200`.
- Egy "virtuális gazdi" robot véletlenszerű, de életszerű napirenddel: ébresztések, távollétek (perctől napokig), simogatás, dobás, hívás, etetés, takarítás (vagy elhanyagolás).
- Ritka helyzetek pótlása állapot-perturbációval: különleges napok, téli éjszaka, bűnös állapot, kint bokorban, sok folt.
- Cél: **~150 000 pár**, duplikátumszűrés, tanító/validációs szétválasztás magonként (nincs szivárgás), H-szokásonkénti és akciónkénti eloszlás a vizualizálóban.

### 10.9 Tanítás az RTX 5060-on

Alap: a pocket-tank `model/train.py` (llama2.c alapú), Lili sémájára állítva.

| Méret | dim / rétegek / fejek / hidden | Paraméter | Q4 fájl | Becsült sebesség a panelen |
|---|---|---|---|---|
| S | 256 / 6 / 8 / 768 | ~5,1M | ~2,7 MB | a legkisebb, mérendő (a súlyolvasás arányában akár ~2-3x gyorsabb) |
| M | 320 / 8 / 8 / 864 | ~8,8M | ~4,7 MB | mérendő |
| L | 384 / 8 / 8 / 1024 | ~14,3M | ~7,56 MB | ~10 token/mp (mért) |

- Hiperparaméterek (kiindulás): `max_seq_len 48`, batch 128, AdamW, lr 6e-4 koszinusz, 200 bemelegítő lépés, 6000 iteráció, weight decay 0,1, dropout 0,1, bf16 autocast, mester súlyok fp32.
- **A veszteséget csak a 7 kimeneti tokenen számoljuk** (a prompt tokenek maszkolva), így a modell kapacitása a döntésre megy, nem a bemenet "megjóslására".
- A tanítás percek kérdése ezen a kártyán (a pocket-tank 14M modellje Mac-en ~30 perc volt), a valós időt a `docs/stats.md`-be írjuk.
- **Méretválasztás szabálya:** a legkisebb méret, amelyik teljesíti a 10.11 céljait. Ha az S vagy M jó, a kisebb modell gyorsabb döntést és kevesebb flash-terhelést ad.

### 10.10 Export és kvantálás

- `model/llama2.c/export.py` (fp32) -> `model/export_q4.py` (64-es csoportok, fp16 skálák) -> `model/out/model_q4.bin` + `tokenizer.bin` + trailer, **a pocket-tank formátumában**, hogy a C motor változtatás nélkül fusson.
- A modell trailer a sémaverziót és a szókincs hash-ét is tartalmazza: ha a firmware sémája eltér, a modell elutasítva, a szabálymotor fut (és a napló jelzi).

### 10.11 Értékelés és elfogadási célok

| Mérés | Cél |
|---|---|
| Érvényes kimenet | 100% (kényszerített dekódolással) |
| A1 egyezés a tanár legvalószínűbb választásával, friss helyzeteken | ≥ 80% |
| Tanár címkéje a modell top-2-jében | ≥ 93% |
| Q4 vs fp32 A1 egyezés | ≥ 95% |
| Döntési idő a panelen | ≤ 3,5 mp (M1 után pontosítva) |

**Viselkedési próbák** (`model/probes_lili.py`, mindegyik ≥ 50 mintán):

| ID | Helyzet | Elvárás |
|---|---|---|
| P1 | `owner_back_long guilty_0` | P(A1 ∈ {pickup_rope, pickup_plush, bring_toy}) ≥ 0,8 (H1) |
| P2 | `guilty_1 mess_1` | P(A1 ∈ {hide_bench, peek_bench} vagy már ott van) ≥ 0,9 (H10) |
| P3 | `day_memorial` | P(mischief_floor) = 0 (maszk), P(közelség: goto_front, ask_pet) ≥ 0,5 (H15) |
| P4 | `tod_night energy_2` | P(sleep_nook) ≥ 0,8 (H11) |
| P5 | `tod_afternoon sun_on loc_room energy_4..7` | P(go_sunbeam vagy nap_sunbeam) ≥ 0,4 (H6) |
| P6 | `owner_back_long lonely_8 bored_7` offline jelenet | P(sleep_bed) ≥ 0,3 (H7) |
| P7 | `bladder_9 pad_full` | P(mischief_floor) ≥ 0,4 |
| P8 | `bladder_9 pad_clean bored_2 lonely_2` | P(use_pad) ≥ 0,85 (H8) |
| P9 | `loc_yard spot_bush` | P(hide_bush vagy peek_bush) ≥ 0,3 (H13) |
| P10 | `hunger_8 bowls_full` | P(go_bowls vagy eat) ≥ 0,85 (H14) |
| P11 | `bored_9 since_long` | P(ugyanaz, mint last) ≤ 0,15 (unalom) |
| P12 | `happy_9 rope_near owner_here` | P(play_bow vagy bring_toy vagy zoomies) ≥ 0,6 (H3, H12) |

### 10.12 Az eszközön

- A modell a `model` partícióról mmap-pel olvasva, a pocket-tank motorjával a Core 1-en, kötegelt prefill-lel (batched prefill).
- KV cache a PSRAM-ban, hossza 48.
- Flashelés: `esptool.py --chip esp32s3 -p <port> write_flash 0x290000 model/out/model_q4.bin` (egyszer kell, illetve modellcserekor).
- Boot napló: döntésenkénti idő, token/mp, top2_margin.

### 10.13 Vizualizációs műszerfal (`tools/llm_visualizer.py`, Gradio, a gazdi kérése)

| Fül | Tartalom |
|---|---|
| 1 Tanítás élőben | train/val veszteség, kimeneti token pontosság, lr, VRAM, token/mp; epochonként 5 véletlen helyzet: tanár válasza vs modell válasza egymás mellett |
| 2 Lili agy-labor | minden bemeneti mező csúszkával vagy legördülővel; azonnali futtatás (fp32 és Q4); a 3 lépéses terv folyamatábraként, mellette az arckifejezés és a buborék előnézeti képe |
| 3 Gondolatolvasó | slotonként a top-10 jelölt valószínűsége oszlopdiagramon; figyelem-hőtérkép: melyik bemeneti token mennyire számított a döntésnél |
| 4 Kvantálás | fp32 vs Q4 ugyanarra a helyzetre; súly-hisztogram előtte és utána; koszinusz-hasonlóság rétegenként; egyezési arány 1000 helyzeten, zöld pipával |
| 5 Adat-elemző | akció-, stílus-, arc-eloszlás az adathalmazban; H-szokásonkénti lefedettség; a próbák (10.11) eredményei egy táblában, piros/zöld |

A Q4 futtatás a hoszton lefordított C motorral történik (`model/runw` mintájára), hogy a vizualizáló pontosan azt mutassa, amit az eszköz csinálna.

### 10.14 Tanulási útvonal (mit érts meg az egyes lépéseknél)

1. **Séma és tokenizálás (M7.1):** miért egy token egy mező, mit jelent a szókincs, és miért kerül pénzbe (időbe) minden token az eszközön.
2. **Adat (M7.2):** a vizualizáló 5. fülén nézd meg, mit tanul meg a modell. Rossz eloszlás = rossz Lili.
3. **Tanítás (M7.3):** az 1. fülön a veszteséggörbe: mikor tanul, mikor magol be (train lemegy, val felmegy).
4. **Mit "gondol" (M7.4):** a 3. fülön a valószínűségek és a figyelem.
5. **Kvantálás (M8.1):** a 4. fülön a hisztogram: így lesz 32 bitből 4, és látod, hogy a döntés mégsem változik.
6. **Eszköz (M8.2):** a soros konzolon a token/mp és a döntési idő: a sávszélesség korlátja a valóságban.

---

## 11. 7. réteg: Hangok, hangfelismerés, Bella-kész szerkezet

### 11.1 Hangkészlet

| ID | Forrás | Hossz | Kiváltó | Hangerő |
|---|---|---|---|---|
| bark_1..3 | Lili videóiból (vagy tartalék) | 0,2-0,5 mp | `bark_head_flip` 3. kocka | 0 dB |
| huff_1..2 | Lili videóiból | 0,3 mp | `huff` 2. kocka | -3 dB |
| pant_loop | videó vagy szintetikus | 1 mp ismétlődő | `pant_smile`, közel a képernyőhöz | -12 dB |
| snore_soft | szintetikus | 2 mp | `sleep_curl` (csak ha nincs éjszakai némítás) | -18 dB |
| ui_fill, ui_clean, ui_pad, ui_toss, ui_open | szintetikus, retro | 0,1-0,3 mp | gondozási koppintások, UI | -6 dB |
| jingle_bday, jingle_adopt | szintetikus | 2 mp | különleges nap első ébredése | -6 dB |

A memóriakeret ~0,5 MB. Éjszaka (22-07) a Lili-hangok -10 dB-lel halkabbak.

### 11.2 Lili valódi hangja videóból (`tools/extract_lili_audio.py`)

1. A videókat az `assets/raw_videos/` mappába másolod.
2. FFmpeg: hangsáv 16 kHz mono WAV-ba.
3. librosa onset-detektálás: rövid, impulzusszerű hangok (vakkantás, puffogás) jelöltjei.
4. noisereduce spektrális zajszűrés, 5 ms fade-in / fade-out, hangerő-normalizálás.
5. Kis Gradio "hangválogató" (a vizualizáló 6. füle vagy külön oldal): meghallgatod a jelölteket, megjelölöd: `bark`, `huff`, `pant`, eldobni.
6. A kiválasztottak a `assets/sounds/`-ba, a `pack_assets.py` csomagolja őket.

### 11.3 Lejátszás

A pocket-tank keverője, animációs eseményhez szinkronizálva (a kocka megjelenésével egy időben indul). A beállításokban és a BOOT hosszú nyomásával némítható.

### 11.4 Hangfelismerés

| Szint | Mit | Módszer | Állapot |
|---|---|---|---|
| 1 | Fütty | FFT sávcsúcs (5.8) | v1 kötelező |
| 2 | Hang / taps | energia és időtartam (5.8) | v1 kötelező |
| 3 | "Lili" név | két jelölt út, az M9.4 kísérlet dönt: (a) saját kis kulcsszó-modell (TFLite Micro), amit a te hangodon tanítunk (pár száz felvétel + zajos változatok), (b) az Espressif ESP-SR parancsfelismerője fonetikus átírással, ha a memória és a CPU engedi | kísérleti, alapból ki (D-07) |

Elfogadás a 3. szintre: 20 kimondásból ≥ 16 felismerés, és 1 óra háttérbeszédben ≤ 1 téves riasztás. Ha nem teljesül, a név-funkció kikapcsolva marad, a fütty mindig működik.

### 11.5 Bella-kész szabályok (most kötelező, hogy később ne kelljen átírni)

- Minden szereplő-állapot tömbben: `DogActor_t actors[MAX_ACTORS]`, v1-ben `MAX_ACTORS = 1`, `actors[0]` = Lili.
- Semmilyen név nem tartalmazza a "lili" szót a szereplő-logikában (a `lili_` előtag csak a projektre utal, a szereplő `actor`).
- Az asset-ek szereplőnként külön sprite-készletben (`SPRITE_SET_ACTOR_0`), a paletta-rámpák szereplőnként.
- A séma F24 mezője és a 27 tartalék token előkészíti a `actor_bella`, `friend_near/far/none` mezőket.
- A reflexek és a terv-végrehajtó szereplőnként fut, a kérdések (mint a pocket-tankban több halnál) körforgásban osztoznak egy agyon.

---

## 12. Mérföldkövek (pontról pontra)

Jelölés: **[G]** = a gazdi (te) feladata vagy jóváhagyása kell. Minden mérföldkő végén: `make check` zöld, `docs/STATUS.md` frissítve.

### M0 Környezet és fork

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M0.1 | Fejlesztői környezet a 6.1-6.2 szerint | WSL-ben `nvidia-smi`, `idf.py --version`, `python -c "import torch; print(torch.cuda.is_available())"` igazat ad |
| M0.2 | Privát fork, `upstream` remote, a fork commit a `docs/UPSTREAM.md`-ben, `CLAUDE.md` és ez a dokumentum a repóban | a repó privát, a fájlok bent vannak |
| M0.3 | A pocket-tank kötelező olvasmányai (4.1), rövid összefoglaló a `docs/UPSTREAM.md`-be: mit veszünk át, hol | a dokumentum elkészült |
| M0.4 | A pocket-tank szimulátor négyzetes módban: `make SQUARE=1 && ./fishsim-square`, `make SQUARE=1 check` | fut, a tesztek zöldek |

### M1 Hardver validáció (a kezedben lévő panelen)

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M1.1 [G] | Gyári flash mentése: `esptool.py --chip esp32s3 -p <port> read_flash 0 0x1000000 backup/factory_216.bin` | a fájl 16 MB, biztonságos helyen |
| M1.2 | A pocket-tank square build `--full` flashelése (`tools/flash_square.sh --full`) | a halak úsznak a paneleden |
| M1.3 [G] | Gombok: melyik fizikai gomb melyik GPIO; a 5.4 tábla frissítése | táblázat a `docs/board-notes-lili.md`-ben |
| M1.4 | QMI8658 INT láb a kapcsolási rajzon; wake-on-motion teszt könnyű alvásból | működik, vagy dokumentált tartalék (periodikus ébredés) |
| M1.5 | Fogyasztás ACTIVE, DIM, könnyű alvás állapotban (az AXP2101 mérőjével, a pocket-tank batlog mintájára) | számok a `docs/stats.md`-ben, a DESK türelmi idő eldöntve |
| M1.6 | Diagnosztikai app (`firmware/diag/`): ES7210 élesítése, szintmérő, spektrum, füttydetektor | füttyre a kijelzőn "FÜTTY", 10 próbából ≥ 9 |
| M1.7 [G] | Saroktesztminta (32/36/40/44 px ívek) | eldöntött `CORNER_R` |
| M1.8 | RTC beállítás soros paranccsal, kikapcsolás után is megmarad | igen |

### M2 Váz: Lili játékmodul a hal helyett

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M2.1 | `lili_types.h`, `config.h`; a halas játék kikerül a buildből, a portok maradnak | sim és firmware fordul, üres képpel fut |
| M2.2 | `render8.c`: 240x240 8 bites puffer, paletta LUT, 2x nagyítás a flush-ban, stripe küldés | `--bench` és eszköz: üres jelenet ≥ 30 fps, a belső heap ≥ 30 KB szabad |
| M2.3 | Kávamaszk, vignetta, sarokzóna debug réteg | a szimulátor és a panel képe egyezik |
| M2.4 | `maps/room.json` és `yard.json` betöltése, helyőrző grafika (7.7), nézetváltás húzással és KEY3-mal | mindkét nézet látszik, a váltás működik |
| M2.5 | Inspector panel (6.3) | minden gomb eseményt küld, a konzol kiírja |
| M2.6 | Eszköz build a helyőrző világgal | ≥ 25 fps a panelen, mért szám a `stats.md`-ben |

### M3 Grafikai gyártósor és Lili megszületése

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M3.1 | blender-mcp beállítás, `scene_setup.py` (kamera 35°, fény, kollekciók) | a Claude Code a Blenderben látja a jelenetet |
| M3.2 [G] | `palette_from_photos.py`, mester-paletta jóváhagyása | `master.gpl` elfogadva |
| M3.3 | Teljes lánc helyőrző kutyával: render -> pixelesítés -> csomagolás -> a szimulátorban látszik | egy parancs (`tools/build_assets.sh`) mindent újragenerál |
| M3.4 [G] | Lili modell (B2-B4), 8 irányú forgóképes kontaktlap | jóváhagyod: fül (D-03), homlokcsík, orr, szemek, farok, bunda |
| M3.5 [G] | Animációk 1. köre: A01-A09 (álló, ülő, járás, futás, lopakodás, lihegés) | kontaktlap és `--anim-viewer` jóváhagyva |
| M3.6 [G] | Animációk 2. köre: A10-A27 | jóváhagyva |
| M3.7 [G] | Szoba és udvar, takarók, évszakos díszek, különleges napok | a 4 évszak x 4 napszak mindkét nézetben jóváhagyva (`--snapshot`) |
| M3.8 | Játékok, foltok, pelenka, tálak, UI ikonok | minden asset a pak-ban, a pak < 3 MB |

Megjegyzés: az M4-M6 a helyőrző grafikával párhuzamosan haladhat az M3-mal.

### M4 Világ, mozgás, reflexek

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M4.1 | Járhatósági rács, A*, simítás, kormányzás, 8 irányú animációválasztás | `--selftest-nav` zöld |
| M4.2 | Y-rendezés, takarók, kuckó-kukucskálás, bokorrejtek, ágy magassága | a debug képeken helyes; `--selftest-render` zöld |
| M4.3 | `gesture.c` mind a 7 gesztussal (5.3), szimulátorban és eszközön | a konzol helyes eseményeket ír, a panelen kipróbálva [G] |
| M4.4 | Terv-végrehajtó: primitívek, mind az 54 akció leképezése, előfeltételek, időkorlátok | minden akció kiváltható az inspectorból |
| M4.5 | R1-R9 reflexek a prioritásokkal | `--selftest-reflex` zöld |
| M4.6 | Játékfizika | `--selftest-toys` zöld, a dobás jól érződik a panelen [G] |
| M4.7 | Gondozási koppintások (8.10) | működnek |

### M5 Igények, idő, mentés, energia

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M5.1 | Igények és ütemek (9.1) | az inspectorban élőben látszanak |
| M5.2 | Pelenka, folt, bűnösség állapotgép (9.2) | `--selftest-progress` zöld |
| M5.3 | Napszak és évszak LUT-ok, átmenetek, díszek, különleges napok | `--selftest-calendar` zöld, szept. 12. tiltások működnek |
| M5.4 | Távollét-szimuláció és ébredési jelenetek (9.6) | `--selftest-offline` zöld, az eloszlások a tervezett sávban |
| M5.5 | Mentés és visszatöltés, verziózás (9.7) | `--selftest-save` zöld, áramtalanítás után Lili ott van, ahol hagytad |
| M5.6 | Energiaállapotok az eszközön (5.7): DIM, DESK és POCKET mód, PWR, PICKUP üdvözlés | asztalról felvéve felébred és üdvözöl; zsebben nem ébred fel [G] |

### M6 Szabálymotor agy (Lili először így "él")

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M6.1 | `model/schema_lili.json` és a generátorok (`vocab.h`, `tokenizer.bin`, Python kódoló) | a C és a Python kódoló azonos tokeneket ad 1000 állapotra |
| M6.2 | `encoder.c`: állapot -> 26 tokenes prompt | a szimulátor konzolja kiírja |
| M6.3 | `teacher_rules.yaml` (~40 sablon, H1-H15), `gen_rules.py`, `brain_rules.c` | minden H-szokás legalább egy sablonban |
| M6.4 | `brain.c` felület, postaláda, Core 1 taszk (először a szabálymotorral) | a szimulátorban és az eszközön Lili önállóan él |
| M6.5 | A próbák (10.11) **a tanáron magán**: ha a tanár nem teljesíti, a diák sem fogja | P1-P12 zöld a szabálymotorral |
| M6.6 [G] | Egy hét "élet" a szabálymotorral a panelen, a viselkedés hangolása a YAML-ben | a gazdi szerint "ez Lili" |

### M7 Az LLM tanítása

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M7.1 | `--dump-traces`, virtuális gazdi robot, perturbációk | 150 000+ pár |
| M7.2 | Tisztítás, duplikátumszűrés, kiegyensúlyozás; vizualizáló 5. fül | az eloszlás ésszerű, nincs H-szokás 1% alatt |
| M7.3 | `train.py` Lili sémára, csak a kimeneti tokeneken számolt veszteség, S/M/L tanítás; vizualizáló 1. fül | a három modell kész, a görbék a `stats.md`-ben |
| M7.4 | Értékelés, próbák; vizualizáló 2. és 3. fül | a 10.11 céljai legalább egy méretnél teljesülnek |
| M7.5 | (opcionális) B szintű tanár: Ollama, `prompt_check`, kétes helyzetek újracímkézése, újratanítás | a próbák nem romlanak, az agy-laborban árnyaltabb |

### M8 Az agy az eszközön

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M8.1 | Export Q4, trailer sémaverzióval; vizualizáló 4. fül | Q4 vs fp32 ≥ 95% |
| M8.2 | Kényszerített dekódolás és környezeti maszk a C motorban; flashelés; Core 1 integráció | 100% érvényes kimenet az eszközön |
| M8.3 | Késleltetés mérése S/M/L méretekkel a panelen; méretválasztás (D-12) | döntés ≤ 3,5 mp, a választás a `decisions.md`-ben |
| M8.4 | Habozás, kérdezési ütem hangolása; `--selftest-llm 5` | 5 perc alatt 0 lefagyás, változatos cél-eloszlás |

### M9 Hang

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M9.1 [G] | Videók az `assets/raw_videos/`-ba, hangkinyerés, válogatás | legalább 2 ugatás és 1 puffogás kiválasztva |
| M9.2 | Hangbank, animáció-szinkron, éjszakai halkítás, némítás | a hang pontosan a fejfelcsapással szól |
| M9.3 | Fütty és hang detektor a játékban (R2, R8), ön-némítás | a bokorból füttyre előjön; a saját ugatására nem reagál |
| M9.4 | Név-felismerés kísérlet (11.4) | elfogadva vagy dokumentáltan elvetve (D-07) |

### M10 Felület és csiszolás

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M10.1 | Üveg állapotsáv (7.8) | 25 fps felett marad, amíg látszik |
| M10.2 | Beállítások lap | minden kapcsoló működik és mentődik |
| M10.3 | Magyar pixelbetű ékezetekkel | minden ékezetes betű helyes |
| M10.4 [G] | Gondolatbuborékok kipróbálása a panelen | döntés: alapból be vagy ki (D-09) |
| M10.5 | Keresési segítség kapcsoló (D-06) | működik |
| M10.6 | Akku: alacsony töltés jelzés, töltés ikon | a pocket-tank logikájával |
| M10.7 | Első indulás: idő beállítása, rövid köszönés | friss panelen végigmegy |

### M11 Hosszútávú teszt és v1.0

| Feladat | Leírás | Kész, ha |
|---|---|---|
| M11.1 [G] | 7 napos valós használat a panelen | nincs összeomlás, a heap stabil, az akku-idő mért |
| M11.2 | Egy teljes év lefuttatása a szimulátorban gyorsítva (`--fast`), minden évszak és különleges nap pillanatképe | minden jelenet jóváhagyva |
| M11.3 | Hibajavítás, `v1.0` tag, magyar nyelvű README képernyőképekkel | kiadva |

### 2. fázis (később): Bella

Bella modellje (a Lili-modell átszínezése és fülei), `MAX_ACTORS = 2`, `friend_*` mezők a sémában, kétkutyás sablonok a tanárban (együtt alszanak a kuckóban, versenyeznek a kötélért), újratanítás.

---

## 13. Tesztelési stratégia

1. **Automatikus:** a selftestek (6.4) minden commit előtt (`make check`), a Claude Code csak zöld állapotot commitol.
2. **Viselkedési próbák:** P1-P12 a tanáron (M6.5) és a modelleken (M7.4, M8.1).
3. **Vizuális:** `--snapshot` pillanatképek minden jellemző jelenetről, összevetve a jóváhagyott referenciaképekkel (eltérés esetén emberi ellenőrzés).
4. **Eszközön:** soros konzol mérései (fps, heap, döntési idő, fogyasztás), 10 perces és 7 napos tartósteszt.
5. **Kézi ellenőrzőlista [G]:** minden mérföldkő végén egy rövid "próbáld ki" lista a `STATUS.md`-ben (pl. "füttyents, amikor a bokor mögött van").

---

## 14. Kockázatok és tartalékmegoldások

| Kockázat | Valószínűség | Tartalék |
|---|---|---|
| A Blender-modell nem lesz elég "Lili" az MCP-vel | közepes | a korábbi próbamodellből indulunk; CC0 alapmodell; jóváhagyási kapuk; kézi pixeljavítás ingyenes szerkesztővel (pl. LibreSprite vagy Pixelorama) az `overrides/` mappában |
| Az LLM döntés túl lassú | alacsony | kisebb modell (S/M), kevesebb token, a kérdezési ütem lazítása |
| Kevés a belső SRAM | közepes | minden nagy puffer PSRAM-ban, kisebb stripe-ok, mérés minden mérföldkőnél |
| Flash-sávszélesség verseny (LLM és assetek) | közepes | az assetek bootkor a PSRAM-ba töltődnek |
| A név-felismerés megbízhatatlan | magas | a fütty a fő hívás, a név kikapcsolva marad |
| Az IMU nem tud ébreszteni | közepes | periodikus rövid ébredés és lekérdezés, vagy csak PWR |
| Asztali módban gyorsan merül | közepes | a türelmi idő a mért fogyasztáshoz igazítva |
| A tanár torzít (a "prompt a DNS" jelenség) | közepes | eloszlás-elemző fül, próbák, a tanár saját próbái |
| A pocket-tank közben változik | biztos | rögzített fork commit, csak célzott átvételek |
| Mentésvesztés | alacsony | CRC, verziózás, 5 perces mentés, a régi verziók mindig betölthetők |

---

## 15. Nyitott döntések (mindegyiknek van alapértelmezése, egyik sem blokkolja az indulást)

| ID | Kérdés | Alapértelmezés | Mikorra kell |
|---|---|---|---|
| D-01 | Fejlesztői környezet | Windows 11 + WSL2 (tükrözött hálózat) | M0 |
| D-02 | Tanár | A szint (szabálymotor), a B szint (Ollama) később opcionálisan | M7.5 |
| D-03 | **Lili melyik füle konyul le** (az ő bal vagy jobb füle, szemből nézve melyik oldalon) | a fotók alapján döntjük el, a gazdi megerősíti | M3.4 |
| D-04 | Befogadás ünnepe (2019 január, pontos nap nem ismert) | január 15. | M5.3 |
| D-05 | Az ajtó helye a szobában és az udvar tájolása | hátsó fal közepe; az udvaron a ház alul | M2.4 |
| D-06 | Keresési segítség (mancsnyom a szélen, amikor átment a másik helyszínre) | kikapcsolva (keresd meg magad) | M10.5 |
| D-07 | Név-felismerés | kísérleti, alapból ki | M9.4 |
| D-08 | A felület nyelve | magyar | M10.3 |
| D-09 | Gondolatbuborékok | a szimulátorban be, az eszközön a próba után döntesz | M10.4 |
| D-10 | Referenciafotók: kötéljáték, plüss, ágy és takaró, sarokülő, pelenka, a régi `.blend` | a meglévő fotókból dolgozunk | M3.4 |
| D-11 | Időbeállítás | Wi-Fi SNTP a beállításokban + soros parancs | M5.3 |
| D-12 | Végleges modellméret | a legkisebb, amelyik teljesíti a 10.11-et | M8.3 |
| D-13 | Lili saját hangja | a gazdi videóiból; tartalék: halk szintetikus kiskutya-hangok | M9.1 |
| D-14 | Az ágy takarója | piros (a fotók alapján) | M3.7 |

---

## 16. Függelék

### 16.1 Indító üzenetek a Claude Code-nak

Minden mérföldkőnél ugyanaz a minta (csak a szám változik):

> Olvasd el a `CLAUDE.md`-t és a `PIXEL_LILI_SPEC.md` releváns fejezeteit. Most az **M\<szám\>** mérföldkövön dolgozunk. Kezdd a feladatlista elkészítésével a 12. fejezet táblázata alapján, kérdezz, ha egy döntés nincs eldöntve (15. fejezet), majd haladj feladatonként. Minden feladat végén mutasd meg a "Kész, ha" feltétel teljesülését, futtasd a `make check`-et, és frissítsd a `docs/STATUS.md`-t.

Az első üzenet konkrétan:

> Olvasd el a `CLAUDE.md`-t és a `PIXEL_LILI_SPEC.md`-t teljes egészében. Kezdjük az **M0**-val: vezess végig a környezet beállításán (6.1-6.2), hozzuk létre a privát forkot, olvasd el a pocket-tank kötelező dokumentumait (4.1), és futtassuk a négyzetes szimulátort a tesztjeivel.

### 16.2 Rövid fogalomtár

| Fogalom | Jelentés |
|---|---|
| BSP / HAL | a hardvert elrejtő alsó réteg (meghajtók) |
| PSRAM | a 8 MB-os külső munkamemória (lassabb, de nagy) |
| SRAM | a processzor 512 KB-os belső, gyors memóriája |
| mmap | a flash memória közvetlen olvasása, mintha RAM lenne, másolás nélkül |
| LUT | táblázat, ami egy indexhez színt (vagy másik indexet) rendel |
| Y-rendezés | a mélység szerinti rajzolási sorrend |
| Token | a modell egy "szava"; az eszközön mindegyik ~0,1 mp |
| Prefill | a prompt feldolgozása; kötegelve gyorsabb |
| Desztilláció | egy tanár (szabálymotor vagy nagy modell) döntéseiből tanul a kicsi diák |
| Kvantálás (Q4) | a súlyok 4 bitre tömörítése 64-es csoportonkénti skálával |
| Kényszerített dekódolás | a kimenetben csak a nyelvtanilag érvényes tokenek választhatók |
| top2_margin | az első két jelölt valószínűségének különbsége; kicsi = habozás |
| Selftest | a szimulátor fejetlen, automatikus ellenőrzése |

