# Pixel Lili: szabályok a Claude Code-nak

Ez a projekt egy ESP32-S3 (Waveshare ESP32-S3-Touch-AMOLED-2.16) alapú virtuális kutya, Lili emlékére, a mediacutlet/pocket-tank forkjaként.

## Az egyetlen igazságforrás
A teljes terv a `PIXEL_LILI_SPEC.md` fájlban van. Munka előtt olvasd el a releváns fejezeteket. Ha bármi ellentmond régebbi jegyzeteknek, a spec érvényes.

## Kötelező szabályok
1. **Adatbázis-szabály:** bármilyen adatbázis vagy SQL fájl KIZÁRÓLAG olvasható. Módosítani, írni csak akkor szabad, ha a felhasználó az adott beszélgetésben külön, kifejezetten engedélyt ad rá, akkor is, ha az engedélyezési mód automatikus.
2. **Mérföldkövenként dolgozz** (spec 12. fejezet). Minden feladat végén mutasd meg a "Kész, ha" feltétel teljesülését.
3. **Csak zöld állapotot commitolj:** `make check` (sim) és a firmware build sikeres.
4. **A `common/` mappa platformfüggetlen:** változtatás nélkül fordul a szimulátorba és a firmware-be.
5. **A Core 0 soha nem vár az agyra.** A képkocka-ciklusban nincs `malloc`, nincs blokkoló hívás.
6. **Mérj, ne becsülj:** minden teljesítményszám a `docs/stats.md`-be kerül, dátummal.
7. **Ingyenes eszközök:** fizetős API vagy szolgáltatás nem használható.
8. **Lili sprite-jai nem tükrözhetők** (aszimmetrikus fül).
9. **A séma egyetlen forrása** a `model/schema_lili.json`; a `vocab.h` és a tokenizer ebből generálódik, kézzel nem szerkeszthetők.
10. **Nyitott döntésnél** (spec 15. fejezet) kérdezz, vagy használd az alapértelmezést, és írd be a `docs/decisions.md`-be.
11. A pocket-tank MIT licencét és forrásmegjelölését meg kell tartani. A repó privát.

## Munkamenet végén
- Frissítsd a `docs/STATUS.md`-t (kész feladatok, következő lépés, a gazdinak szóló "próbáld ki" lista).
- Ha a pocket-tankból átvettél vagy módosítottál fájlt, jegyezd a `docs/UPSTREAM.md`-be.
