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
| 2026-10-18 | Képkocka-idő a panelen (pocket-tank square, 2 hal, első-indítás oldal) | 39,1-41,8 ms/képkocka (~24-25 fps): polls 0,8 + tick 1,4-2,0 + render 8,0-11,0 + flush 16,5-22,4 (fill 13,5-20,0) + sleep 5-12 ms | docs/logs/2026-10-18_frames_and_buttons_216.log |
| 2026-10-19 | RTC eltérés beállítás előtt | RTC - PC = -167 s (a pocket-tank boot-kori seedje óta, nem drift) | tools/rtc_sync.py --check, diag fw |
| 2026-10-19 17:23:09 | RTC beállítás a PC órájához igazítva | utána RTC - PC = -0,61 .. +0,17 s (egész másodperces regiszter, +-1 s) | tools/rtc_sync.py |
| 2026-10-19 17:27 | RTC megtartás PMIC-kikapcsolás után (ki 17:26:19, be 17:26:47, akkuról) | RTC - PC = -0,56 .. +0,22 s, valid (OS bit 0): az idő megmaradt | tools/rtc_sync.py --check, diag powerlog |
| 2026-10-19 | Kijelző sarok sugara (látható üveg) | 64 px ív levágva, 72 px ív teljesen látszik -> CORNER_R 72 fizikai px; egyenes széleken nincs takarás (px 0 és 479 látszik) | diag sarokteszt, gazdi fotója |
| 2026-10-19 | Mikrofonlánc frekvenciapontossága (ES7210, I2S 16 kHz, FFT 512) | lejátszva 1250 / 2000 / 3000 Hz -> mérve 1251 / 2000 / 3000 Hz (hiba <= 1 Hz, egy bin 31,25 Hz) | tools/mic_tone_check.py, PC hangszóró |
| 2026-10-19 | Mikrofon szintek (gain kód 10 = 30 dB) | csendes zaj: L -73 / R -72 dBFS; tesztton a hangszóróból: -31..-38 dBFS csúcs; beszéd (podcast): -26..-29 dBFS | diag status sor |
| 2026-10-19 | Füttydetektor, címkézett felvétel (94 s: csend, 11 fütty, beszéd/podcast, letevés 5x, kopogás) | régi detektor: fütty 11, téves: beszéd 5, letevés 11, kopogás 1. Új kapuk (ratio > 20 dB, frac >= 0,85, low_rel <= 0 dB, csúcs > -55 dBFS, 8 keret, 1 keret rés, 6 keretes újraélesítés): fütty 11, téves 0 | tools/whistle_sim.py, docs/logs/2026-10-19_whistle_labelled.log |
| 2026-10-19 | Füttydetektor v3, 2. címkézett felvétel (5 s csend, 13 normál fütty, 75 s podcast) | v3 visszajátszva: 13/13 fütty (az utolsó átnyúlik a podcast fázisba), podcast alatt 0 téves; régi detektor ugyanezen 8 téves. Élőben (v2 a panelen): 13 esemény = 13 fütty, podcast alatt 0 | tools/whistle_sim.py, docs/logs/2026-10-19_whistle_labelled_2.log |
| 2026-10-19 | IMU WoM ébresztés könnyű alvásból | 338 s nyugalmi alvás után a felemelés ébresztett (ok: GPIO 17 / INT1); ébren 100 mg küszöbbel minden felemelés eseményt adott | diag womsleep, kijelző fotó |
| 2026-10-19 | 8 bites renderelő a PC-n (lilisim --bench, 2000 képkocka) | render 0,9-1,0 us, flush (LUT + 2x, 480x480) 27 us / képkocka; vászon 57 600 B a 460 800 B-os RGB565 kép helyett | WSL2, Ryzen 5 7600 (a panel számai: M2.6) |
| 2026-10-19 | make LILI=1 check a /mnt/c-n | kb. 5 perc -> 5,4 s (az LVGL .d fájljai kimaradnak a Lili-célnál) | sim/Makefile |
| 2026-10-19 | Kávamaszk a PC-n (lilisim --bench) | mask 18,7 us / képkocka (flush 23,6 us, render 0,9 us); a tábla 57 840 B, képkockánként kb. 22 ezer pixel érintett | WSL2 |
| 2026-10-19 | Térképek (gen_maps) | szoba: 2057 / 3600 járható cella, udvar: 2575 / 3600; 18 pont; render a bútorokkal 4,1 us / képkocka a PC-n | tools/gen_maps.py, lilisim --bench |
| 2026-10-19 | Render a szőnyeg háromszögmintájával | 98,6 us / képkocka a PC-n (előtte 12 us): a statikus háttér gyorsítótárba kerül az M2.6-ban | lilisim --bench |
| 2026-10-19/20 | **M1.5 fogyasztás, ACTIVE** (diag, 36 fps, fényerő 255, mikrofon fut) | 60 perc: 100 -> 92 % = 80 mAh -> **~80 mA** (a 19:45-20:25 szakaszból, a töltésmérő 10 perces 100 %-os platója nélkül: ~90 mA); ~11-12 óra képernyőidő | plog, AXP2101 SoC, 1 % = 10 mAh |
| 2026-10-19/20 | **M1.5 fogyasztás, DIM** (fényerő 30 %, 10 fps, mikrofon fut) | 60 perc: 92 -> 88 % = 40 mAh -> **~40 mA** | plog |
| 2026-10-19/20 | **M1.5 fogyasztás, könnyű alvás** (kijelző ki, mikrofon ki, WoM élesítve, 30 percenként ébredés) | 8,97 óra: 88 -> 76 % = 120 mAh -> **13,4 mA átlag**; a töltésmérő 21:30-00:00 között 2,5 órán át 88 %-on állt, utána egyenletesen 1 % / 30 perc = 20 mA. A valós érték **13-20 mA** között. A pocket-tank könnyű alvása 4,7 mA volt (docs/DEVICE.md): nálunk valami még fogyaszt (gyanú: ES8311 + erősítő, AMOLED sleep-in, USB-PHY), ez az M5.6 dolga | plog |
| 2026-10-19/20 | WoM éjszaka | 23:08-kor 18, 06:28-kor 12 WoM ébredés néhány másodpercen belül (a panel felvétele), mindegyik után visszaaludt; hamis ébredés nyugalomban nem volt | plog |
| 2026-10-20 | Inspector (lilisim ablak 840 x 480, LVGL) | képkocka 1,2 ms, 30 fps a headless felvételben; a világ 112 B (esemény-számlálókkal) | lilisim --inspector-shot |
