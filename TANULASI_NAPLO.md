# Tanulási napló (Pixel Lili)

## 2026-10-19 | 0. lépés (hibajavítás): sérült WSL fájlrendszer

Készült: a WSL Ubuntu újratelepítve a D:\WSL-re, a környezet (ESP-IDF 5.4.1, PyTorch venv, pocket-tank klón) scriptből visszaépítve; a `.wslconfig`-ból kikerült a kísérleti `sparseVhd`.

Döntés: javítgatás helyett tiszta újratelepítés, mert a WSL-ben csak letölthető eszközök voltak, minden érték a C: repóban és a D: mentésekben él.

Fogalmak: ext4 emergency_ro, vhdx, e2fsck

## 2026-10-19 | 1. lépés: a diag firmware fordítása és első futása

Készült: a `firmware/diag` app lefordul (`tools/build_diag.sh`), a panelre kerül (`tools/flash_216.py diag`), 36 fps-sel fut, a mikrofon, az RTC és a PMIC válaszol.

Döntés: a félbeszakadt session kódjára építünk, mert moduláris (egy fájl egy chip), és a pocket-tank kijelző/PMIC portjait változtatás nélkül használja.

Fogalmak: include guard, szimbólumütközés (linker), flash_args, partíciós tábla

## 2026-10-19 | 2. lépés: M1.8 RTC beállítás és megtartás

Készült: `tools/rtc_sync.py`, ami a PC órájának másodperchatárához igazítva írja a PCF85063-at és megméri az eltérést; az RTC -167 s-ról +-1 s-ra állt.

Döntés: másodperchatárhoz igazított küldés kézi gépelés helyett, mert a regiszter egész másodperces, így a küldés pillanata adja a pontosságot.

Fogalmak: BCD, oszcillátor-leállás jelzőbit (OS)

## Fogalomtár

**ext4 emergency_ro:** az ext4 fájlrendszer hibát észlel (pl. IO hiba egy hirtelen leállás után), és az adatvédelem miatt csak olvashatóra kapcsol; javítani csak lecsatolt állapotban, `e2fsck`-kal lehet.

**vhdx:** a Windows virtuális lemezfájlja; a WSL2 Ubuntu teljes ext4 fájlrendszere egyetlen ilyen fájl (`D:\WSL\Ubuntu-24.04\ext4.vhdx`).

**e2fsck:** az ext2/3/4 fájlrendszer-ellenőrző; a könyvtár- és inode-szerkezet konzisztenciáját javítja, csak nem csatolt fájlrendszeren biztonságos.

**include guard:** a `#ifndef X / #define X / #endif` keret egy fejlécfájl körül, hogy többszöri `#include` esetén is csak egyszer fordítódjon; a guard neve egyedi kell legyen, nem eshet egybe valódi makróval.

**szimbólumütközés (linker):** C-ben a nem `static` függvények globális névtérben vannak; ha két tárgykód ugyanazt a nevet definiálja, a linker "multiple definition" hibát ad. Megoldás: modul-előtag (`pcf_rtc_`) vagy `static`.

**flash_args:** az ESP-IDF által generált fájl: első sora a flash mód/frekvencia/méret, utána "offset fájl" párok; pontosan ezt írja ki az `idf.py flash` is.

**partíciós tábla:** a 16 MB flash felosztása (`partitions.csv`, 0x8000-en); a bootloader ebből tudja, hol az app, hol az NVS. A diag saját táblát használ (egy factory app), a modell partíciót (0x290000) nem írja.

**BCD (binárisan kódolt decimális):** minden tizedes számjegy külön 4 biten; a 0x59 bájt 59-et jelent. Az RTC-k így tárolják az időt, mert közvetlenül kijelezhető; olvasáskor/íráskor át kell váltani (`bcd2bin`, `bin2bcd`).

**OS bit (oscillator stop):** a PCF85063 másodperc-regiszterének 7. bitje; 1, ha az óra oszcillátora valaha leállt (pl. teljes áramtalanítás) a legutóbbi beállítás óta. Ezért "valid" csak akkor az idő, ha ez a bit 0.
