# XP-Kernmechanik in 3D (PAKET 29)

Die Engine bildet das **dokumentierte Verhalten** von RPG Maker XP nach
(Routen-Codes 1–45, Tempo-Stufen 1–6, Häufigkeiten 1–6, Kollisionsregeln),
implementiert es aber **neu als zeitgesteuerte 3D-Mechanik**. Dieses
Dokument listet bekannte XP-Schwächen und wie die Engine sie behebt.

## 1. XP-Schwächen → Engine-Fixes

| XP-Verhalten (2004) | Warum es störte | Engine (PAKET 29) |
|---|---|---|
| Bewegung in **Frames @ 40 fps** gezählt | Tempo hing an Bildrate & Last | Alles **dt/Sekunden-basiert** (`CharacterMotion`), framerate-unabhängig |
| Events „hüpften“ **instant Kachel→Kachel** | ruckelig, kein Fluss | **Interpolierte Kachelschritte**, Dauer = XP-Tempo-Tabelle |
| **Sprung** war eine flache 2D-Sprite-Parabel | keine echte Höhe | Sprung läuft auf **echter Y-Achse** (Parabel, distanzskaliert) |
| **Diagonalen 5–8** fehlten in vielen Tools/Menüs | „RPG Maker ist Vier-Richtungs-Spiel“ | Codes 5–8 + Tokens `DL DR UL UR`, XP-„Ecke-schneiden“-Regel |
| „**Annähern**“ als Greedy-Algorithmus | NPC blieb an **jeder Ecke/Wand** hängen | **Begrenzte Breitensuche** (Radius 8) + Greedy-Fallback |
| Blickrichtung oft hart & regellos | Charakter flackert | XP-Richtungsregeln: Diagonal bevorzugt horizontale Achse; Turn auch bei Block |
| Kamera **kachelzentriert & hart** | Sprünge am Bildrand | **Weiche Follow-Cam** (exp. Dämpfung) + Snap bei Map-Transfer |
| 3 Ebenen, keine Höhe | flache Maps | Beliebig viele Layer + Elevation (Bestand, `Map`) |
| 640×480 fest | Pixelanmutung | Skalierbares Fenster/Ansicht (Bestand, `Renderer`) |
| Dash = unklarer Faktor | unvorhersehbar | Dash = **+1 Geschwindigkeitsstufe** (genau ×2,0 über Stufe 4) |

## 2. Geschwindigkeits-/Häufigkeitstabellen

Zeitbezug: klassisches 40-Frames-Raster, in Sekunden übersetzt
(`include/rpgmaker3d/CharacterMotion.h`, Namensraum `xp`).

| Stufe | s/Kachel | Kachel/s | Frequenz-Pause |
|---|---|---|---|
| 1 (langsamste) | 1,60 | 0,62 | 1,50 s |
| 2 | 0,80 | 1,25 | 1,25 s |
| 3 | 0,40 | 2,50 | 1,00 s |
| 4 (normal/Spieler) | 0,20 | 5,00 | 0,75 s |
| 5 (sprintet) | 0,10 | 10,00 | 0,50 s |
| 6 (schnellste) | 0,05 | 20,00 | 0,25 s |

Spieler-Default: Stufe 4 (5,0 Kacheln/s), Shift-Dash = Stufe 5 (10,0).

## 3. Bewegungsablauf (Events)

```
Route/Autonom entscheidet nächsten Schritt
        │
        ▼
Kollision (Kachel, Events, Spieler, Rand; Diagonal: + 2 Zwischenzellen)
        │ frei
        ▼
logische Zielzelle sofort belegen (ev.x / ev.z)      ← Kollision bleibt
        │                                               kachelgenau
        ▼
CharacterMotion.BeginStep/BeginJump (Dauer = Tempo-Tabelle)
        │
        ▼
worldPos interpoliert; Landung → exaktes Snap → postPause
        │
        ▼
nächster Routen-Schritt (ein Schritt pro Frame-Takt, wie XP)
```

- **Seitenwechsel/Teleport (202)** beenden einen laufenden Schritt sofort.
- **Route-Pause** (`V(n)`-Tempo) zählt ab **Landung**, nicht ab Start.
- **Sprung** setzt nur das ZIEL frei voraus (Durchlässig: überall landen).

## 4. Autonome Bewegung

- **Zufällig**: 4-Richtungs-Schritt nach Frequenz-Tabelle.
- **Annähern**: BFS-Wegfindung (Radius 8 Kacheln, ≤1024 Zellen) — der NPC
  umgeht Mauern; Greedy-Achsenwahl bleibt der Fallback ohne Weg.
- **Fest**: keine Bewegung (wie bisher).

## 5. Demo im SampleProject

`Map001_events.json`, Event **„Maschinistin“** (PAKET 29): Custom-Route
`DR DR J(2,0) UL UL J(-2,0) W20` zeigt Diagonalen + animierte Sprünge.

## 6. Ausblick (nicht Teil dieses Pakets)

- Anime-Flags 31–34 / Opacity 40 / Blend 41 (Route-Restcodes)
- Hängen-Diagonalen für den **Spieler** (freie 360°-Bewegung ist bestand)
- Terrain-Abhängige Schrittsounds

---

# 3D-Modelle mit Keyframe-Morph-Animation (PAKET 46, Etappe 3 Stufe 1)

Bisher waren 3D-Modelle statisch (nur Einzel-OBJ). Das `.anim`-Manifest
(INI-Stil wie die Game.ini) macht OBJ-Frame-Serien zur laufenden Animation –
Positions- und Normalen-Lerp auf der CPU, Upload in einen dynamischen VBO
pro Frame. RPG-taugliche Vertexzahlen vorausgesetzt, ist das bewusst simpel
(kein Skelett, kein glTF – saubere Stufe mit ehrlicher Grenze unten).

## Manifest-Format

`<ordner>/figur.anim` neben den Frame-OBJs:

```ini
[frames]                  ; zeilenweise – Reihenfolge = Frame-Index
f0 = ritter_idle0.obj
f1 = ritter_idle1.obj
f2 = ritter_walk0.obj
f3 = ritter_walk1.obj

[clip:idle]
frames = 0,1              ; Indexliste (Ping-Pong direkt: 0,1,0)
fps    = 3                ; Schritte/Sekunde (0.1..120)
loop   = 1                ; 0 = am letzten Frame stehen bleiben
start  = 1                ; beim Laden automatisch (immer explizit setzen)

[clip:walk]
frames = 2,3
fps    = 8
```

- **Topologie-Regel:** alle Frames gleiche Vertex-/Indexzahl UND gleiche
  Vertex-Reihenfolge (Morph paart Index i mit i). So exportieren, wie es
  Morph-/Shape-Key-Exports in Blender & Co. erzeugen.
- **Lade-Regeln:** `Model::LoadAnyModelFile` (ResourceManager + Editor-
  Import) routet per Endung `.anim`; `LoadFromOBJ` bleibt für Einzeldateien.
  Fehler (fehlende Datei, Topologie-Bruch) verweigern das Manifest mit
  Log-Zeile, NIE Teilladung. Der OBJ-Parser wurde dabei gehaertet:
  Facetten mit ungueltigen Indexverweisen sind jetzt Fehler statt UB.
- **Laufzeit (PAKET 47 korrigiert Stufe 1):** das Model ist die reine
  **Vorlage** (Frames+Clips, statisch auf Frame 0). `Scene::Update` legt
  je Entitaet einen eigenen Clip-Stand + eigene dynamische Renderpuffer an
  (`ModelRendererComponent::animClip/animTime/instanceMeshes`) — Entitaeten
  mit demselben Model laufen ab jetzt **unabhaengig voneinander**
  (Autostart per `start=1` je Instanz, nicht mehr am Template).
  Nicht-loopende Clips bleiben auf dem Endframe stehen; bei Clip-Start
  springt die Pose sofort auf den ersten Clip-Frame.
- **Instanz-Steuerung aus Ruby (PAKET 47):**
  `actor.set_model_file("assets/models/pillar_pulse.anim")` (bisher nur
  `set_model("cube"/"plane")` — echte Dateien gingen gar nicht aus Skripten;
  Kandidatenpfade: wie angegeben, `<Projekt>/…`, `assets/models/…`,
  `assets/…`), `actor.play_clip("pulse"[, neustart])`, `actor.stop_clip`.
  C++-Seite: `Scene::StartEntityClip/StopEntityClip`.
- **Demo:** `SampleProject/assets/models/pillar_pulse.anim` (+2 OBJs) –
  im Editor als Modell importieren, der Loop laeuft sofort (start=1), pro
  Entitaet unabhaengig; rein aus Skript: `a = Actor.new("p");
  a.set_model_file("pillar_pulse.anim"); a.move_to(...)`.
- **Nebenfix (PAKET 46):** der OBJ-Parser verweigert Facetten mit
  ungueltigen Indexverweisen jetzt sauber (vorher stille UB-Gefahr).
- **Event-Steuerung (PAKET 48, Stufe 3):** statt Skript-Umweg direkt als
  Event-Befehl – **508 „Objekt-Clip abspielen“** (Ziel-Objekt-ID,
  Clip-Name, Neustart-Flag) und **509 „Objekt-Clip stoppen“**, beide im
  Qt-Event-Katalog (Seite 3) mit eigenem Bearbeitungs-Dialog. Intern laeuft
  es ueber `EventSystem_SetClipRunners` -> `Scene::StartEntityClip/
  StopEntityClip`, also exakt dieselbe Instanz-Clipmaschine wie aus Ruby.

## Verbleibende Grenzen (bewusst, im Code vermerkt)

- Zwischenbilder sind linear auf der CPU (Morph); UV kommen aus Frame A;
  Multi-Mesh-OBJs bleiben Einzel-Mesh (Engine-Loader-Regel).
- Pro animierter Entitaet ein eigener dynamischer VBO (RPG-Mengen ok;
  tausende Instanzen sind nicht das Ziel).

Ausblick: Bewertung von glTF-Skinning (großer Block, eigener Entscheid) —
der Event-Befehl „Objekt-Clip" (508/509) ist seit PAKET 48 umgesetzt.
