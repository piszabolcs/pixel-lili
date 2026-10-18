# Panel-jegyzetek (Lili, 2.16)

Panel: Waveshare ESP32-S3-Touch-AMOLED-2.16, ESP32-S3 rev v0.2, MAC 90:70:69:fe:6c:74. Windows: `Soros USB-eszköz (COM3)` (beépített USB-Serial-JTAG, külön driver nem kellett).

## M1.3 Gombok (a gazdi tölti ki)

| Fizikai hely / felirat | Mit csinált a pocket-tank alatt | Várt GPIO a spec szerint |
|---|---|---|
| középső | | PWR (PMIC, SYS_OUT GPIO 16): rövid = alvás/ébresztés, 1,5 s = ki |
| | | BOOT GPIO 0 |
| | | KEY3 GPIO 18 |

## M1.7 Sarok (CORNER_R)
Még nincs mérve. A pocket-tank 40 px-szel fut (a gyártó képei alapján).

## Megfigyelések az első bootból
- A boot ~8 s-ig tart a modell bench miatt (1,1 s -> 9,2 s), ez alatt a kijelző már él.
- Az RTC érvényes időt adott az első bootkor (2026-10-18 17:08), külön beállítás nélkül.
- Az ES7210 (mikrofon) felfüggesztve indul; az M1.6 diag appban kell élesíteni.
