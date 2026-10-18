# Pixel Lili: induló felmérés, brainstorming, telepítési lista

Dátum: 2026-10-18
Készítette: Claude Code (a `Lili_project` mappa teljes átnézése és a gép felmérése után)
Státusz: **felmérés, semmi nincs még telepítve vagy létrehozva.** A HARD-GATE szerint csak a jóváhagyásod után kezdünk építeni.

---

## 1. Mi van a mappában (leltár)

| Hely | Mi | Értékelés |
|---|---|---|
| `Dokumentacio/PIXEL_LILI_SPEC.md` | 1237 soros, v2.0 rendszerterv (16 fejezet, M0-M11 mérföldkövek, D-01..D-14 nyitott döntések) | **Ez az egyetlen igazságforrás.** Kiforrott, pontról pontra indítható. |
| `Dokumentacio/CLAUDE.md` | 11 kötelező munkaszabály (adatbázis csak olvasható, `common/` platformfüggetlen, mérés nem becslés, ingyenes eszközök, sprite nem tükrözhető, stb.) | Bemegy a leendő repó gyökerébe változtatás nélkül. |
| `Egyeb/Lili_projekt_osszefoglalo.md` | A korábbi (Claude-os) összefoglaló: hardver, karakter, személyiség, lépések | Részben elavult, a spec 1. fejezete auditálta. Referenciának jó. |
| `Egyeb/osszefoglalo_geminitől.txt` | 3188 soros Gemini-beszélgetés | A spec kimondja: ahol ellentmond, a spec érvényes. Elavult benne: 320-as szókincs, 76 px sarok, PyTorch nightly, Claude/Gemini API tanár. Nem kell többet olvasni. |
| `Egyeb/scripts/build.py` | Blender script: Lili primitívekből, 8 irány, `walk` és `turn` animáció, ortho kamera, Cycles 64x64 | **Kincs.** Ez maga a "korábbi Blender-próba". Nincs `.blend` fájl, de nem is kell: a script minden futtatáskor újraépíti a modellt. A D-10 kérdés ("megvan-e a régi .blend") ezzel megválaszolva: a forrás a script. |
| `Egyeb/scripts/post.py` | Pixelesítés: alfa-küszöb, 14 színű medián-vágás, 1 px kontúr | A spec 7.5 B7 lépésének őse. Újra kell írni a spec szerint (4x4 blokk mód, Lab-legközelebbi, mester-paletta), de az ötlet igazolt. |
| `Egyeb/scripts/scene.py` | 2.5D szoba-próba: 35°-os vetítés, szőnyeg, kötéljáték, útvonal, gif | A `lili_3d_proba.gif` forrása. Igazolja a Y-rendezéses 2.5D elvet. |
| `Egyeb/scripts/lili.py` | Pillow-ból rajzolt 40x40 sprite (v1) | Történeti, a 3D irány felváltotta. |
| `Egyeb/kepek/` | `lili_3d_proba.gif`, `lili_v1.gif`, `sheet_dirs.png` (8 irányú lap) | Referencia a `reference/` mappába. |
| `Egyeb/fotok/lili_es_bella/` (12 kép) | Lili és Bella fotói | Paletta (B0) és modell (B2) referenciája. |
| `Egyeb/fotok/vasarlas/` (4 kép) | Vásárlási képernyőképek | **Figyelem:** van köztük az 1.8"-os panel ("Without battery") és a 2.16"-os ("With Battery") oldala is. Lásd 3.1. |
| `Egyeb/lili_projekt (2).zip` | Ugyanez a tartalom becsomagolva (benne még egy zip ugyanazzal) | Nincs benne semmi új. Archívum, törölhető vagy marad. |

**Megjegyzés a scriptekhez:** a `/tmp/lili3d/` és `/home/claude/` útvonalak mutatják, hogy ezek a Claude felhős sandboxában futottak, nem a te gépeden. Helyben még soha nem futott Blender render.

---

## 2. Mi van a gépeden (felmérés)

| Komponens | Állapot | Megjegyzés |
|---|---|---|
| Windows 11 Pro 26200, Ryzen 5 7600, 31 GB RAM | OK | |
| NVIDIA RTX 5060 8 GB, driver 617.14 | OK | WSL2-ben a CUDA ebből jön, külön CUDA toolkit nem kell |
| Lemez | **C: 40 GB szabad**, D: 272 GB, F: 178 GB | A C: szűk. Javaslat: a WSL virtuális lemezét (ext4.vhdx) és az ESP-IDF-et a D:-re tenni (lásd 5.4). |
| WSL2 | **nincs telepítve** | A spec D-01 alapértelmezése WSL2 + Ubuntu 24.04. Ez a legnagyobb hiányzó darab. |
| Git 2.53 | OK | |
| Python 3.12 (Windows) + pip | OK, de a spec 3.11-et kér a venv-hez | Az `uv` letölti a 3.11-et magától, nem probléma. |
| Node 24, npm, `claude` CLI (npm) | OK | |
| VS Code + Visual Studio 2026 Community | OK | VS Code-hoz a "WSL" bővítmény kell majd. |
| Ollama | telepítve, **0 modell** | Csak az opcionális B szintű tanárhoz (M7.5) kell modell. |
| Blender | **hiányzik** | M3-hoz kell. |
| uv, FFmpeg, CMake, Ninja, gcc, make | **hiányoznak** (Windowson) | WSL-ben lesznek. `uv` Windowson is kell a blender-mcp-hez. |
| ESP-IDF, esptool, PlatformIO, Arduino | **hiányoznak** | ESP-IDF v5.4 WSL-be. |
| usbipd-win | hiányzik | USB átadás a WSL-nek flasheléshez. |
| Soros port | csak COM1 | A panel még nincs csatlakoztatva (vagy nem érkezett meg). |

---

## 3. Ellentmondások, kockázatok, amiket a doksik átnézése közben találtam

### 3.1 Melyik panel van (vagy lesz) a kezedben?
A spec végig a **2.16"-os** (CO5300, 480x480, akkus) panelre épül, és a pocket-tank ezt `sdkconfig.square` néven támogatja. A vásárlási képek között viszont ott az **1.8"-os** (368x448, "Without battery") oldal is. Ha mégis az 1.8"-os jött, az nem baj (a pocket-tank alapból azt támogatja), de a spec 5. fejezetét, a 240x240-es logikai vásznat és a 2x nagyítást át kell írni (368x448 nem osztható szépen 2-vel). **Ezt tisztázni kell az M1 előtt.**

### 3.2 A pocket-tank valóban támogatja a 2.16-ot (ellenőrizve ma a GitHubon)
A `docs/board-amoled-2.16.md` létezik. Mért adatok, amik a spec audit-táblázatával egyeznek:

| Mérés | Érték |
|---|---|
| fps két hallal | 20,3-25,5 |
| szabad belső heap futás közben | 38 KB |
| LLM | 9,6-10,1 token/mp, 4,25-4,44 mp/döntés |
| akku bekapcsolva | ~79 mA, ~12 óra |
| kikapcsolva | < 1 mA |

Ismert nyitott hibáik, amik a mi M1-ünket is érintik: az érintés a bal szélen ~7 px-et, függőlegesen ~20 px-et csúszik; a PWR gomb alvás/ébresztés logikája teszteletlen; az IMU tengelyek ellenőrizetlenek; az alvó fogyasztás nincs mérve. Pontosan ezek vannak a spec M1.3-M1.5-ben, tehát a terv jó helyen tapogat.

### 3.3 "Privát fork" a GitHubon nem létezik
A GitHub egy publikus repó forkját **nem engedi priváttá tenni.** A megoldás: új privát repó (`pixel-lili`), bele `git clone --mirror` vagy sima push a pocket-tank tartalmával, és a pocket-tank `upstream` remote-ként. Funkcionálisan ugyanaz, csak nem a "Fork" gombbal. Ez összhangban van a te github-guardian szabályoddal (privát repó, te vagy az egyetlen szerző, magyar README).

### 3.4 A blender-mcp átnevezve
A spec `uvx blender-mcp` parancsa még működik, de a csomag új neve `mcp-for-blender`. Claude Code-hoz: `claude mcp add blender uvx mcp-for-blender`. Windows támogatott, Blender 3.0+, Python 3.10+, `uv` kötelező.

### 3.5 Hol fusson a Claude Code?
A spec azt mondja: WSL2-ben. Te most a **Windows-os Claude asztali appban** dolgozol velem. Két lehetőség:
- **(a) Marad ez a session Windowson**, és én a WSL-t a `wsl.exe -e bash -lc "..."` parancsokon át vezérlem. A repó a WSL fájlrendszerében él (`\\wsl.localhost\Ubuntu-24.04\home\...`), mert az NTFS-en (`/mnt/c`) a `make` és a `git` 5-10x lassabb. Kérdés, hogy az asztali app meg tudja-e nyitni munkamappaként a WSL útvonalat. Ezt az M0.1-ben kipróbáljuk.
- **(b) `claude` indítása a WSL terminálból** a repó mappájában. Ez a spec eredeti elképzelése, biztosan működik, de akkor nem ebben az appban dolgozunk.
Javaslatom: (a)-val indulunk, ha az app nem bírja a WSL útvonalat, (b).

### 3.6 Lili melyik füle konyul le (D-03)
A fotók alapján az én olvasatom: **a saját jobb füle konyul le, a bal áll.** Szemből nézve tehát a néző bal oldalán van a lekonyuló fül. (Az ülős fotón, ahol a kamerába néz: a kép jobb oldalán álló fül, a bal oldalán hajtott fül.) **Erősítsd meg,** mert ez a 3D modellben és minden sprite-on végigmegy, és tükrözni tilos.

### 3.7 Hiányzó referenciák (D-10)
Nincs fotó: kötéljátékról, plüssről, sarokülőről, pelenkáról. Az ágy takarója a fotón inkább **magenta-pink**, mint piros (D-14 alapértelmezés "piros"). A napfényfoltos ajtó-fotó viszont tökéletes a H6-hoz és az udvar tájolásához.

### 3.8 Lemezhely
Becslés a teljes környezetre: WSL Ubuntu ~2 GB, ESP-IDF v5.4 + esp32s3 toolchain ~3,5 GB, PyTorch CUDA venv ~7 GB, Blender ~1 GB, projekt + renderek + tanítóadat ~3 GB, opcionális Ollama modell ~5 GB. **Összesen 16-21 GB.** A C: 40 GB-jából ez megy, de szorosan. A D:-n 272 GB van: a WSL vhdx-et oda javaslom.

---

## 4. Brainstorming: hogyan érdemes nekiállni

### 4.1 Három dolog, ami a specből kiolvasható és jó hír
1. **A hardveres réteg 80%-a kész** a pocket-tankban (kijelző, érintés, PMIC, RTC, IMU, hang, LLM-motor, szimulátor, mentés). Mi Lilit, a szobát és az agy sémáját írjuk, nem meghajtókat.
2. **A helyőrző grafika (7.7) miatt a kód nem vár a művészetre.** M2, M4, M5, M6 kapszula-kutyával is teljesen fejleszthető és tesztelhető.
3. **A szabálymotor tanár (10.7 A szint) miatt az LLM az utolsó lépés**, nem az első. Lili először szabályokkal "él" (M6), a modell csak simábbá teszi.

### 4.2 Javasolt sorrend (a spec M-jei, az én hangsúlyaimmal)
```
M0 környezet + repó ──> M0.4 fishsim-square fut WSL-ben (ez az első "látható" pillanat)
   │
   ├── ha a panel megjött: M1 hardver-validáció (1-2 este, [G] feladatokkal)
   │
M2 váz: 8 bites renderelő + kávamaszk + két nézet helyőrzővel ──> M2.6 fut a panelen is
   │
   ├── M3 grafika PÁRHUZAMOSAN: Blender + mcp-for-blender, a build.py-ból indulva
   │      (M3.3 teljes lánc helyőrzővel az első, Lili modell utána)
   │
M4 világ, A*, reflexek, játékfizika ──> M5 igények, idő, mentés ──> M6 szabálymotor
   │
   └── "Lili él" (M6.6: egy hét a panelen) ──> M7-M8 LLM ──> M9 hang ──> M10 UI ──> M11 v1.0
```

### 4.3 Döntések, amiket most kell meghozni (a többi D-xx várhat)
| ID | Kérdés | Javaslatom |
|---|---|---|
| D-01 | WSL2 vagy natív Windows | **WSL2.** A pocket-tank Makefile-jai Linux/macOS-re készültek, Windows nincs említve. Natív Windows (MSYS2) csak idő-elszívó lenne. |
| 3.1 | Melyik panel | te tudod |
| 3.5 | Claude Code Windows-appból vezérli a WSL-t, vagy WSL-ben fut | (a)-val próbáljuk, (b) tartalék |
| 3.8 | WSL vhdx a D:-re | igen |
| D-03 | lekonyuló fül | jobb (saját), megerősítendő |

### 4.4 Ötletek a spec fölé (nem kötelező, csak hogy lásd a lehetőségeket)
- **"Első este" cél:** M0.4 után a pocket-tank halai úsznak a WSL-es ablakban. Ez 1 este. Utána az M2.4-es két nézet helyőrzővel még 2-3 este. Innen már minden látható, nem csak kód.
- **A `build.py` mint M3 kiindulás:** a primitív-Lili már megvan 8 irányban. Az első Blender-feladat nem "modellezz kutyát", hanem "nyisd meg a build.py jelenetét, tedd a kamerát 35°-ra, cseréld a kúp-fület rigelhető fülre." Ez kisebb kockázat, mint a CC0 alapmodell.
- **Kontaktlap-kapu minden animációhoz:** a spec is kéri (B8), én hozzáteszem: minden kontaktlapot a `reference/approved/` mappába mentünk dátummal, hogy visszanézhető legyen, mikor mi volt elfogadva.
- **Vizualizáló korán:** a Gradio műszerfal 5. fülét (adat-elemző) már M6-ban érdemes megcsinálni a szabálymotor eloszlásaira, nem csak M7-ben. Így a YAML-hangolásnál azonnal látod, mit csinál Lili.
- **Tanulási napló:** a spec 10.14-ben van egy "mit érts meg" útvonal. Javaslom egy `docs/learning.md`-t, ahová mérföldkövenként 5-10 sorban leírom, mit csináltunk és miért, a te szavaiddal. A projekt végén ez a README "hogyan készült" fejezete.

### 4.5 Kockázatok, amiket a legkorábban érdemes kimérni
| Kockázat | Mikor derül ki | Mit tegyünk |
|---|---|---|
| WSLg SDL ablak akadozik | M0.4 | ha igen: natív Windows build csak a szimulátornak (SDL2 MSYS2-vel), a firmware marad WSL-ben |
| 38 KB belső heap kevés a 8 bites renderelőnek | M2.2 | a stripe pufferek PSRAM-ba, mérés minden lépésnél |
| USB átadás WSL-be (usbipd) macerás | M1.1 | tartalék: `esptool` Windowson pipből, csak a build megy WSL-ben |
| A Blender-Lili nem lesz "Lili" | M3.4 | build.py-ból indulás, jóváhagyási kapuk, kézi pixeljavítás `overrides/`-ban |

---

## 5. Telepítési lista

Két szintre bontva: ami az **M0-M2-höz** kell (szimulátor + firmware build), és ami **később** (M3 Blender, M7 PyTorch, M9 FFmpeg). Minden ingyenes. Az összes parancsot végigvezetem, nem kell kézzel beírnod; itt azért vannak, hogy lásd, mi történik majd.

### 5.1 Windows oldal, M0-hoz

| # | Mi | Parancs vagy hely | Méret |
|---|---|---|---|
| 1 | WSL2 + Ubuntu 24.04 | `wsl --install -d Ubuntu-24.04` (rendszergazda PowerShell, újraindítás kell) | ~2 GB |
| 2 | `.wslconfig` tükrözött hálózattal | `%USERPROFILE%\.wslconfig`: `[wsl2]` `networkingMode=mirrored` `memory=16GB` | |
| 3 | WSL vhdx áthelyezése D:-re (opcionális, de javasolt) | `wsl --export` / `wsl --import` a `D:\WSL\Ubuntu-24.04` mappába | |
| 4 | usbipd-win | `winget install usbipd` | kicsi |
| 5 | uv (Windows) | `powershell -c "irm https://astral.sh/uv/install.ps1 \| iex"` | kicsi |
| 6 | VS Code "WSL" bővítmény | `code --install-extension ms-vscode-remote.remote-wsl` | kicsi |
| 7 | GitHub privát repó `pixel-lili` | GitHub weben vagy `gh` CLI (`winget install GitHub.cli`) | |

### 5.2 WSL (Ubuntu) oldal, M0-hoz

| # | Mi | Parancs | Méret |
|---|---|---|---|
| 1 | Alap eszközlánc | `sudo apt install build-essential git cmake ninja-build libsdl2-dev python3-venv python3-pip pkg-config libusb-1.0-0 ffmpeg` | ~0,5 GB |
| 2 | uv | `curl -LsSf https://astral.sh/uv/install.sh \| sh` | kicsi |
| 3 | ESP-IDF v5.4 | `git clone -b v5.4 --recursive https://github.com/espressif/esp-idf.git ~/esp/esp-idf && cd ~/esp/esp-idf && ./install.sh esp32s3` | ~3,5 GB |
| 4 | Claude Code (ha a (b) utat választjuk) | `curl -fsSL https://claude.ai/install.sh \| bash` | kicsi |
| 5 | pocket-tank + LVGL | `git clone https://github.com/mediacutlet/pocket-tank` majd `git clone --depth 1 --branch v9.2.2 https://github.com/lvgl/lvgl.git sim/lvgl` | ~0,3 GB |
| 6 | Ellenőrzés | `nvidia-smi`, `idf.py --version`, `cd sim && make SQUARE=1 && ./fishsim-square`, `make SQUARE=1 check` | |

### 5.3 Később

| Mikor | Mi | Parancs | Méret |
|---|---|---|---|
| M3 | Blender 4.x (Windows) | `winget install BlenderFoundation.Blender` | ~1 GB |
| M3 | mcp-for-blender addon | `uvx mcp-for-blender install-addon`, Blenderben Edit > Preferences > Add-ons > "Interface: MCP for Blender", N panel > Start | kicsi |
| M3 | MCP bekötés Claude Code-ba | `claude mcp add blender uvx mcp-for-blender` | |
| M3 | Python képfeldolgozás (WSL venv) | `uv venv .venv --python 3.11 && uv pip install pillow numpy scipy` | kicsi |
| M7 | PyTorch CUDA 12.8 (WSL venv) | `uv pip install torch torchvision --index-url https://download.pytorch.org/whl/cu128` | ~7 GB |
| M7 | Vizualizáló | `uv pip install gradio plotly matplotlib` | ~0,3 GB |
| M7.5 (opc.) | Ollama modell a B tanárhoz | pl. `ollama pull qwen3:8b` vagy `gemma3:12b-it-qat` (8 GB VRAM-ba férjen) | ~5-8 GB |
| M9 | Hangfeldolgozás | `uv pip install librosa soundfile noisereduce` | kicsi |
| M3 (opc.) | Pixel-szerkesztő kézi javításhoz | Pixelorama (ingyenes, Windows) | kicsi |

### 5.4 Amit NEM kell telepíteni
- CUDA Toolkit: a Windows driver adja a CUDA-t a WSL-nek.
- Arduino IDE, PlatformIO: a pocket-tank ESP-IDF-es.
- Visual Studio C++ eszközök: már fent van, de nem használjuk, minden WSL-ben fordul.
- Fizetős API-k, PixelLab: a spec P2 tiltja.

---

## 6. Mi a következő lépés

1. Válaszolsz a 4.3 táblázat 5 kérdésére (elég egy-egy szó).
2. Jóváhagyod ezt a felmérést mint kiindulást.
3. Indul az **M0** a spec 16.1 indító üzenetével: környezet (5.1-5.2), privát repó, pocket-tank kötelező olvasmányok, `fishsim-square` fut.

Az M0 a te gépeden kb. 1-2 estét vesz igénybe (az ESP-IDF és a WSL letöltése a leglassabb rész), és semmit nem ír a panelre.
