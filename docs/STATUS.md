# Állapot

Frissítve: 2026-10-18

## Kész
- **M0.1** Fejlesztői környezet: WSL2 Ubuntu 24.04.5 a `D:\WSL\Ubuntu-24.04`-en (tükrözött hálózat, systemd), build-essential (gcc 13.3), SDL2 2.30, cmake 3.28, ninja 1.11, ffmpeg 6.1, uv 0.13; ESP-IDF **v5.4.1** (`~/esp/esp-idf`, `install.sh esp32s3`, `idf.py --version` OK); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14); `.venv` Python 3.11 + **torch 2.11.0+cu128**, `torch.cuda.is_available()` = True, sm 12.0. Windowson: usbipd-win 5.3, uv, VS Code WSL bővítmény.
- **M0.2** A repó a pocket-tank `a3184fc` klónja, `upstream` remote beállítva; `CLAUDE.md`, `PIXEL_LILI_SPEC.md`, `docs/`, `reference/` bent. **Helye: `C:\Users\kaposzta\Desktop\Lili_project\pixel-lili`** (WSL-ből `/mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili`). A gazdi döntése (2026-10-18): **GitHub nincs**, a repó csak helyi; a félig létrehozott üres privát `piszabolcs/pixel-lili` repó érintetlen, bármikor törölhető. A WSL-ben csak a fordító környezet él: `~/esp/esp-idf`, `~/dev/lili-venv` (PyTorch), `~/dev/pocket-tank` (referencia-klón).
- **M0.3** `docs/UPSTREAM.md`: a 10 kötelező pocket-tank doksi és a kulcsforrások összefoglalója (átvett modulok fájl- és függvényszinten, mért számok, build/flash, memóriakeret, mentés, LLM pipeline, Director parancsok, hang, kihagyott részek, 17 buktató).
- **M0.4** `make SQUARE=1` -> `fishsim-square` fordul; `make SQUARE=1 check` zöld (selftest, selftest-llm, selftest-saves, selftest-update). Headless pillanatkép: `--snapshot` PPM-eket ír (ffmpeg-gel PNG-vé alakítható).

## Folyamatban
- (nincs)

## Következő
- M1 hardver-validáció, amint a panel a kezedben van. Első lépés M1.1: gyári flash mentése (`esptool read_flash 0 0x1000000`), ehhez USB átadás a WSL-nek (`usbipd`) vagy Windows-os esptool.
- M0 tanulsága az M2-höz: a pocket-tank build aláírt binárist kér (`tools/ota_key.sh`), és a `firmware/main/CMakeLists.txt` OTA/Wi-Fi komponenseket húz; ezeket az M2.1-ben vágjuk ki (lásd UPSTREAM.md 9-10).

## Próbáld ki (a gazdinak)
- WSL terminálban: `cd /mnt/c/Users/kaposzta/Desktop/Lili_project/pixel-lili/sim && ./fishsim-square` -> 480x480-as akvárium ablak WSLg-n. Fordítás innen: `make SQUARE=1 -j6` (51 mp /mnt/c-ről).
- `~/dev/lili-venv/bin/python -c "import torch; print(torch.cuda.get_device_name(0))"` -> "NVIDIA GeForce RTX 5060".
