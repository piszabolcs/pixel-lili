# Állapot

Frissítve: 2026-10-20 (reggel)

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
- **M3.2 [G]** `palette_from_photos.py`: a mester-paletta Lili fotóiból **és a jóváhagyott célképekből**, a gazdi jóváhagyja.
- **M3 Blender gyártósor** (a gazdi Blender 5.2.2 LTS-t telepített).
- Később (D-07): "Lili" névfelismerés.

## Próbáld ki (a gazdinak)
- **A Lili-világ a panelen:** `py -3.12 tools/flash_216.py lili`, utána a panel a helyőrző szobát mutatja (KEY3 = udvar, BOOT = évszak). Vissza a diagra: `flash_216.py diag`.
- **A panelen (M1.3):** menj végig az üdvözlő oldalon (érintés), nézd meg, hogy úsznak-e a halak, működik-e az érintés és a 3 gomb. Írd meg, melyik gomb mit csinált (M1.3).
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Project/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
