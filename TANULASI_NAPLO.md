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

## 2026-10-19 | M2, 4c. lépés: a bútorok a szélekhez, új láthatósági szabály

Készült: a sarokülő, a pelenka és a tálak 70 %-kal közelebb a jobb és az alsó szélhez; `mask_rect_visible`: egy tárgy teljes téglalapja az üvegen van-e (a lekerekített négyzet konvex, ezért elég a 4 sarkát ellenőrizni).

Döntés: a "középpont 24 px-re a saroktól" szabályt a mérhetőbb "a teljes tárgy látszik, 2 px margóval" váltotta, mert a 24 becsült szám volt, és a kérés (a szélekre tett tárgyak) e nélkül nem teljesíthető.

Hiba: a Python-heredocban írt C-stringben a `\n` megint valódi sortöréssé vált; C-stringet ezután csak szerkesztőeszközzel írok.

Fogalmak: konvex alakzat (sarokellenőrzés)

## 2026-10-19 | M2, 4d. lépés: a sarokülő és a szőnyeg javítása

Készült: a sarokülő az eredeti hosszában a jobb falnál, egy L-ként; a szőnyeg 3:2-es, a fotó mintája 12 háromszögből, a háromszögön belüliséget élfüggvények döntik el.

Döntés és hiba: a "le a széléig" kérést félreértettem (meghosszabbítottam a sarokülőt), és a szőnyeget általános X-mintára egyszerűsítettem; most a fotó és a valódi arányok az alap. Tanulság: térbeli kérésnél egy mondatban visszaigazolom, mit értettem (mozgatás vagy átméretezés), mielőtt átírom.

Fogalmak: élfüggvény (pont a háromszögben)

## 2026-10-20 | 7. lépés: M1.5 fogyasztásmérés, M1 lezárva

Készült: egy éjszakás automatikus mérés (60 perc ACTIVE, 60 perc DIM, 9 óra könnyű alvás, sorok az NVS-ben): ~80-90 / ~40 / 13-20 mA; DESK türelmi idő javaslat 8 óra.

Döntés: hosszú ablakok (1 óra, egy éjszaka), mert a töltésmérő 1 %-os (10 mAh-s) lépésű, és a 100 %-on platózik; 20 perces ablakból +-30 mA hiba lett volna.

Fogalmak: kvantálási hiba (mérésnél), töltésmérő (fuel gauge)

## 2026-10-20 | M2, 5. lépés: eseménysor és inspector panel

Készült: `common/lili/events.c/h` (17 eseménytípus, 32 elemű gyűrűs sor, tele sornál a legrégebbit dobja), `world_handle_event` (nézetváltás, időugrás, évszak, napszak), `sim/inspector.c/h` (LVGL: gombok, állapotkijelzés, eseménykonzol, kattintás a képen = TAP), a billentyűk is eseményt küldenek; `--inspector-shot` az egész ablakot PPM-be menti kijelző nélkül.

Döntés: minden bemenet (gomb, egér, később érintés, IMU, mikrofon) ugyanazon a soron át jut a világba, mert így a világ nem tudja, ki küldte, az inspector és a panel azonos viselkedést ad, és egy felvett eseménysorral ugyanaz az élet visszajátszható (selftest).

Fogalmak: gyűrűs puffer (ring buffer), termelő-fogyasztó minta

## 2026-10-21 | M2, 6. lépés: a Lili-világ a panelen
Készült: `firmware/lili` (új ESP-IDF alkalmazás: a `common/lili` mag + a pocket-tank kijelző-portja), `display_port_flush_gen` (a csíkot a hívó callbackje tölti közvetlenül a DMA pufferbe, nincs keretpuffer), `world_render_bg/fg/cached` (a statikus réteg gyorsítótára), `tools/build_lili.sh`, `flash_216.py lili`. Mért: 40 fps gyorsítótár nélkül, **62,5 fps** gyorsítótárral, belső heap 270 KB szabad.

Döntés: nincs 460 KB-os RGB565 keretpuffer; minden 24 soros csíkot a callback állít elő a 8 bites vászonból (LUT, 2x, maszk, bájtcsere), mert így a PSRAM nem lesz a szűk keresztmetszet, és a vászon 8-szor kisebb. A statikus réteget (padló, fal, lapos propok: szőnyeg, pelenka, tálak) nézetenként egyszer rajzoljuk, utána másoljuk vissza.

Fogalmak: csíkos generálás (callback), vezetékbeli bájtsorrend, memoizálás (gyorsítótár), képkocka-költségvetés

## 2026-10-21 | M3, 1. lépés: a Blender jelenet és a közös kamera
Készült: `tools/blender/lili_common.py` (kamera, fény, toon anyag, irányok), `tools/blender/scene_setup.py` (kollekciók, kamera-ellenőrzés, helyőrző kutya 8 irányban), `tools/blender_run.py` (fejetlen Blender Windows-ról), `tools/contact_sheet.py` (kontaktlap). Kép: `docs/img/m3_1_check.png`.

Döntés: a kamera számai (35 fok, 1,25 cm/px, pivot a (24, 40)-en) egyetlen fájlban élnek, és a szkript minden futáskor számokkal bizonyítja őket, mert ha a háttér és Lili kamerája eltér, a kutya "lebeg" vagy belesüllyed a padlóba. A toon szín emisszióként jön ki, így a render pontosan a palettaszínt adja.

Hiba: az első futáson 3 ellenőrzés elbukott. Tünet: a mélység 16 px-t ért 9,2 helyett, a magasság 0-t. Gyökérok: a kamera forgatását beállítottam, de a Blender a `matrix_world`-öt csak a jelenet frissítésekor számolja újra, a vetítő függvény a régi (lefelé néző) mátrixot olvasta. Javítás: `view_layer.update()` a kamera beállítása után. Korábbi észlelés: pont erre való a beépített ellenőrzés; a renderek jók voltak, mert a render maga frissít.

Fogalmak: ortografikus vetítés, fejetlen (headless) futtatás, származtatott mátrix frissítése, emissziós toon árnyalás

## 2026-10-21 | M3, 1b. lépés: élő Blender (MCP)
Készült: a `mcp-for-blender` 2.2.2 (a blender-mcp új neve) telepítve: Blender-kiegészítő + MCP szerver a Claude Code-ban, verzióra rögzítve, telemetria háromszorosan tiltva. `tools/blender_live.py`: ugyanazon a helyi socketen parancsot küld a nyitott Blendernek (jelenet listája, nézetkép, kód futtatása).

Döntés: telepítés előtt a letöltött csomag kódját átnéztem, mert a kiegészítő tetszőleges Python kódot futtat a Blenderben, és a csomag adatgyűjtő modult is tartalmaz. A gyártás továbbra is gyári beállításokkal fut, így a kiegészítő nem befolyásolja a buildet.

Fogalmak: MCP (Model Context Protocol), helyi socket (localhost port), verzió-rögzítés, opt-in telemetria

## 2026-10-21 | M3, 2. lépés: a mester-paletta a célképekből
Készült: `tools/palette_from_photos.py` (k-means Oklabban a célképek kijelölt területein, horgony-színek, 8 árnyalatú rámpák), `assets/palettes/master.gpl` + `master.json`, `docs/img/m3_2_palette.png` (jóváhagyási kapu). `--probe` mód: egy terület domináns színei, ezzel választottam a területeket.

Döntés: a színek a jóváhagyott célképekből jönnek, nem a nyers fotókból (azok a telefon fehéregyensúlyát hordozzák). A rámpa nem a bázisszín sötétítése és világosítása (az sáros lesz), hanem Oklabban lépked, és az árnyék a kék, a fény a sárga felé hajlik, ahogy a hi-bit pixel artban.

Hiba: az első futás elakadt, mert az ajtón át látszó kert zöldjét "zöld árnyalat" szabállyal kerestem, de a terület nagy része napfényes krém, zöld alig volt. Javítás: horgony-szín (a klaszter, ami a megadott színhez a legközelebb áll), ez minden bizonytalan anyagra jobb, mint az árnyalat-szabály.

Fogalmak: Oklab, k-means, horgony-szín, hue shift az árnyékban

## 2026-10-21 | M3, 3. lépés: a teljes gyártósor a helyőrző kutyával
Készült: `assets/anims.json` (katalógus), `tools/gen_palette.py` (paletta a magba), `tools/blender/lili_rig_placeholder.py` + `render_all.py` (96 kocka), `tools/pixelize.py` (B7: módusz-szín, 50 % fedés, 1 px kontúr, kézi javítás védelme), `tools/pack_assets.py` (LPAK + `asset_ids.h`), `common/lili/pak.c/h` (CRC, TOC, RLE-dekódolás közvetlenül a vászonra), `tools/build_assets.py` (egy parancs). A szimulátor betölti a pakot és a sprite-ot rajzolja; selftestek a pakra.

Döntés: a sprite-ot nem csomagoljuk ki a memóriába, hanem minden képkockán az RLE-ből rajzoljuk: a dekódolás olcsóbb, mint a másolás, és 1000 kocka kicsomagolva 2,3 MB lenne a 48 x 48-as cellákkal. A pak egyetlen puffer, a világ csak egy mutatót tart rá.

Hiba 1: a szimulátor összeomlott (segfault) a pak tesztjénél. Tünet: a selftest kimenet nélkül állt le. Gyökérok: a Python író 12 bájtos sprite-fejlécet ír, a C olvasó 13-at várt; a kockarekordok 1 bájttal eltolva, az RLE-mutató a semmibe mutatott. Javítás: 12 a C-ben is, és a formátum leírása a `pack_assets.py` fejlécében az egyetlen hiteles forrás. Korábbi észlelés: egy formátum két megvalósításánál a bájtszámot egy helyről kell venni; a selftest azonnal megfogta.

Hiba 2 (ismét): a Python heredoc a C `
`-t valódi sorvéggé írta. Javítás: a C stringeket tartalmazó változtatásokat csak az Edit eszközzel. Harmadszor fordult elő; mostantól szabály.

Fogalmak: RLE (futamhossz-kódolás), CRC-32, tartalomjegyzék (TOC) egy bináris fájlban, módusz-szín a pixelesítésnél

## 2026-10-21 | M3, 4. lépés: Lili modellje
Készült: `tools/blender/lili_model.py` (a modell szkriptből: test, fej, pofa, orr, homlokcsík, szemek fénnyel, két különböző fül, lábak mancsokkal, kunkorodó farok Bezier-görbéből), `assets/blender/lili.blend`; a `render_all.py` és a `build_assets.py` felismeri a modellt (új `model` lépés). Képek: `docs/img/m3_4_lili_8dir.png`, `docs/img/sheets/idle_stand.png`.

Döntés: a modell nem kézzel, hanem szkriptből épül, mert így bármikor újraépíthető, a módosítás egy szám átírása, és a .blend-et a gazdi a Blenderben megnézheti. A fogantyúk (root, head, leg0-3, tail) ugyanazok, mint a helyőrzőnél: az animációk kódja egy sort sem változott.

Hiba: az első változaton a fülek alig látszottak. Tünet: 2 cm állt ki a fejből. Gyökérok: a fül talpát a koponya közepéhez képest adtam meg, és az a gömb belsejébe esett. Javítás: a talp a koponya felső élére, a fül 13 cm. A kontaktlap mutatta meg; ezért van jóváhagyási kapu minden vizuális lépésnél.

Fogalmak: Bezier-görbe bevel-lel, fogantyú (handle) a rigben, méretellenőrzés a cellában

## 2026-10-21 | M3, 4b. lépés: natív 480-as felbontás
Készült: `LILI_SCALE 1` és `LILI_K` a `config.h`-ban, a flush natív ága, a térképek tervezőrácsról skálázása (`gen_maps.py`), a kézi rajzolás és a selftestek `K`-val, a Blender oldal 96 px-es cellával, selective outline és vektorizált pixelesítés; minden asset újragenerálva; a panelen mérve 38 fps.

Döntés: a térképeket nem írtuk át 480-ra, hanem maradtak a 240-es tervezőrácson, és egy szorzó (`LILI_K`) viszi át őket. Így a spec és a térképek számai változatlanok, és ha egyszer más felbontás kell, egy szám változik.

Hiba: a firmware nem fordult, mert a `config.h` megjegyzésében `*.json` szerepelt, ami a C-ben megjegyzés-nyitásnak számít (`/*` a megjegyzésen belül, -Werror). Korábbi észlelés: ugyanez már egyszer előfordult (assets/maps/*.json). Szabály: C-megjegyzésben nincs csillag-per.

Fogalmak: tervezőrács és skálázó szorzó, selective outline, vektorizálás (numpy)

## 2026-10-21 | M3, 4c. lépés: egybefüggő test metaballból
Készült: a bundás test egyetlen metaball-család (törzs, nyak, fej, pofa, lábak összeolvadva), a fehér jelölések az anyagban pozíció szerinti maszkkal (shader-csomópontok: Texture Coordinate, Separate XYZ, Math, Mix), a fogantyúk megmaradtak. Három próbakör a kontaktlapon.

Döntés: a jelölések festve vannak, nem külön testek, mert a külön gömbök vagy elnyelik a testet, vagy kilógnak; a maszk a sima felületen fut, mint a mintaképen.

Hiba 1: a fej nem olvadt össze. Gyökérok: a Blender metaball-család csak számjegyes utótagot ismer (`lili_mb.001`), a `lili_mb.head` saját család lett, a saját alapértelmezett felbontásával (0,4 m) semmit nem rajzolt. Javítás: számozott nevek.
Hiba 2: a törzs hiányzott. Gyökérok: az ELLIPSOID elem mérete nem méter, hanem a sugár körüli arány; 0,125-ös "méret" a 0,1-es sugarat 1,25 cm-re zsugorította. Javítás: arányok (1,25; 0,95; 0,85).
Hiba 3: a test vékony volt. Gyökérok: a mező képlete szerint a felület a sugár 0,575-szeresénél van, nem 0,84-nél, ahogy becsültem. Javítás: számolás a képletből, a sugarak 1,74-szeresek. Korábbi észlelés: a képletet az elején kellett volna kikeresni, nem becsülni (CLAUDE.md 6: mérj, ne becsülj).

Fogalmak: metaball (implicit felület), család és alapobjektum, pozíció szerinti anyagmaszk, küszöb és merevség

## 2026-10-21 | M3, 4d. lépés: fülek, mancsok, arc az élő Blenderben
Készült: kerekített, vastag, belül rózsaszín fülek (bmesh + Solidify + inset + Subdivision, a lógó fül Bend-del), comb-láb-mancs-lábujj a metaball-családban, kisebb arccsontok és rövid pofa, keskenyebb pofamaszk, rövidebb test; a módosítások az élő Blenderben, a `look` eszközzel nézegetve, majd a `lili_model.py`-ba visszavezetve; sprite-ok és pak újragenerálva, selftest zöld.

Döntés: az élő Blenderben formáztam, de a végén minden a szkriptbe került, és a `lili.blend` a szkriptből épül. Így a gyors, vizuális iteráció és az újraépíthetőség is megvan; az élő fájl csak biztonsági másolat.

Hiba 1: a metaball test eltűnt az élő Blenderben. Gyökérok: a család felbontása a betöltés után az alapértékre (0,4 m) állt, így semmi nem poligonizálódott. Javítás: a felbontás újra beállítva; tanulság: a `look` előtt mindig ellenőrizni a kiértékelt csúcsok számát.
Hiba 2: a fül egyetlen lap lett, teljesen rózsaszín. Gyökérok: a `bmesh.ops.inset_region` egy magányos lapon csak `use_boundary=True`-val készít gyűrűt. Javítás: a kapcsoló beállítva, a gyűrű barna, a közép rózsaszín.
Hiba 3: a pofa túl széles fehér. Gyökérok: a pofamaszk csak x és z szerint szűrt, az arccsontok pedig előrenyúltak a határ elé. Javítás: |y| < 4,6 cm feltétel a maszkban.

Fogalmak: bmesh, inset, Solidify és Subdivision Surface, Bend (Simple Deform)

## 2026-10-21 | M3, 4e. lépés: lágy árnyalás
Készült: a toon rámpák EASE átmenettel (`soft=True`), a test maszkos anyaga is; a fül hegye ívelt, a farok vége teltebb, finomabb metaball felbontás; újragenerálva, selftest zöld.

Döntés: a kemény 3 sávot lágy átmenet váltja, mert a pixelesítő úgyis a paletta 8 árnyalatára képez; a folyamatos átmenetből így finom lépcsők lesznek, a kemény sávokból viszont csak 3 szín. A mintakép lágy árnyalása pont ez.

Fogalmak: színrámpa interpolációja (CONSTANT, LINEAR, EASE), kvantálás rámpára

## 2026-10-21 | M3, 4f. lépés: hátsó lábak barnán, a fül a felénél konyul
Készült: a fehér maszk szűkítve (has közepe, mellkas, két első láb és mancs), a jobb fül hajtása a hálóba építve (a felső fél 125 fokkal előre-lefelé, lágy gerinccel); élő Blenderben beállítva, a szkriptbe visszavezetve, újragenerálva.

Hiba: a Bend modifier a fület a szélessége mentén tekerte fel. Gyökérok: a Simple Deform "limits" értéke a deformációs tengely mentén szűkít, nem a fül hossza mentén, és a hajlítás előjele is hátrafelé vitte a hegyét. Javítás: a hajtás a bmesh-ben, a csúcsokat a gerinc körül forgatva; az előjelet a kép alapján állítottam be (első próba hátra, második előre).

Fogalmak: csúcsforgatás tengely körül (Matrix.Rotation), lágy gerinc (súlyozott hajtás)

## 2026-10-23 | 2D vonal, 0. lépés: a Lili-képek élesítése
Készült: `tools/sprite_clean.py` és `assets/sprites/lili_src/poses.json`: 18 póz (10 PNG + 2 montázsból 8) 96 px-es, indexelt PNG sprite-tá, átlátszó háttérrel, Lili saját 40 színével; összevető lap: `docs/img/lili_src_compare.png`.

Döntés: nem vektorizálunk és nem AI-felskálázunk; a pixel art élességét az adja, hogy minden pixel egy rácson ül, kevés színnel és kemény széllel. A forrás valódi pixelméretét Fourier-módszerrel mértem (az élek ismétlődésének periódusa).

Hiba: az első kör színei fakók és rózsaszínesek lettek. Gyökérok: a játék mester-palettájára képeztem, aminek a bundarámpája a szobaképről mért, árnyékban kékre hajló színekből áll. Javítás: Lili saját palettája a pózokból, a 256-os paletta üres helyein. A kontúr először szürke lett, mert a JPG-montázsok széle a bézzsel keveredett; csak a valódi alfájú PNG-k szélét mérem.

Fogalmak: periódusmérés Fourier-transzformációval, elárasztásos háttérleválasztás (flood fill), indexelt PNG

## 2026-10-23 | 2D vonal, 1. lépés: animáció a PixelLabbal
Készült: járás 6 irányból (S, SW, W, N, NE, SE), ülő idle, lihegő mosoly, alvás, meghajlás; a PixelLab szerkesztőjében (Pixelorama), "Animate with Text (New)", 96 px, 8 kocka = 2 generálás. Előnézetek: `tools/pixellab_import.py` -> `docs/img/anim/pixellab_*.gif`. A pakba még semmi nem került.

Döntés: a PixelLab azért jobb a saját próbáinknál (videómodell, FLUX.2 kulcskockák), mert pixel artra tanított modell, és az egész mozgást egyben generálja a mi kockánkból, így a stílus nem csúszik el. A járásnál felálló fület a gazdi elfogadta.

Hiba: a megnyitás és a mentés a Windows fájlablakát dobta fel, amit innen nem lehet bezárni. Gyökérok: a Pixelorama a böngészőben egy rejtett `<input type=file>`-t kattint meg, az export pedig letöltést indít, amire a Chrome mentési ablakot nyit. Javítás: a kattintást elfogom, és a fájlt közvetlenül az inputba töltöm; az exportot a letöltés helyett egy helyi fogadóoldalra küldöm (postMessage, majd POST a 127.0.0.1-re), ami a repóba írja.

Fogalmak: postMessage (ablakok közti üzenet), egyidejű feladatkorlát (queue)

## 2026-10-23 | 2D vonal, 2. lépés: hátterek rétegekre bontva
Készült: `tools/bg_prepare.py`: a gazdi 5 háttere (szoba, udvar 4 évszak) a valódi 400-as rácsukon, az ajtóüvegek átlátszóak (mögöttük az évszak udvara), a tálak, a pelenka és a váza külön elemek, a padló alattuk kitöltve; előnézet árnyékkal, napfolttal, éjjeli tónussal (`docs/img/bg/`).

Döntés: az árnyék és a fény futás közben számolódik (remap tábla), nem a képen van, mert a napszakkal változik; ami nem változik (a fák árnyéka az udvaron), az maradhat a képen.

Hiba 1: a padló kitöltésénél a sorok másolása rácsmintát adott. Gyökérok: a deszkavégek vízszintes vonalai minden ismétlésnél ugyanabban a sorban jelentek meg. Javítás: oszloponként a deszka saját színe (medián), elölről hátra átmenettel.
Hiba 2: a tárgyak maszkja az egész téglalap lett. Gyökérok: a `floodfill` egy `fromarray`-jel készült képen nem módosított semmit (a kép a tömb memóriáját használta), ezért minden háttérpixel "lyuknak" számított. Javítás: `.copy()`. A váza terrakottája padlószínű (mérve 20-67 eltérés), ezért a teste kézzel rajzolt sokszöggel van kivágva.

Fogalmak: morfológiai nyitás és zárás, lyukkitöltés elárasztással, medián

## 2026-10-23 | 2D vonal, 3. lépés: a festett világ a szimulátorban
Készült: a pak képeket, háttérpalettákat és átszínező táblákat visz (`tools/bg_build.py`, `tools/pack_assets.py`, IMAGE típus); a világ ezekből rajzol (`lili_world.c` painted_*), a napszak a paletta tónusa, Lili árnyéka a saját alakjából számolódik; bemutató kör és videó (`--demo-record`).

Döntés: a fény és az árnyék 8 bites térben marad: egy maszk megmondja, hol, egy 256 bájtos tábla megmondja, melyik szín helyett melyik sötétebb vagy világosabb szín legyen. Így pixelenként egyetlen kiolvasás, a panelen is gyors, és a hátteret nem kell napszakonként külön tárolni.

Hiba: a pak eddig Lili színei nélkül ment ki (a mester-paletta 144-183 helyei nullák voltak). Gyökérok: a `sprite_clean.py` Lili színeit külön fájlba írta, a pakoló csak a mester-palettát olvasta. Javítás: a pakoló beírja Lili színeit a helyükre.

Fogalmak: palettás átszínezés (remap LUT), statikus réteg kulcsa (cache key), Y szerinti rendezés

## 2026-10-23 | 2D vonal, 4. lépés: a meghajlás harmadszor
Készült: play bow v3 a gazdi promptjával ("legs still, wiggle tail and ass like happy dog, and happy face"), 8 kocka (`assets/pixellab/play_bow_SW_v3_sheet.png`); a gazdi jóváhagyása után bekötve (`tools/pixellab_to_sprites.py`), videó: `docs/img/anim/sim_play_bow_v3.mp4` (szoba és udvar, mellette 3x nagyítás).

Döntés: új animáció csak megmutatás és jóváhagyás után kerül a pakba. Megmutatás előtt mérés: a mancsok környékének kockánkénti átfedése (a hátsó mancs legalsó sora végig a 75., az x-tartománya sem változik), így a lábemelés számmal is kiderül, nem csak szemre.

Hiba: a v2-t (a hátsó lábát emelgette) jóváhagyás nélkül építettem be. Gyökérok: a kockák átnézésekor csak az első lábakat figyeltem, és nem kérdeztem meg a gazdit. Javítás: mérés mind a négy mancsra, és kérdés a beépítés előtt.

Fogalmak: régióátfedés (IoU), forráskocka kihagyása a ciklusból

## 2026-10-23 | 2D vonal, 5. lépés: kisebb szobai Lili, tálak, demó a panelen
Készült: a szobában 72 px-es Lili (a 96 px-es kockák mesterként maradnak), a tálaknál újra a kockák saját tálja látszik, a két tál között séta; a demó a `common/lili/lili_demo.c`-ben, a pak a firmware-be linkelve, a panelre flashelve (32-33 fps).

Döntés: a pak nem külön partícióba, hanem az app képébe kerül (`target_add_binary_data`). Így nincs új partíciós tábla és külön flash-lépés, a pocket-tank modellpartíciója érintetlen marad, és a flashből a gyorsítótáron át közvetlenül olvasható. Ára: minden flasheléskor a pak is újra felmegy (1,5 MB, pár másodperc), és az app-hely 2,5 MB-ra korlátozza (most 33 % szabad).

Hiba: a tálak széthúzásakor a szoba tálját Lili elé rendeztem, ezért eltakarta a fejét. Gyökérok: a Y-rendezést a tálra javítottam, de nem néztem meg, hogy evés közben a fej a kockák tálján belül van. Javítás: a használt tál félrelép, és egy teszt ellenőrzi, hogy a tál területén Lili minden pixele látszik.

Hiba: a demó a tál melletti pontokat nem érte el (446-nál megállt 451 helyett). Gyökérok: az útkeresés 12 px-es falközt tart, a padló alja ennél közelebb van. Javítás: a falköz csak útközben számít, a célpontnak elég padlón lennie.

Fogalmak: bináris beágyazás (EMBED), XIP (flashből futás/olvasás), mesterpéldány és származtatott méretek

## 2026-10-24 | 2D vonal, 6. lépés: az éjszakai PixelLab-gyártás
Készült: a `assets/pixellab/jobs.json` mind a 143 feladata (8 irány, átmenetek, a kötél, a keksz, a tálak, a pisilés), összesen 168 sheet a próbákkal együtt; előnézetek `docs/img/anim/pixellab_*.gif`, áttekintő oldal `docs/review/index.html`, napló `docs/logs/2026-10-24_pixellab_night.md`. Semmi nincs bekötve: előbb a gazdi nézi át.

Döntés: minden a PixMiniMax modellel megy, nem a v3-mal. A PixMiniMax külön mezőben kapja a szereplőt ("a small fawn chihuahua dog") és a kiinduló pózt, így az átmeneteket (leül, feláll, lefekszik, guggol) helyesen rajzolja, a v3 ezeknél embert rajzolt a kutyára. Egy átmenet utolsó kockája új kezdőkocka lesz ("makes"), így a Gemini-lapokról hiányzó pózok (ülés W, E, SE, állás NW, guggolás SE) is megvannak.

Hiba: oldal- és hátnézetben a kutya a megriadásnál a néző felé fordult (startle W, NW, NE), a "scurries away" pedig megfordította és elszaladt vele. Gyökérok: a modell a mozdulatot a legtermészetesebb irányban rajzolja, és a "fordulj el" vagy "fuss el" szót szó szerint veszi. Javítás: a leírásban a nézet és a tartás ("seen from behind ... keeps facing away", "seen from the side ... keeps facing left", "in place, does not turn around"); az első próbák `*_try1` néven megmaradtak.

Hiba: egy-két indítás nem futott le (a panel épp átváltott, vagy "maximum number of concurrent jobs"), és a gépelés egyszer a vászonra ment billentyűparancsként. Gyökérok: a szerkesztő egy kész feladatnál magától átvált, a fókusz elvész. Javítás: két indítás egymás után, utána nincs gépelés, amíg a feladatok le nem futnak; minden indítás előtt nagyított ellenőrzés (első kocka, seed 0, a "Generate" gomb).

Fogalmak: átmenet-animáció és láncolt kezdőkocka, nézetrögzítő prompt

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

**konvex alakzat:** bármely két belső pontját összekötő szakasz is belül marad (pl. a lekerekített négyzet). Ezért ha egy téglalap mind a 4 sarka belül van, az egész téglalap belül van: 4 ellenőrzés elég a több ezer pixel helyett.

**élfüggvény (pont a háromszögben):** egy él (a -> b) és egy pont p esetén a (b - a) x (p - a) kereszt-szorzat előjele megmondja, hogy p az él melyik oldalán van. Ha a háromszög mindhárom élére ugyanaz az előjel jön, a pont bent van; így rajzolják a háromszögeket a grafikus kártyák is.

**kvantálási hiba (mérésnél):** ha a mérőeszköz csak lépcsőkben mér (itt 1 % = 10 mAh), egy rövid idő alatti változás +-1 lépéssel téved; a hiba relatív súlya a mérési idő növelésével csökken. Ezért mértünk órákat percek helyett.

**töltésmérő (fuel gauge):** a PMIC (AXP2101) becsli az akku töltöttségét feszültségből és áramintegrálásból; teli akkunál egy ideig 100 %-ot mutat (plató), és lépcsősen frissül, ezért rövid távon késik.

**gyűrűs puffer (ring buffer):** fix méretű tömb, amit körbe-körbe használunk: a `head` a legrégebbi elem, a `count` a benne lévők száma, az új elem a `(head + count) % N` helyre kerül. Nincs malloc, nincs mozgatás; ha tele van, vagy az újat dobjuk, vagy (itt) a legrégebbit.

**termelő-fogyasztó minta:** a bemenetet adók (termelők: érintés, IMU, mikrofon, gombok) egy sorba írnak, egyetlen fogyasztó (a képkocka-ciklus) üríti. Így nincs versengés az állapoton, és a bemenet forrása elkülönül a viselkedéstől.

**csíkos generálás (callback):** a kijelzőt nem egy kész képpel etetjük, hanem a kijelző-port minden 24 soros csíkhoz meghív egy általunk adott függvényt (`fill(dst, y0, n)`), ami a DMA pufferbe írja a sorokat. Közben a másik puffert már küldi a hardver (ping-pong), így a számolás és az adatküldés átlapolódik.

**vezetékbeli bájtsorrend:** az RGB565 szín 16 bit; az ESP32 kisvégű (a kisebb bájt van elöl), a kijelző viszont a nagyobb bájtot várja előre. Ezért minden pixelnél fel kell cserélni a két bájtot; mi egyszerre két pixelen, egy 32 bites műveletben tesszük.

**memoizálás (gyorsítótár):** amit sokszor ugyanúgy számolnánk ki, egyszer kiszámoljuk és eltesszük, majd csak másoljuk. Kulcs kell hozzá: a `world_bg_key` megmondja, mikor lenne más az eredmény (ma csak nézetváltáskor); ha a kulcs változik, újraszámol.

**képkocka-költségvetés:** 60 fps = 16,7 ms egy képkockára. A mérés bontása (tick, render, kitöltés, küldés) megmutatja, hova megy az idő, és hol érdemes gyorsítani: nálunk a gyorsítótár a rendert 10,5-ről 1,75 ms-ra vitte, a maradék nagy tétel a csíkok kitöltése (9,5 ms).

**ortografikus vetítés:** a kamera sugarai párhuzamosak, nincs perspektíva: ami távolabb van, nem lesz kisebb. Ezért egy tárgy ugyanakkora a szoba elején és végén, és a pixelméret mindenhol ugyanaz (1,25 cm/px). 35 fokos lenézésnél egy talajon fekvő távolság sin 35 = 0,57-szeresre, egy függőleges magasság cos 35 = 0,82-szeresre rövidül a képen.

**fejetlen (headless) futtatás:** a program ablak nélkül, parancssorból fut (`blender -b -P script.py`). Így a gyártás megismételhető: ugyanaz a parancs mindig ugyanazt a képet adja, kézi kattintgatás nélkül.

**származtatott mátrix frissítése:** a Blender az objektum helyéből és forgatásából számolja a világmátrixot (`matrix_world`), de csak a jelenet frissítésekor. Aki közben olvassa, a régi értéket kapja. Általános minta: a gyorsítótárazott, származtatott adatot frissíteni kell, mielőtt olvasunk belőle.

**emissziós toon árnyalás:** a fény erősségét (Shader to RGB) egy lépcsős színátmenet (Color Ramp, konstans) három palettaszínre képezi, és ezt fényforrásként (Emission) adjuk ki. Így a kimenet nem keveredik további fénnyel: a pixel pontosan a palettaszín.

**MCP (Model Context Protocol):** szabványos csatlakozó, amivel egy AI-asszisztens külső programot eszközként használhat. A Claude Code elindít egy kis szervert (`uvx mcp-for-blender`), ami a Claude kéréseit a Blender parancsaira fordítja. Az eszközlista a munkamenet indulásakor töltődik be.

**helyi socket (localhost port):** két program hálózati csatornán beszél, de csak a saját gépen belül (`127.0.0.1`, itt a 9876-os port). Kívülről nem érhető el; a kérés és a válasz itt JSON szöveg.

**verzió-rögzítés (pinning):** a csomagot pontos verzióval kérjük (`==2.2.2`), nem "a legújabbat". Így egy későbbi frissítés nem változtat csendben a működésen, és pontosan azt futtatjuk, amit átnéztünk.

**opt-in telemetria:** adatgyűjtés, ami csak kifejezett engedéllyel indul (alapból ki). Mi ezen felül környezeti változóval is tiltjuk, hogy egy véletlen kattintás se kapcsolja be.

**Oklab:** észlelésalapú színtér (L világosság, a és b színtengelyek): két szín távolsága benne nagyjából azt méri, amennyire a szem különbözőnek látja őket. Az RGB-ben a számtani átlag és a távolság félrevezet (a sötét színek összemosódnak, a sárga "könnyebb", mint a kék).

**k-means:** csoportosító eljárás: k középpontot keres úgy, hogy minden pixel a hozzá legközelebbi középponthoz tartozzon, és a középpontok a saját csoportjuk átlagai legyenek. Néhány kör után beáll; a nagy csoportok adják egy terület domináns színeit.

**horgony-szín:** egy kézzel megadott közelítő szín; a mért klaszterek közül azt választjuk, amelyik ehhez a legközelebb áll. Így a mérés adja a pontos értéket, de az ember mondja meg, melyik anyagról van szó.

**hue shift az árnyékban:** a festők és a pixelművészek az árnyékot nem pusztán sötétebbre, hanem hidegebbre (kékebbre), a fényt melegebbre (sárgábbra) veszik. Ettől él a kép; a tiszta sötétítés szürkés, "sáros" lesz.

**RLE (futamhossz-kódolás):** az egyforma, egymás utáni bájtokat (szám, érték) párként tároljuk; a sprite nagy része átlátszó (0) vagy egyszínű folt, ezért 5-szörösre tömörül. Dekódolás közben rögtön rajzolhatunk, nem kell kicsomagolt másolat.

**CRC-32:** 32 bites ellenőrző összeg; bármely bájt sérülése más értéket ad. A pak betöltésekor kiszámoljuk, és ha nem egyezik a fejlécben tárolttal, a játék a helyőrző grafikával indul, nem egy sérült képpel.

**tartalomjegyzék (TOC) egy bináris fájlban:** a fájl végén egy táblázat (azonosító, típus, offset, méret) minden bejegyzéshez. Az olvasó a fejlécből megtudja, hol a tábla, és onnan ugrik a kívánt adatra; nem kell végigolvasni a fájlt.

**módusz-szín a pixelesítésnél:** egy 4 x 4-es blokkból nem az átlagszín lesz a pixel (az elmosódott, köztes színt adna), hanem a leggyakoribb palettaszín. Így a kontúr éles marad, és minden pixel garantáltan a paletta egyik színe.

**Bezier-görbe bevel-lel:** a farok nem gömbökből áll, hanem egy görbe (4 vezérlőpont), amire a Blender csövet "húz" (bevel depth), a pontonkénti sugár pedig a vége felé vékonyít. Az alakja két szám átírásával hangolható.

**fogantyú (handle) a rigben:** egy üres objektum (Empty), amihez a testrészek gyermekként kapcsolódnak; az animáció csak a fogantyút forgatja vagy mozgatja, a hozzá tartozó formák követik. A helyőrző és a modell ugyanazokat a fogantyúkat adja, ezért a mozgás kódja közös.

**méretellenőrzés a cellában:** a szkript a modell befoglaló dobozát a kamerán át logikai pixelbe vetíti, és kiírja; így számmal látszik, hogy belefér-e a 48 x 48-as cellába, és hol a pivot.

**tervezőrács és skálázó szorzó:** a layout számait egy kényelmes rácson (240) írjuk, és a kód egy konstanssal (`LILI_K` = 2) viszi át a tényleges felbontásra. Egyetlen helyen változtatható, és a tesztek is ezzel szoroznak, ezért a felbontásváltás nem járt a térképek átírásával.

**selective outline:** a kontúr nem egyetlen fekete vonal, hanem a szomszédos anyag legsötétebb árnyalata: a bunda mellett sötétbarna, a fehér mellkas mellett szürkésbarna. A hi-bit pixel art egyik ismertetőjegye; a pixelesítő a szomszéd pixel rámpájából számolja.

**vektorizálás (numpy):** a pixelenkénti Python-ciklus helyett az egész képet tömbműveletekkel dolgozzuk fel (itt: egy blokk 16 pixelének gyakorisági táblája egyszerre minden blokkra). Négyszer nagyobb képen is rövidebb lett a futás.

**metaball (implicit felület):** nem háromszögekből álló felület, hanem egy mező: minden elem (gömb, kapszula, ellipszoid) a tér pontjaihoz egy értéket ad, az értékek összeadódnak, és ahol az összeg eléri a küszöböt, ott a felület. Két közeli elem mezője összefolyik, ezért a nyak sima átmenettel megy a fejbe.

**család és alapobjektum (metaball):** az azonos nevű, csak számmal különböző metaball-objektumok (`lili_mb`, `lili_mb.001`) egy mezőt alkotnak; az alapobjektum (utótag nélkül) adja a felbontást és az anyagot. Így a fej a nyak-fogantyúhoz, a lábak a csípőkhöz kötve mozoghatnak, mégis egy test marad.

**pozíció szerinti anyagmaszk:** a shader a felületi pont helyéből (a tárgy saját koordinátái) számol egy 0/1 értéket (pl. z kisebb, mint egy ferde vonal = has), és ezzel választ két színrámpa között. A jelölés így a felületen "fest", nem külön geometria.

**küszöb és merevség (metaball):** a merevség (stiffness) az elem mezőjének erőssége, a küszöb (threshold) az a mezőérték, ahol a felület megjelenik. Együtt adják, hogy a látható felület a megadott sugár hányadrészénél van: s (1 - (d/r)^2)^3 = küszöb, s = 2, küszöb = 0,6 -> d = 0,575 r.

**bmesh:** a Blender hálószerkesztő könyvtára Pythonból: csúcsokat, éleket, lapokat építünk kézzel (a fül körvonala 28 pontból), majd a kész hálót egy mesh-be írjuk.

**inset:** egy lap belsejébe egy kisebb, azonos alakú lapot vág, körülötte gyűrűvel; itt a gyűrű a fül barna széle, a közép a rózsaszín belső fül. Magányos lapon a határéleket is engedni kell (`use_boundary`).

**Solidify és Subdivision Surface:** két módosító: a Solidify a lapból vastag testet csinál (14 mm), a Subdivision minden élt lekerekít (2 szint a nézetben, 3 a renderben). Sorrendjük számít: előbb a vastagság, aztán a lekerekítés.

**Bend (Simple Deform):** a hálót egy tengely körül, adott szöggel meghajlítja; a lógó fül így nem csak el van forgatva, hanem meg is van törve, mint egy valódi, lekonyuló fül.

**színrámpa interpolációja:** a Color Ramp két pont között CONSTANT módban ugrik (kemény sáv), LINEAR módban egyenletesen, EASE módban S-görbével megy át. A lágy árnyaláshoz EASE: a fény-árnyék határ elmosódik, de a két végén tartja a palettaszínt.

**kvantálás rámpára:** a pixelesítő minden renderelt pixelhez a paletta legközelebbi színét választja; a rámpa 8 árnyalata között a folyamatos átmenet 5-8 fokozatú lépcsővé válik. Ezért a lágy render pixelesen is lágy, a kemény sáv viszont pixelesen is kemény.

**csúcsforgatás tengely körül:** egy ponthalmaz hajlítása úgy, hogy a kiválasztott csúcsokat egy tengely (itt a fül keresztirányú gerince) körül forgatjuk egy forgatási mátrixszal; a gerinc alatti csúcsok a helyükön maradnak.

**lágy gerinc (súlyozott hajtás):** a forgatás szöge a gerinctől távolodva 0-ról teljesre nő (itt negyed fülmagasságon át), így nem éles törés, hanem ívelt hajlat lesz; a Subdivision tovább simítja.

**periódusmérés Fourier-transzformációval:** a kép oszloponkénti él-erősségét egy jelnek tekintjük; ha a rajz pixelei p képpontonként ismétlődnek, a jel 1/p frekvencián erős. A legerősebb frekvencia adja a pixelméretet (itt 1,17 és 4,27), a fázisa a rács eltolását.

**elárasztásos háttérleválasztás (flood fill):** a kép széléről indulva, szomszédról szomszédra terjedve jelöljük háttérnek a háttérszínhez hasonló pixeleket. A kutya sötét kontúrja megállítja, ezért a világos mellkas akkor sem tűnik el, ha a színe közel van a háttérhez.

**indexelt PNG:** a pixel nem színt, hanem egy palettaindexet tárol (mint a játék 8 bites vászna); a 0-s index átlátszó. Így a sprite pontosan a játék palettájának színeit használja.

**régióátfedés (IoU, intersection over union):** két kép ugyanazon területén a nem átlátszó pixelek metszete osztva az uniójukkal; 1,0 = pontosan ugyanott van minden pixel. A mancsok területére kockánként kiszámolva megmutatja, mozdul-e a láb (a meghajlásnál a hátsó mancsoké 0,94-0,99 maradt: csak a test széle rezdül).

**forráskocka kihagyása:** a PixelLab sheet elején kétszer ott a kiinduló póz, a generált kockák ehhez képest kicsit elcsúsznak (itt 2 px-szel feljebb van a talp). Ciklusban ez egy ugrás lenne körönként, ezért csak a generált 8 kocka megy körbe; az utolsó és az első generált szinte egyforma, így a kör folytonos.

**bináris beágyazás (target_add_binary_data):** az ESP-IDF a megadott fájlt a firmware írásvédett adatai közé linkeli, és két szimbólumot ad hozzá (`_binary_assets_pak_start`, `_end`). A program úgy látja, mint egy közönséges tömböt a memóriában.

**XIP (execute in place):** az ESP32-S3 a külső flash-t egy gyorsítótáron át a címtérbe képezi le, így a kód és az adat onnan közvetlenül olvasható, nem kell RAM-ba másolni. A pak 1,5 MB-ja ezért nem foglal RAM-ot; ami gyakran kell (a háttér), azt a PSRAM-os statikus réteg tárolja.

**mesterpéldány és származtatott méretek:** a legnagyobb, minőségi változat (itt a 96 px-es kocka) a forrás, a kisebbek (72 és 60 px) mindig ebből készülnek újra. Így egy méretváltás csak egy szám átírása, és nem romlik a minőség az ismételt kicsinyítéstől.

**átmenet-animáció és láncolt kezdőkocka:** az átmenet egyik pózból a másikba visz (állásból ülésbe); az utolsó kockája pontosan a célpóz, ezért kezdőkockaként továbbadható a következő animációnak (ülésből lihegés). Így egy hiányzó póz rajzolás nélkül előáll, és a két animáció a határon nem ugrik.

**nézetrögzítő prompt:** a szöveges animációs modell a mozdulathoz magától választ nézetet (a megriadt kutya a néző felé fordul). Ha a leírás kimondja a nézetet és a tartást ("seen from behind ... keeps facing away"), a test a kezdőkocka irányában marad, és a nyolc irány egymás mellé tehető.
