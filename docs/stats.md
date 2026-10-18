# Mért számok (dátummal, a spec P6 elve szerint)

| Dátum | Mi | Érték | Hol / hogyan mérve |
|---|---|---|---|
| 2026-10-18 | WSL2 környezet | Ubuntu 24.04.5, kernel 6.18.54, gcc 13.3, SDL2 2.30.0, cmake 3.28.3 | `wsl_root_setup.sh` kimenet |
| 2026-10-18 | ESP-IDF | v5.4.1, esp32s3 toolchain | `idf.py --version` |
| 2026-10-18 | PyTorch | 2.11.0+cu128, CUDA 12.8, RTX 5060 sm 12.0, `cuda.is_available()` True | `.venv/bin/python` |
| 2026-10-18 | pocket-tank sim (square) | fordul, `make SQUARE=1 check` zöld; selftest-llm: 11 célváltás (5 habozó), 20 kérdés, 0 override 60 sim-mp alatt | WSL2, Ryzen 5 7600 |

A pocket-tank saját mért adatai (upstream): `docs/pocket-tank-stats.md` és `docs/UPSTREAM.md` 2. fejezet.
| 2026-10-18 | Panel azonosítás | ESP32-S3 rev v0.2, 8 MB PSRAM, 16 MB flash (gyártó 0x20, eszköz 0x4018, quad), MAC 90:70:69:fe:6c:74 | esptool 5.6, COM3 |
| 2026-10-18 | Gyári flash mentés | 16 MB, 120,5 s, 1114 kbit/s | esptool read_flash, 921600 |
| 2026-10-18 | Firmware build (pocket-tank square, IDF 5.4.1) | 53 s; app 2 035 712 B (0x1f1000), 22% szabad a 2,5 MB partícióban | WSL, Ryzen 5 7600 |
| 2026-10-18 | Modell flash 7,56 MB | 40,8 s, 1481 kbit/s | esptool write_flash 0x290000 |
| 2026-10-18 | Első boot a panelen | belső heap: boot 233 KB, advisor után 83 KB, app_main vége 48 KB, tank taszk indulásakor 53 KB; PSRAM terv 4484 KB | docs/logs/2026-10-18_first_boot_216.log |
| 2026-10-18 | LLM bench a panelen | dot100k 13 001 us; batch44 flash 52,2 ms / ram 51,7 ms; decode flash 7,1 ms / ram 5,2 ms; első döntés profil: matmul 2283 + attn 726 + norm 139 + rope 38 + swiglu 360 + decode 932 ms = ~4,5 s | uo. |
| 2026-10-18 | Akku első bootkor | 85 %, 4164 mV, kábelen | uo. |
| 2026-10-18 | IMU nyugalomban, asztalon (kijelző felfelé) | g = [-680 417 16908], inverted=0, orientáció +Y | uo. |
