# Állapot

Frissítve: 2026-10-18 (este)

## Kész
- **M0.1** Fejlesztői környezet: WSL2 Ubuntu 24.04.5 a `D:\WSL\Ubuntu-24.04`-en (tükrözött hálózat, systemd), build-essential (gcc 13.3), SDL2 2.30, cmake 3.28, ninja 1.11, ffmpeg 6.1, uv 0.13; ESP-IDF **v5.4.1** (`~/esp/esp-idf`, `install.sh esp32s3`, `idf.py --version` OK); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14); `.venv` Python 3.11 + **torch 2.11.0+cu128**, `torch.cuda.is_available()` = True, sm 12.0. Windowson: usbipd-win 5.3, uv, VS Code WSL bővítmény.
- **M0.2** A repó a pocket-tank `a3184fc` klónja, `upstream` remote beállítva; `CLAUDE.md`, `PIXEL_LILI_SPEC.md`, `docs/`, `reference/` bent. **Helye: `C:\Users\kaposzta\Desktop\Lili_project\pixel-lili`** (WSL-ből `/mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili`). A gazdi döntése (2026-10-18): **GitHub nincs**, a repó csak helyi; a félig létrehozott üres privát `piszabolcs/pixel-lili` repó érintetlen, bármikor törölhető. A WSL-ben csak a fordító környezet él: `~/esp/esp-idf`, `~/dev/lili-venv` (PyTorch), `~/dev/pocket-tank` (referencia-klón).
- **M0.3** `docs/UPSTREAM.md`: a 10 kötelező pocket-tank doksi és a kulcsforrások összefoglalója (átvett modulok fájl- és függvényszinten, mért számok, build/flash, memóriakeret, mentés, LLM pipeline, Director parancsok, hang, kihagyott részek, 17 buktató).
- **M0.4** `make SQUARE=1` -> `fishsim-square` fordul; `make SQUARE=1 check` zöld (selftest, selftest-llm, selftest-saves, selftest-update). Headless pillanatkép: `--snapshot` PPM-eket ír (ffmpeg-gel PNG-vé alakítható).

## Kész (M1, részben)
- **M1.1** Gyári flash lementve: `C:\Users\kaposzta\Desktop\Lili_project\backup\factory_216_2026-10-18.bin` (16 777 216 B, SHA256 mellette `.sha256` fájlban). Windows-os esptool 5.6 a COM3-on (az ESP32-S3 beépített USB-soros portja), 921600 baud, 2 perc. USB-átadás a WSL-nek nem kellett.
- **M1.2** A pocket-tank square build (`idf.py ... sdkconfig.defaults;sdkconfig.square`, WSL, 53 mp) `--full` módban a panelre flashelve Windows-os esptool-lal (NVS törlés 0x9000, modell 0x290000, trailer 0xA8F000, bootloader + tábla + app + otadata). Boot napló: `docs/logs/2026-10-18_first_boot_216.log`. Feláll: CO5300 480x480 (MADCTL a0), CST9220, AXP2101 (500 mA töltés), RTC (idő érvényes), QMI8658 (+Y), ES8311/ES7210, modell trailer "good", advisor LLM. **A gazdi megerősítette: a halak úsznak, az érintés működik.**
- Az OTA aláíró kulcs (`firmware/keys/ota_signing_key.pem`, gitignore-olva) másolata: `Lili_project\backup\ota_signing_key_2026-10-18.pem`.

## Folyamatban
- **M1.3 kész** (csak a PWR él a pocket-tank alatt; BOOT=GPIO 0 tiltva PMIC-es lapon, KEY3=GPIO 18 nem kezelt; részletek `docs/board-notes-lili.md`). Hátra: M1.4 IMU INT (a square-en nincs expander, a GPIO a rajzból kell), M1.5 fogyasztás, M1.6 mikrofon diag app, M1.7 sarok-teszt, M1.8 RTC soros parancs.

## Következő
- M1.3-M1.8 a panelen (gazdi + Claude), utána M2.
- Flash-recept Windowsról: build WSL-ben (`~/build/pixel-lili-fw-216`), artefaktumok `firmware/build/216/`, flash `py -3.12 -m esptool --chip esp32s3 -p COM3 -b 921600 ...` (a lépések a scratch `flash_216_full.ps1`-ben; M2-ben bekerül a `tools/` alá).
- M0 tanulsága az M2-höz: a pocket-tank build aláírt binárist kér (`tools/ota_key.sh`), és a `firmware/main/CMakeLists.txt` OTA/Wi-Fi komponenseket húz; ezeket az M2.1-ben vágjuk ki (lásd UPSTREAM.md 9-10).

## Próbáld ki (a gazdinak)
- **A panelen:** menj végig az üdvözlő oldalon (érintés), nézd meg, hogy úsznak-e a halak, működik-e az érintés és a 3 gomb. Írd meg, melyik gomb mit csinált (M1.3).
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
