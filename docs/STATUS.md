# Állapot

Frissítve: 2026-10-18

## Kész
- M0.1 Fejlesztői környezet: WSL2 Ubuntu 24.04 a D:\WSL-en, build-essential, SDL2 2.30, cmake 3.28, ninja, ffmpeg 6.1, uv 0.13; ESP-IDF v5.4.1 (`~/esp/esp-idf`, `install.sh esp32s3`); `nvidia-smi` a WSL-ben látja az RTX 5060-at (driver 617.14). PyTorch még nincs (M7-nél).
- M0.4 pocket-tank a3184fc: `make SQUARE=1` -> `fishsim-square` fordul, `make SQUARE=1 check` zöld (selftest, selftest-llm, selftest-saves, selftest-update).
- M0.2 Helyi repó: a pocket-tank klónja `upstream` remote-tal, spec + CLAUDE.md + reference/ bent. (GitHub privát repó: folyamatban)

## Folyamatban
- M0.2 privát GitHub repó létrehozása és push.
- M0.3 pocket-tank kötelező doksik összefoglalója -> docs/UPSTREAM.md.

## Következő
- M1 hardver-validáció, amint a panel a kezedben van (M1.1 gyári flash mentése az első lépés).

## Próbáld ki (a gazdinak)
- WSL terminálban: `cd ~/dev/pocket-tank/sim && ./fishsim-square` -> megnyílik a 480x480-as akvárium ablak WSLg-n.
