# Pixel Lili: projektösszefoglaló

Egy kis, érintőképernyős, zsebben hordható virtuális kutya (Tamagotchi jellegű) emlékül Lilinek, a nemrég elvesztett chihuahuámnak. A kutya viselkedését egy pici, helyben futó nyelvi modell (LLM) irányítja, internet nélkül.

## 1. Inspiráció

- **Pocket Tank** (StratoBuilds): virtuális akvárium ESP32-S3-on, ahol egy kb. 14 millió paraméteres, desztillált LLM dönti el a halak céljait (enni, pihenni, felfedezni, menekülni stb.).
- GitHub: https://github.com/mediacutlet/pocket-tank (MIT licenc)
- Felépítés: minden lény időnként egy sorban leírja a helyzetét (éhség, energia, félelem, ki van közel, személyiség), a modell kiegészíti egy céllal és sürgősséggel. Egy "reflex" réteg (sima C++) alakítja a célt mozgássá 25-30 fps-sel. A modell a második CPU magon fut, a súlyok a flash memóriából olvasódnak.

## 2. Hardver (megrendelve)

- **Waveshare ESP32-S3-Touch-AMOLED-2.16**, akkus változat
  - ESP32-S3R8, kétmagos, 240 MHz, 8 MB PSRAM, 16 MB flash
  - 2.16" AMOLED érintőkijelző, 480×480 pixel
  - Kijelzővezérlő: CO5300 (QSPI), érintésvezérlő: CST9220 (I2C)
  - QMI8658 6 tengelyes mozgásérzékelő, PCF85063 RTC óra, AXP2101 tápellátás, hangkodek, visszhangszűrős mikrofon, TF kártyahely, programozható gomb
- **Fontos:** a Pocket Tank az 1.8"-os panelre készült, ezért a kijelző és érintés inicializálását, valamint a felbontást (368×448 helyett 480×480) át kell írni. A Waveshare wiki példakódjai segítenek.
- Szükséges még: USB-C kábel, ami adatot is visz.

## 3. Fejlesztői gép

- NVIDIA RTX 5060, 8 GB VRAM: a kis modell tanításához bőven elég.
- Blackwell architektúra miatt friss PyTorch kell CUDA 12.8 vagy újabb builddel.
- A nagy "tanár" modell nem fut helyben: a tanító adatot API-n (Claude/Gemini) vagy egy kis helyi modellel (Ollama, 4-8B) generáljuk.

## 4. Lili: kinézet (a pixel art alapja)

- Kis termetű chihuahua, krémes, világos vörösesszőke bunda, mellkas, has, mancsok fehérek
- Fehér csík (blaze) a homlokán a két szeme között
- **Az egyik füle felálló, a másik gyakran félig lekonyul**
- Nagy, kerek, sötét szemek, amelyek **kicsit szétfelé állnak**
- Az orra rózsaszínes-barna, **világosra kopott**
- Felkunkorodó farok
- Jókedvében lihegett, és ettől úgy nézett ki, mintha mosolyogna (látszott a nyelve)
- Kölyökként sokáig szőrtelen volt a hasa

## 5. Lili: személyiség (a viselkedés és az LLM tanító adat alapja)

- Nagyon pörgős, mindig boldog, folyton csóválta a farkát
- Mindenhová jött a gazdája után, odaült a lábához
- Amikor a gazdi hazaért, hozta a kis fonott, csomózott, színes kötéljátékát vagy az épp aktuális pici plüssét, és előtte rágcsálta
- Incselkedett: a fejét a földhöz nyomta, a popsiját az égnek tartotta (játékra hívó pózban)
- Ha kellett, nagyon tudott ugatni: ugatáskor előre nézett, majd a fejét teljesen függőlegesbe felcsapta
- Puffogott is
- Rosszalkodott: beszökött a szobákba, és az ágyon aludt, ha nem volt otthon senki
- Néha nem a pisilőhelyre pisilt, és ha rossz kedve volt, több helyre kis adagokban "rosszalkodott". A gazdi ezen mindig csak nevetett
- Kedves volt az udvaron élő többi kutyával is
- Szeretett az ölbe ülni és simogatáskor belemászni a gazdiba
- Becenevek: **"kis csórehasú"** (szőrtelen kölyökhasa miatt), **"kis kattosom"** (mert nagyon kattos és játékos volt)
- Testvére/társa: **Bella** (pirosabb-barna testű, fehér arcú, fekete orrú chihuahua). Később ő is bekerülhet második kutyaként

## 6. Funkciók terve

- **Érintés:** simogatás (húzott ujjra behunyja a szemét), labdadobás, etetés, játék odaadása
- **Mozgásérzékelő:** rázásra felriad, döntésre gurul a labda és utána fut
- **Óra:** nappal-éjszaka ciklus, este elalszik, születésnapon különleges esemény
- **Mikrofon (később):** hangra felkapja a fejét és odaszalad
- **Nincs elpusztulás:** ha elhanyagolják, legfeljebb szomorú és álmos, de mindig ott marad
- **Lili-specifikus pillanatok:** hozza a fonott játékot "amikor hazaérsz" (bekapcsoláskor), popsi az égnek póz, felcsapott fejű ugatás, ágyra szökés, néha egy apró kaki kupac, amit egy érintéssel fel lehet takarítani

## 7. LLM célok (első lista, a Pocket Tank mintájára)

Állapotok: éhség, energia, kedv, unalom, magány, simogatási igény.
Célok: `seek_food`, `follow_owner`, `bring_toy`, `chew_toy`, `play_bow` (popsi az égnek), `bark`, `ask_pet`, `sleep`, `sneak_to_bed`, `explore`, `zoomies`.
Példa bemenet: `hunger 3 energy 8 happy 9 bored 6 owner near toy mid bed far` → kimenet: `bring_toy 7`

## 8. Sprite-ok és animációk

- Méret: 40×40 vagy 48×48 pixel, a kijelzőn 6-10× nagyítva, 8-12 színes paletta
- Szükséges animációk: ül/pislog, farokcsóválás, lihegős mosoly, séta (4-6 kocka), futás, alvás (lélegzés), ugatás felcsapott fejjel, popsi az égnek, játék rágcsálása, simogatás (behunyt szem), evés
- **Az első verzió kódból készült** (Python + Pillow, pixelenként leírva): farokcsóválás, lihegés, pislogás. Ennek előnye, hogy minden kocka tökéletesen egyforma stílusú, és a változtatások szövegesen kérhetők
- Lehetséges AI eszköz: **PixelLab** (pixellab.ai), amely karaktereket és animációkat generál, és van MCP szervere is (https://api.pixellab.ai/mcp, API tokennel). Ez Claude Code-ból vagy más MCP-képes kliensből használható
- Gemini/képgeneráló AI jó stílusreferenciának, de egymáshoz illő animációs kockákat ritkán ad, ezért a végleges kockák kódból vagy PixelLabbal készülnek

## 8b. Új irány: 2.5D pixel világ (döntés)

- A cél: Lili szabadon járkál egy kis szobában térhatással (3/4-es felülnézet), odajön közel, fejet fordít és csodálkozik, elszalad enni és visszajön, elhozza a játékát, popsi az égnek pózban rágcsálja, elszalad vele ha az ujj közelít, a játékot egy suhintással el lehet dobni neki.
- Módszer: **3D modellből előre renderelt pixel sprite-ok** (mint a klasszikus játékokban). Lili 3D modellje Blenderben készül, minden animáció 8 irányból, kis felbontásban renderelődik, majd palettaredukció és körvonal. Az ESP32 csak sprite-okat rajzol mélység szerint rendezve, így folyamatos marad a 25-30 fps.
- Valódi 3D az ESP32-n nem reális (nincs GPU), ez a módszer adja ugyanazt az érzést.
- Eszközök, mind ingyenes: Blender (fejlesztői gépen futtatható, a Claude felhőben headless is futtatja), opcionálisan blender-mcp (nyílt forrású, Blender vezérlése AI-ból), Python + Pillow utófeldolgozás.
- Állapot: működő próba megvan (primitívekből épített Lili, 8 irányú séta, fejfordítás).
- Gemini helyett a projekt Claude Code-ban folytatódik.

## 9. Lépések

1. **Beüzemelés:** Waveshare példakód, kép és érintés működjön a 2.16"-on (ESP-IDF vagy Arduino + LVGL)
2. **Sprite-ok:** Lili animációinak elkészítése, sprite sheet, átalakítás C tömbbé
3. **Egyszerű agy:** szabályalapú viselkedés LLM nélkül (állapotgép)
4. **Interakciók:** érintés, mozgásérzékelő, nappal-éjszaka
5. **LLM agy:** Lili személyiségéből tanító adat generálása, kis modell tanítása az RTX 5060-on, csere a szabályokra
6. **Csiszolás:** hangok, ház, apró részletek, később Bella

## 10. Szabályok a munkához

- Ha bármilyen adatbázis vagy SQL fájl kerül a projektbe, azt csak olvasni szabad, módosítani csak külön engedéllyel.
