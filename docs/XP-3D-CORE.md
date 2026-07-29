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
- **Laufzeit:** `Scene::Update` tickt alle Modelle mit Manifest
  (`IsAnimated()`), autonom per `start=1` oder spaeter per API
  (`PlayClip/StopClip`, C++-Seite fertig). Nicht-loopende Clips bleiben auf
  dem Endframe stehen; `PlayClip` springt sofort auf Frame 1.
- **Demo:** `SampleProject/assets/models/pillar_pulse.anim` (+2 OBJs) –
  im Editor als Modell importieren, der Loop laeuft sofort (start=1).

## Stufe-1-Grenzen (bewusst, dokumentiert im Code)

- **Geteilte Pose:** mehrere Entitaeten mit demselben `shared_ptr<Model>`
  bewegen sich synchron (ein Morph-Puffer pro Model). Instanz-Posen =
  Stufe 2 (eigene Puffer je Entitaet).
- Morph ist linear+CPU; UV kommen aus Frame A; Multi-Mesh-OBJs bleiben
  Einzel-Mesh (Engine-Loader-Regel).
- Clip-Wechsel aus Events/Ruby folgt mit Stufe 2 zusammen (API existiert
  bereits: `PlayClip("name")`).

Perspektive Stufe 2+: Instanz-Pose je Entitaet, Clip-Aufruf aus Event/
Skript, danach Bewertung von glTF-Skinning (großer Block, eigener Entscheid).
