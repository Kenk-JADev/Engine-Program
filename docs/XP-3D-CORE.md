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
