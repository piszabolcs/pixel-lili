# Állapot

Frissítve: 2026-10-19 (este)

## Kész
- **M0.1** Fejlesztői környezet: WSL2 Ubuntu 24.04.5 a `D:\WSL\Ubuntu-24.04`-en (tükrözött hálózat, systemd), build-essential (gcc 13.3), SDL2 2.30, cmake 3.28, ninja 1.11, ffmpeg 6.1, uv 0.13; ESP-IDF **v5.4.1** (`~/esp/esp-idf`, `install.sh esp32s3`, `idf.py --version` OK); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14); `.venv` Python 3.11 + **torch 2.11.0+cu128**, `torch.cuda.is_available()` = True, sm 12.0. Windowson: usbipd-win 5.3, uv, VS Code WSL bővítmény.
- **M0.2** A repó a pocket-tank `a3184fc` klónja, `upstream` remote beállítva; `CLAUDE.md`, `PIXEL_LILI_SPEC.md`, `docs/`, `reference/` bent. **Helye: `C:\Users\kaposzta\Desktop\Lili_project\pixel-lili`** (WSL-ből `/mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili`). A gazdi döntése (2026-10-18): **GitHub nincs**, a repó csak helyi; a félig létrehozott üres privát `piszabolcs/pixel-lili` repó érintetlen, bármikor törölhető. A WSL-ben csak a fordító környezet él: `~/esp/esp-idf`, `~/dev/lili-venv` (PyTorch), `~/dev/pocket-tank` (referencia-klón).
- **M0.3** `docs/UPSTREAM.md`: a 10 kötelező pocket-tank doksi és a kulcsforrások összefoglalója (átvett modulok fájl- és függvényszinten, mért számok, build/flash, memóriakeret, mentés, LLM pipeline, Director parancsok, hang, kihagyott részek, 17 buktató).
- **M0.4** `make SQUARE=1` -> `fishsim-square` fordul; `make SQUARE=1 check` zöld (selftest, selftest-llm, selftest-saves, selftest-update). Headless pillanatkép: `--snapshot` PPM-eket ír (ffmpeg-gel PNG-vé alakítható).

## Kész (M1)
- **M1.1** gyári flash mentve (`Lili_project\backup\factory_216_2026-10-18.bin`), **M1.2** pocket-tank square fut a panelen, **M1.3** gombok (csak PWR él a pocket-tank alatt; BOOT = GPIO 0, KEY3 = GPIO 18).
- Diag firmware: `firmware/diag` (build: `bash tools/build_diag.sh` WSL-ben, flash: `py -3.12 tools/flash_216.py diag`, konzol: `py -3.12 tools/serial_log.py`). Vissza a pocket-tankra: `py -3.12 tools/flash_216.py tank`.
- **M1.4** QMI8658 INT1 = GPIO 17, a WoM 338 s alvás után felemelésre ébresztett (ébresztés az INT1 aktuális szintjének ellentétére).
- **M1.6** mikrofon (frekvencia 1 Hz-en belül), füttydetektor v3: két címkézett felvételen 24/24 fütty, 0 téves (`tools/whistle_sim.py`).
- **M1.7** CORNER_R = 72 fizikai px (36 logikai).
- **M1.8** RTC beállítás (`tools/rtc_sync.py`) és megtartás PMIC-kikapcsolás alatt.

## Folyamatban
- **M1.5 fogyasztás:** 2026-10-19 19:29-kor élesítve (`power run 60 60`): kábel ki -> 60 perc ACTIVE -> 60 perc DIM -> könnyű alvás a PWR-ig, sorok az NVS-ben. Reggel: PWR röviden, kábel vissza, `py -3.12 tools/serial_log.py --seconds 5 --send powerlog --send-delay 1`, majd a DESK türelmi idő a decisions.md-be.
- **RTC drift:** `py -3.12 tools/rtc_sync.py --check` (beállítva 2026-10-19 17:23:09).

## Következő
- M2 a szimulátorban (panel nélkül is megy): Lili váz a hal helyett, 8 bites renderelő, kávamaszk (CORNER_R 72), két nézet helyőrzővel, inspector.
- Az M1 lezárása az M1.5 adataival; utána M2.6 (a panelen).
- Később (D-07): "Lili" névfelismerés.

## Próbáld ki (a gazdinak)
- **A panelen:** menj végig az üdvözlő oldalon (érintés), nézd meg, hogy úsznak-e a halak, működik-e az érintés és a 3 gomb. Írd meg, melyik gomb mit csinált (M1.3).
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
