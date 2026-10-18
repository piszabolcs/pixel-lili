# Panel-jegyzetek (Lili, 2.16)

Panel: Waveshare ESP32-S3-Touch-AMOLED-2.16, ESP32-S3 rev v0.2, MAC 90:70:69:fe:6c:74. Windows: `Soros USB-eszköz (COM3)` (beépített USB-Serial-JTAG, külön driver nem kellett).

## M1.3 Gombok (kész, 2026-10-18)

A gazdi tapasztalata a pocket-tank alatt: **csak a PWR gomb csinál valamit**, a másik kettő semmit. A forrás ezt megmagyarázza:

| Gomb | GPIO | Pocket-tank viselkedés | Lili terv (spec 5.4) |
|---|---|---|---|
| PWR (középső, a PMIC-en át) | SYS_OUT = GPIO 16 (`S_PIN_PWR_SENSE`, magas amíg nyomva) + AXP2101 IRQ | rövid = alvás/ébresztés, 1,5 s = kikapcsolás, 10 s = PMIC saját vágás | ugyanez |
| BOOT | GPIO 0 (`BTN_SLEEP` a `main.c`-ben) | be van olvasva, de PMIC-es lapon (`s_pmic`) az alvás-funkciója tiltva, ezért nem csinál semmit | állapotsáv be/ki, hosszan némítás |
| KEY3 | GPIO 18 (`S_PIN_KEY3`, "unused") | nincs kezelve | nézetváltás szoba <-> udvar |

A soros naplóban a gombnyomások alatt nem jelent meg esemény (`docs/logs/2026-10-18_frames_and_buttons_216.log`), ami egyezik a fentiekkel. Melyik fizikai gomb a BOOT és melyik a KEY3, az M2.1-ben derül ki, amikor a saját kódunk logolja őket.

## M1.4 IMU megszakítás (nyitott)
A square lapon **nincs I/O expander** (a 1.75-ön a QMI_INT1 az expander P6 lábán van). A `board_pins.h` a square-hez nem ad IMU INT GPIO-t. A kapcsolási rajzból kell kinézni (Waveshare wiki, `resources/ESP32-S3-Touch-AMOLED-2.16/`), van-e közvetlen GPIO; ha nincs, marad a periodikus ébredés + lekérdezés (spec 5.7 kockázat).

## M1.7 Sarok (CORNER_R)
Még nincs mérve. A pocket-tank 40 px-szel fut (a gyártó képei alapján).

## Megfigyelések az első bootból
- A boot ~8 s-ig tart a modell bench miatt (1,1 s -> 9,2 s), ez alatt a kijelző már él.
- Az RTC érvényes időt adott az első bootkor (2026-10-18 17:08), külön beállítás nélkül.
- Az ES7210 (mikrofon) felfüggesztve indul; az M1.6 diag appban kell élesíteni.
