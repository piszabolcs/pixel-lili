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

## 2026-10-19 | 3. lépés: M1.7 sarokteszt

Készült: a sarokteszt 32..72 px közötti 6 ívvel; mérés: CORNER_R = 72 fizikai px (36 logikai); a diag lábléce ehhez igazítva rövidebb és középre zárt.

Döntés: a tesztet kibővítettük 72 px-ig, mert az első fotó már mutatta, hogy a spec 40 px-es kiinduló értéke kevés; a 72-t választottuk, mert az a legkisebb, ami teljesen látszik.

Fogalmak: fizikai és logikai pixel, biztonsági zóna

## 2026-10-19 | 4. lépés: M1.6 mikrofon és szintmérő

Készült: `tools/mic_tone_check.py`, ami ismert hangokat játszik a PC hangszórójából és összeveti a panel mérésével; a lánc 1 Hz-en belül pontos, a zajpadló -72 dBFS, a beszéd -26 dBFS körül.

Döntés: ismert frekvenciájú tesztjelet használtunk, mert egy hibás órabeállítás minden frekvenciát ugyanazzal a szorzóval torzítana, és ezt csak egy ismert bemenet leplezi le.

Fogalmak: I2S, MCLK (256 fs), dBFS, FFT bin

## 2026-10-19 | 5a-5b. lépés: füttydetektor mérése és hangolása szimulációval

Készült: a detektor keretenként naplózza a mérőszámait (tonalitás `frac`, mély sáv `low_rel_db`), a panel vezetett címkézett felvételt készít (`label`), a PC-n `tools/whistle_stats.py` és `tools/whistle_sim.py` visszajátssza; a régi detektor 17 téves riasztást adott, az új kapukkal 0, és mind a 11 fütty megvan.

Döntés: a küszöböket nem a panelen próbálgattuk, hanem a felvett jellemzőkön PC-n, mert így egy mérésből bármennyi változat kipróbálható újraflashelés nélkül; a kiválasztott érték egy széles "plató" közepe, nem a széle.

Fogalmak: címkézett adat, tonalitás (spektrális koncentráció), robusztus küszöb (plató), újraélesítés (refrakter idő)

## 2026-10-19 | 5c. lépés: füttydetektor v3, élő teszt és második mérés

Készült: a v1 kapuk élőben túl szigorúak voltak (erős, hosszú füttyöt kértek), a lazított v2 a podcast mellett határon volt; egy második, normál füttyökkel és podcasttal készült felvételből egy hangerőkapu (a sorozat leghangosabb kerete >= -42 dBFS) lett a v3, ami mindkét felvételen 24/24 füttyöt és 0 téves riasztást ad.

Döntés: 250 ms helyett 192 ms (eltérés a spectől, rögzítve), mert a hosszt a hangerő jobban helyettesíti; a hangerőkapu a két csoport közötti rés közepén van.

Fogalmak: reprezentatív tanítóadat, redundáns kapuk, mintavételi hiba a fázishatáron

## 2026-10-19 | 6. lépés: M1.4 IMU mozgásra ébresztés

Készült: bizonyítva, hogy a QMI8658 INT1 a GPIO 17, és a felemelés 338 s alvás után felébresztette a panelt; az alvás előtt nyugalmat vár és az INT1 aktuális szintjének ellentétére élesít.

Döntés: szintváltásra (az aktuális szint ellentéte) ébresztünk fix "magas szint" helyett, mert az adatlap szerint a láb eseményenként átvált, és a fix szint az első próbán azonnali téves ébredést okozott.

Hiba és tanulság: a "lefagyott" látszat oka az volt, hogy az USB-konzol könnyű alvás után nem éled vissza; a panel közben futott. Az eredményt a kijelző (és az NVS-napló) mutatta meg. Korábban észrevehető lett volna, ha az alvásos teszt eredményét eleve a kijelzőről vagy a plog-ból olvasom, nem a konzolról.

Fogalmak: WoM (wake-on-motion), szintérzékeny ébresztés, könnyű alvás

## 2026-10-19 | M2, 1. lépés: Lili saját szimulátor-célja és a közös mag

Készült: `common/lili/` (config.h, lili_types.h, lili_world.c/h) és `sim/lili_main.c`; `make LILI=1` -> `lilisim` (ablak: Tab szoba/udvar, Q kilépés), `make LILI=1 check` zöld; a halas `fishsim-square` érintetlenül fordul tovább.

Döntés: külön szimulátor-cél a pocket-tank 5439 soros main.c-jének kibelezése helyett, mert így a halas kód referencia marad, és a pocket-tank frissítései konfliktus nélkül átvehetők.

Fogalmak: platformfüggetlen mag (common), determinisztikus PRNG (xorshift32), helyőrző grafika

## 2026-10-19 | M2, 2. lépés: 8 bites indexelt renderelő

Készült: `common/lili/render8.c/h` (240x240 bájtos vászon, 256 elemű RGB565 LUT, 2x nagyító flush sorsávokra), `palette.c/h` (indextérkép: 0 átlátszó, 1-15 UI, 8-as rámpák), a világ már indexekben rajzol; selftest: LUT, 2x, 24 soros sávok, páratlan sávkezdet, átlátszóság, vágás; `--bench`.

Döntés: a szín csak a flush-ban születik meg, mert így a napszak/évszak egy LUT-csere, a vászon nyolcad akkora, és a panel sávjai ugyanabból a függvényből jönnek, mint a szimulátor teljes képe.

Hiba: a `make check` a /mnt/c-n percekig tartott (az LVGL ~300 függőségfájljának fejlécellenőrzése 9P-n); a Lili-cél csak a saját függőségeit követi, így 5,4 s.

Fogalmak: indexelt szín és paletta, LUT, rámpa, 32 bites dupla írás

## 2026-10-19 | M2, 3. lépés: kávamaszk, vignetta, sarokzóna

Készült: `common/lili/mask.c/h`: a fizikai képen a 72 px-es sarkok feketék, a szélen 6 px-es elsötétedés, egy negyed táblájából (57,6 KB), csak a keret sávjához nyúlva; `M` billentyű: a tiltott sarokzónák sraffozva; selftest visszaméri az M1.7-et (y 462: x 24 takarva, x 32 látszik).

Döntés: fizikai felbontáson, a flush után, előre számolt táblából, mert így a sarokív 1 px-es pontosságú és képkockánként csak kb. 22 ezer pixel kell.

Hiba: a vizestál korábbi "26 px az ívtől" távolsága rossz geometria volt (a pont a sarokkör középpontján belül van, ott az egyenes szél számít, 41,5 px); a decisions.md javítva, a selftest most a valódi függvénnyel ellenőrzi.

Fogalmak: előjeles távolságfüggvény (SDF), szimmetria-kihasználás

## 2026-10-19 | M2, 4. lépés: szoba és udvar térképből

Készült: `assets/maps/room.json`, `yard.json` (spec 8.2-8.3: padló, akadályok, pontok, zónák, bútorok), `tools/gen_maps.py` -> `common/lili/maps_gen.c/h` (60x60-as járhatósági bitmaszk, SPOT_* azonosítók, RAMP_ID_*), a világ a bútorokat dobozként rajzolja, Lilivel együtt mélység (Y) szerint rendezve; `M` billentyű: járhatatlan cellák és pontok; selftest: minden pont járható, a biztonsági zónában vannak az érinthető pontok, az ágy mögött Lilit eltakarja az ágy.

Döntés: JSON forrás, fordításkori C-generálás, mert a panelen nincs mit JSON-t elemezni (kód, RAM, hibalehetőség), a JSON viszont olvasható és szerkeszthető marad.

Hibák: (1) `/*` a kommentben (`assets/maps/*.json`) beágyazott komment-figyelmeztetést adott; (2) két selftest rossz pixelt választott (a mellkas fehér, az udvaron a ház fala), a kód jó volt. Tanulság: tesztpontot a rajzolás koordinátáiból számolva kell választani, nem szemre.

Fogalmak: kódgenerálás (build-time), bitmaszk, Y-rendezés (painter's algorithm)

## 2026-10-19 | M2, 4b. lépés: a szőnyeg az ágy helyett, új alsó sor

Készült: a `room.json`-ban az ágy helyén a gazdi szőnyege (lapos, járható, háromszögminta 4 színnel és bolyhos pöttyözéssel), a pelenka a bal, a tálak a jobb harmadban, középen a dísz helye; a generátor `pattern` mezőt kapott; a mélységi tesztet egy udvari bokor váltotta.

Döntés: a szőnyeg mintáját kód rajzolja (helyőrző), a pöttyözés a pixel koordinátájából számolt hash, így minden képkockán azonos; a végleges textúra az M3-ban a fotó alapján készül.

Fogalmak: hash-alapú (determinisztikus) textúra

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

**fizikai és logikai pixel:** a kijelző 480x480 fizikai pixel; a játék világa 240x240 logikai rácson él, és kirajzoláskor minden logikai pixel 2x2 fizikai pixel lesz. A sarok 72 fizikai px = 36 logikai px.

**biztonsági zóna:** a képernyőnek az a része, ahol egy elem biztosan teljesen látszik és megérinthető. A lekerekített sarkoknál egy pont akkor látszik, ha a sarokkör középpontjától legfeljebb R távolságra van; a spec 5.2 szerint az érinthető elemek közepe ennél még 24 logikai px-szel beljebb legyen.

**I2S:** soros hangbusz három vonallal: BCLK (bitóra), LRCK/WS (melyik csatorna: bal vagy jobb), adat. Itt az ESP a master, ő adja az órákat, az ES7210 ezekhez igazodva küldi a mintákat.

**MCLK (256 fs):** a kodek belső mesterórája, a mintavételi frekvencia (fs) egész számú többszöröse: 256 x 16 kHz = 4,096 MHz. Ebből osztja le az ADC a saját ütemét; ha az osztó nem illik az MCLK-hoz, a mintavétel elcsúszik.

**dBFS:** decibel a digitális végkitéréshez (full scale) képest: 0 dBFS a legnagyobb ábrázolható szint, -72 dBFS a 16 bites tartomány alján lévő zaj. Nem hangnyomás (dB SPL), csak a digitális szint.

**FFT bin:** a 512 pontos FFT a 0..8 kHz-et 256 egyenlő sávra (binre) osztja, mindegyik 16000 / 512 = 31,25 Hz széles. A csúcs pontos helyét a szomszéd binek arányából lehet finomítani, így lett a hiba 1 Hz-en belüli.

**címkézett adat:** minden minta mellett ott az ismert igazság (ez fütty, ez beszéd). Ebből választható olyan küszöb, ami a csoportokat a legjobban szétválasztja; ugyanez az elv az LLM tanításánál.

**tonalitás (spektrális koncentráció):** a sáv energiájának hányada a csúcs körüli néhány binben. Egy fütty majdnem tiszta szinusz (~1,0), a beszéd és a zaj szétterül (0,3-0,7).

**robusztus küszöb (plató):** ha egy paramétertartományban mindenhol ugyanaz a jó eredmény jön ki, a tartomány közepét választjuk, nem a szélét, mert így egy kicsit más környezetben (más szoba, más zaj) is jó marad.

**újraélesítés (refrakter idő):** egy esemény után a detektor csak egy rövid csendes szakasz után jelezhet újra, így egy akadozó fütty nem számít kettőnek.

**reprezentatív tanítóadat:** a hangoláshoz használt felvételnek tartalmaznia kell azokat a helyzeteket, amik élesben előfordulnak; az első felvételen nem volt normál hangerejű fütty és nem volt zenés podcast, ezért a belőle hangolt küszöbök élesben rosszul működtek.

**redundáns kapuk:** több független feltétel együtt; ha bármelyik kizárja a zavart, a többi lazább lehet. Ezért lehetett egyenként sokat lazítani téves riasztás nélkül, és ezért kell a lazítást mindig együtt is ellenőrizni.

**hiba a fázishatáron:** a címke idő szerint vált, a valóság nem: egy fütty átnyúlhat a következő fázisba és ott "téves riasztásnak" látszik. Elemzéskor a fázisok első 1-2 másodpercét külön kell kezelni.

**WoM (wake-on-motion):** a gyorsulásmérő alacsony fogyasztású módban maga figyeli a mozgást (két egymás utáni minta különbsége a küszöb felett), és egy lábon jelez; így az ESP alhat, és csak mozgásra ébred.

**szintérzékeny ébresztés:** könnyű alvásban a GPIO-ébresztés egy szintre (magas vagy alacsony) éled, nem élre. Ha alváskor a láb már az ébresztő szinten áll, a chip azonnal felébred; ezért kell az aktuális szint ellentétére élesíteni.

**könnyű alvás (light sleep):** a CPU-k órája leáll, a RAM megmarad, a program az alvás utáni sorban folytatódik; az ébredés ms-os. A mélyalvással ellentétben nincs újraindulás, de több áramot fogyaszt.

**platformfüggetlen mag:** a `common/lili` csak szabványos C-t használ (nincs SDL, LVGL, ESP-IDF, malloc); a szimulátor és a firmware ugyanezt a forrást fordítja, és csak a "testet" adja hozzá (ablak vagy kijelző, egér vagy érintés). Így a logika a PC-n tesztelhető, és ugyanúgy fut a panelen.

**determinisztikus PRNG (xorshift32):** három eltolás és XOR egy 32 bites állapoton; ugyanabból a magból mindig ugyanaz a sorozat jön, minden gépen. Ezért reprodukálhatók a selftestek (spec 4.6).

**helyőrző grafika:** a végleges sprite-ok helyén egyszerű formák (spec 7.7), hogy a világ, a mozgás és az agy fejlesztése ne várjon a Blenderes grafikára; a formátum ugyanaz, csak a tartalom cserélődik.

**indexelt szín és paletta:** a pixel nem színt tárol, hanem egy sorszámot (0-255) egy 256 színű palettában. Egy pixel 1 bájt 2 helyett, és a paletta cseréjével az egész kép átszíneződik.

**LUT (lookup table):** előre kiszámolt táblázat; itt indexből RGB565 színt ad egyetlen tömbolvasással, számolás nélkül. A forró ciklusban ez a leggyorsabb.

**rámpa:** egy anyag (bunda, padló, fű) 8 árnyalata egymás után a palettában, sötéttől világosig. A toon árnyalás és a helyi fények (napfolt, este) csak eltolják az indexet a rámpán belül.

**32 bites dupla írás:** a 2x vízszintes nagyításnál ugyanaz a 16 bites szín kétszer kell egymás mellé; egy 32 bites szóba kétszer betéve egyetlen írással megvan, és a következő fizikai sor egy `memcpy`.

**előjeles távolságfüggvény (SDF):** egy pont távolsága egy alakzat határától, belül negatív, kívül pozitív előjellel. Egy lekerekített négyzetre egy rövid képlet; ebből jön a maszk (kívül fekete), a vignetta (a határ közelében halványít) és a biztonsági zóna (legalább 24 px-re belül) is, egyetlen függvényből.

**szimmetria-kihasználás:** ha egy forma tükrös az x és y tengelyre, elég egy negyedét kiszámolni és tárolni; a másik háromba tükrözött indexszel olvasunk. Itt 4x kevesebb memória (57,6 KB 230 KB helyett).

**kódgenerálás (build-time):** egy script fordításkor állít elő forráskódot egy adatfájlból (itt JSON-ból C-táblákat). Az adat egy helyen él, a generált kódot kézzel nem szerkesztjük; a make újragenerálja, ha a forrás változik.

**bitmaszk:** sok igen/nem érték bitenként tárolva; a 60x60 = 3600 járhatósági cella 450 bájt. Kiolvasás: a cella sorszámából bájt (`>> 3`) és bit (`& 7`).

**Y-rendezés (painter's algorithm):** hátulról előre rajzolunk, mint a festő: a kisebb Y (távolabbi) elem előbb, a nagyobb Y (közelebbi) később, így a közelebbi eltakarja a távolabbit. A kulcs a talppont Y-ja.

**hash-alapú textúra:** a pixel koordinátáiból egy keverőfüggvénnyel (hash) "véletlenszerű" számot képzünk, és ebből döntjük el, sötétebb-e. Mivel csak a koordinátától függ, minden képkockán ugyanaz, nem villog, és nem kell hozzá tárolt kép vagy PRNG-állapot.
