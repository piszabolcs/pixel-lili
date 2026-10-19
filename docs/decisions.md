# Döntésnapló (D-xx, a spec 15. fejezete)

| ID | Döntés | Dátum | Megjegyzés |
|---|---|---|---|
| D-01 | Fejlesztői környezet: Windows 11 + WSL2 Ubuntu 24.04 (tükrözött hálózat), a WSL lemez a D:\WSL-en; a Claude Code a Windows-appból vezérli a WSL-t | 2026-10-18 | a gazdi döntése |
| D-03 | Lili **saját jobb** füle konyul le, a bal áll (szemből nézve a néző bal oldalán a lekonyuló) | 2026-10-18 | a gazdi megerősítette a fotók alapján |
| panel | A 2.16"-os panel (CO5300, 480x480, akkus) érkezett meg | 2026-10-18 | a gazdi megerősítette |
| repó | GitHub nem enged privát forkot publikus repóból, ezért új privát repó `pixel-lili`, a pocket-tank `upstream` remote-ként | 2026-10-18 | |
| repó-2 | GitHub elhagyva, a repó csak helyben él a `Desktop\Lili_project\pixel-lili` mappában (a gazdi döntése a gh-bejelentkezési körök után) | 2026-10-18 | a WSL csak fordít |
| M1.7 / CORNER_R | **CORNER_R = 72 fizikai px (36 logikai px)**. A sarokteszten (32..72 px, 8 px lépés) a 64 px-es ív levágódik, a 72 px-es teljesen látszik; a 4 egyenes szélen a 0. és 479. pixel is látszik. A spec 5.2 kiinduló 40 px-e (a pocket-tank "a gyártó képei alapján") és az 1. fejezet auditjának 5. sora ezzel megdől: a korábbi 76 px-es becslés közelebb volt. Ellenőrizve: a spec 8.2 sarokhoz legközelebbi pontja, a `bowl_water` (198,196) logikai, 26 logikai px-re van az ív belső pontjától (kell >= 24). | 2026-10-19 | fotó + diag sarokteszt; M2-ben a pocket-tank RENDER_CARD 26/22 px-es behúzása is újraszámolandó |
