# Látványterv-promptok Geminihez (M3 előtt)

Cél: mielőtt a Blenderben bármi készül, legyen 3-6 kép, amire a gazdi azt mondja, "ilyen legyen". Ezekhez mérjük utána a modellt, a palettát, a fényt és a részletességet. A kép nem a végleges grafika, hanem **célkép (style target)**.

## 1. Hogy hívják ezt a stílust

1. **Modern pixel art**, angolul gyakran **"hi-bit pixel art"** vagy "HD pixel art": a 16 bites korszak (SNES, Chrono Trigger) szerkezete, mai eszközökkel. Sok szín, finom árnyalás, dinamikus fények, részletes, festői hatású csempék.
2. A nézet: **top-down 3/4 view** (döntött felülnézet, angolul "oblique top-down" vagy "three-quarter top-down").
3. Jellemzői a képen (Sea of Stars, Sabotage Studio): telített, meleg fő színek hideg árnyékokkal; erős fény-árnyék (rim light, fénypöttyök, részecskék); a sprite-ok kontúrja nem fekete, hanem az anyag sötét árnyalata (**selective outline**, "sel-out"); dús, rétegzett növényzet; tiszta pixelrács.
4. Nem ugyanaz, mint a **HD-2D** (Octopath Traveler): ott pixeles figurák állnak egy 3D világban mélységélességgel. Mi a tiszta 2D-t akarjuk.

## 2. Hogyan használd

1. Töltsd fel a Gemininek referenciaként: (a) egy Sea of Stars képet **csak stílusnak**, (b) 2-3 fotót Liliről (arc szemből, oldalról, ahol a fülei látszanak), (c) a szőnyeg fotóját (`reference/photos/rug_owner_room_2026-10-19.jpg`).
2. Az angol promptot másold be egy az egyben. A képmodellek angolul pontosabbak.
3. Egyszerre egy dolgot változtass (pl. csak a fényt, vagy csak a kutya méretét), különben nem derül ki, mi tetszik.
4. A Gemini pixel artja "hamis": a pixelek nem egyformák, néha elmosódik. Ezt ne bánd, a **stílust, a színeket, a hangulatot és Lili karakterét** nézzük rajta, a pontos pixelrácsot mi csináljuk.
5. A kedvenceket mentsd ide: `reference/concept/` (pl. `room_01.png`), és írd meg, mi tetszik bennük és mi nem.

## 3. Promptok

### A) A szoba (a fő célkép)

```
Modern hi-bit pixel art game scene, top-down three-quarter view (camera tilted about 35 degrees), in the visual style of the reference screenshot (rich saturated warm colours, cool shadows, soft dynamic lighting, selective outlines, crisp pixel grid, detailed painterly tiles). Square 1:1 image, it is shown on a small square screen with rounded corners, so keep every important object away from the four corners.

The scene: a cosy small Hungarian room seen from above at an angle. The back wall runs horizontally across the top quarter of the image with an open door in its middle; warm afternoon sunlight falls through the door onto the wooden floor as a soft beam. On the left side lies a rectangular rug (3:2, its long side running front to back) with a geometric triangle pattern in sage green, off-white, light grey and dark charcoal grey (see the rug photo). Along the right wall stands an L-shaped wooden kitchen corner bench with a shadowy nook under the seat. Bottom left: a white puppy pad on the floor. Bottom right: two small bowls side by side, food and water. Bottom middle: a small seasonal decoration (a vase of spring flowers).

In the middle of the room: a tiny chihuahua puppy (see the dog photos) standing on the floor, about one eighth of the image wide, clearly bigger in proportion than real life compared to the furniture, cute but natural, not a ball or blob.

Mood: warm, gentle, nostalgic, a little magical, like a loving memory. No text, no UI, no watermark, no characters other than the dog. Avoid: Pokemon or Game Boy look, chibi blob shapes, flat vector art, 3D render, blurry upscaled pixels, black outlines everywhere.
```

### B) Lili karakterlap

```
Modern hi-bit pixel art character sheet of one small chihuahua puppy, in the style of the reference screenshot (selective outlines in darker shades of the fur colour, soft 3-4 tone shading, light from the upper left, crisp pixel grid). Transparent or plain light background, no text.

The dog, based on the photos: tiny chihuahua with puppy-like proportions; creamy light reddish-blonde fur; white chest, belly and paws; a white blaze stripe on the forehead between the eyes; big, round, dark eyes set slightly apart; a pinkish-brown, worn light nose; tail curled high over the back. IMPORTANT ASYMMETRY: her own LEFT ear stands straight up, her own RIGHT ear is half folded and droops (when she faces the viewer, the drooping ear is on the viewer's LEFT side). Never mirror this.

Show her in eight directions in a row (front, front-left, left, back-left, back, back-right, right, front-right), standing, each about 40 by 30 pixels, all in the same scale and lighting. Below that, a second row: sitting, walking mid-step, lying curled asleep, and happy panting with the tongue out and a smiling face.
```

### C) Az udvar

```
Modern hi-bit pixel art game scene, same style, camera and scale as the room image: top-down three-quarter view, square 1:1, nothing important in the four corners. A small Hungarian garden behind the house: lush grass with flower patches, two big trees whose canopies can cover the dog when she walks behind them, three round bushes, a flower bed, a wooden fence or hedge along the top edge, and the house wall with its door along the bottom edge. The same tiny chihuahua puppy plays in the middle with a colourful knotted rope toy. Late spring afternoon light, a few butterflies. No text, no UI. Avoid Pokemon look, blobs, 3D render, blurry pixels.
```

### D) Napszak és évszak (az elfogadott szoba- vagy udvarképre)

Töltsd vissza a kedvenc képet, és írd:

```
Keep this exact scene, layout, camera and pixel art style. Change only the time and season: [ winter evening: dark blue window light, a warm yellow table lamp glow pooling on the floor, a string of Christmas lights blinking above the corner bench, snow visible through the open door ]
```

További változatok a szögletes zárójelbe: `autumn morning: low golden light, a pumpkin as the decoration, falling leaves outside`, `summer noon: strong bright light, sharp short shadows, a watermelon slice decoration`, `night: deep blue darkness, moonlight through the door, the dog asleep curled on the rug`.

## 4. Mi lesz ezután

1. A jóváhagyott képekből készül a mester-paletta (M3.2), a stílus pontos leírása (kontúr, árnyalatszám, fény), és a spec 7.1 frissítése.
2. A Blender azt adja, amiben jó: arányok, 8 irány, mozgás és időzítés, konzisztens fény. A Sea of Stars szintű részletességhez a renderelt kockákat kézzel tisztítjuk pixelszerkesztőben (ingyenes: Pixelorama vagy LibreSprite), a `sprites/overrides/` mappába (spec 7.5, B7 5. pont).
