# Mért számok (dátummal, a spec P6 elve szerint)

| Dátum | Mi | Érték | Hol / hogyan mérve |
|---|---|---|---|
| 2026-10-18 | WSL2 környezet | Ubuntu 24.04.5, kernel 6.18.54, gcc 13.3, SDL2 2.30.0, cmake 3.28.3 | `wsl_root_setup.sh` kimenet |
| 2026-10-18 | ESP-IDF | v5.4.1, esp32s3 toolchain | `idf.py --version` |
| 2026-10-18 | PyTorch | 2.11.0+cu128, CUDA 12.8, RTX 5060 sm 12.0, `cuda.is_available()` True | `.venv/bin/python` |
| 2026-10-18 | pocket-tank sim (square) | fordul, `make SQUARE=1 check` zöld; selftest-llm: 11 célváltás (5 habozó), 20 kérdés, 0 override 60 sim-mp alatt | WSL2, Ryzen 5 7600 |

A pocket-tank saját mért adatai (upstream): `docs/pocket-tank-stats.md` és `docs/UPSTREAM.md` 2. fejezet.
