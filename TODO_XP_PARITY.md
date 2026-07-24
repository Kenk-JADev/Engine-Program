# TODO – RPG Maker XP-Parität (Arbeitsliste)

> **Zweck:** Diese Liste ist der verbindliche Arbeitsplan, bis Engine + Editor
> funktional dem RPG Maker XP (RGSS 1) entsprechen. Jedes Paket ist so
> beschrieben, dass es ohne erneute Code-Analyse umgesetzt werden kann.
> **Pflege:** Erledigte Pakete mit Datum + Commit-SHA abhaken. Neue Lücken
> unten anhängen. NICHTS löschen, was noch offen ist.
>
> Stand der Analyse: 2026-07-23, HEAD `404e9ef` (Branch `arena/019f6f2a-engine-program`).

---

## Bereits fertig (nicht mehr anfassen, nur regressionsprüfen)

- **RGSS-Kerntab:** Rect, Color, Tone, Font (+`default_*`), Table, Bitmap
  (blt/stretch_blt/fill_rect/gradient/draw_text/text_size/hue_change/blur...),
  Viewport, Sprite, Plane, Tilemap (48er-Autotile-Muster, flash_data,
  Prioritäten), Window-Vollset, Graphics (freeze/transition crossfade+Maske),
  Input XP, Audio XP, RPG::Cache, RPG::Sprite, RPG::Weather — ab `404e9ef`.
  Bewusste Näherungen im README dokumentiert.
- **Event-Befehle:** kompletter XP-Befehlssatz im Editor-Katalog
  (`qt_editor/QtEventCommandCatalog.cpp`) UND im Runtime-Interpreter
  (`src/EventSystem.cpp`, `EventInterpreter::ExecuteCommand`). Nicht-XP-Codenamen
  im Enum: `OpenMenuScreen`(=XP 341), `OpenSaveScreen`(=342), `GameOver`(=343),
  `ReturnToTitle`(=344). Kampf-Verzweigungen (`IfWin/IfEscape/IfLose`) sind
  indent-basiert — es gibt bewusst KEIN XP-604-Endkürzel.
- **Datenbank-Dialog:** alle 13 XP-Tabs (Akteure … System) vorhanden
  (`qt_editor/QtDatabaseDialog.cpp`).
- **Troop-Kampfereignisse:** Seiten + Bedingungen, Commit `f92f13c`.
- **Random Encounters:** Laufzeit-Zähler 50–150 % von `encounterStep`,
  Troop-Wahl aus `MapInfo.encounterList[8]` (`src/Game.cpp` ~Z. 964).
- **Speichern/Laden im Spiel:** JSON-Slots inkl. Slot-Kopfdaten
  (`src/Game.cpp` `GetSaveSlotInfo`).
- **Skript-Editor:** eigenes F11-Fenster, XP-konform KEIN „Ausführen"-Knopf
  (`1289447`).

---

## PAKET 1 — Tileset-Tab: Passability/Flags komplett 🔴 KRITISCH
**Status: ✅ ERLEDIGT (2026-07-23)** — `TilesetData` hat jetzt alle
XP-Flag-Tabellen (passage/4dir/priority/bush/counter/terrain + Autotile[7]/
Panorama/Nebel/Battleback), Laden/Speichern in `Database.cpp`
(ParseIntArrayInto/WriteIntArray), Laufzeit-Kopplung via
`Tileset::SetTilesetData` an allen Lade-Stellen (Engine-Start, Editor),
richtungsbewusste `GameMap::IsPassable(x,z,dirBit)` + Spieler-Bewegung mit
DirBits. Nachgeprüft: `g++ -fsyntax-only` sauber.

**Problem:** `rpg::TilesetData` (include/rpgmaker3d/Database.h ~Z. 160) speichert
nur `id/name/tilesetName/flags`. `flags` wird von `src/Database.cpp`
(ParseTilesetObject ~Z. 411, Serialize ~Z. 1057) gelesen/geschrieben, aber:
1. **Keine Editor-UI:** `QtDatabaseDialog::buildTilesetsTab()` (~Z. 1134) hat
   nur Name + Grafikdatei.
2. **Laufzeit ignoriert `flags`:** `GameMap::IsPassable` (src/Game.cpp ~Z. 410)
   prüft nur `TileInfo::solid` der Runtime-Klasse `rpg::Tileset`
   (include/rpgmaker3d/Tileset.h) — und `Tileset.cpp` (~Z. 48) setzt
   `solid=false` für ALLE Tiles. Folge: zur Laufzeit ist JEDES Tile begehbar
   (nur Map-Rand blockiert). Das macht jedes Spiel unspielbar.

**Umsetzung (alle 3 Schritte gehören zusammen):**

### 1a) Datenstruktur erweitern (`include/rpgmaker3d/Database.h`)
```cpp
struct TilesetData {
    int id = 0;
    std::string name = "World";
    std::string tilesetName = "tileset_demo.png";
    std::string autotileNames[7];      // NEU: XP 7 Autotile-Slots ("" = leer)
    std::string panoramaName;          // NEU: XP legt Panorama/Nebel/Battleback
    std::string fogName;               // NEU: am Tileset fest (nicht an der Map)
    std::string battlebackName;        // NEU
    std::vector<int> flags;            // BESTEHEND = "passage": 0=begehbar, 1=blockiert
    std::vector<int> passage4dir;      // NEU: Bits 1=unten,2=links,4=rechts,8=oben;
                                       //      0 = Default (alle Richtungen frei)
    std::vector<int> priority;         // NEU: 0..5 (XP-Priorität)
    std::vector<int> bushFlags;        // NEU: 0/1 Durchwiese
    std::vector<int> counterFlags;     // NEU: 0/1 Tresen (Interaktion darüber hinweg)
    std::vector<int> terrainTags;      // NEU: 0..7 frei verwendbar
};
```
Index = Tile-ID wie in den Map-Daten (Zeilen-Index im Tileset-Grid).
(Hinweis: Die RGSS-Tilemap kennt XP-IDs <384 = Autotile; der native Map-Editor
malt reguläre Grid-Tiles — beide Welten dokumentieren, nicht mischen.)

### 1b) Laden/Speichern (`src/Database.cpp`)
- `ParseTilesetObject`: zusätzlich Arrays `flags4dir`,`priority`,`bush`,
  `counter`,`terrain` (gleiches Int-Array-Parsing wie `flags` ~Z. 411-433,
  ggf. in kleine Hilfsfunktion auslagern) + Strings `autotile1..7`,
  `panorama`,`fog`,`battleback` (TryParseString).
- Serialisierung (~Z. 1050-1062): dieselben Schlüssel schreiben.
  Kompatibilität: alte Dateien ohne neue Felder → Defaults, kein Fehler.

### 1c) Laufzeit-Verdrahtung
- `rpg::Tileset` (include/rpgmaker3d/Tileset.h, src/Tileset.cpp):
  Methoden `SetTilesetData(const rpg::TilesetData&)` speichert Kopie;
  `int GetPassage(tileId)`, `int GetPassage4Dir(tileId)`,
  `int GetPriority(tileId)`, `bool IsBush/IsCounter(tileId)`,
  `int GetTerrainTag(tileId)` — jeweils Default 0 wenn Vektor zu kurz.
- Aufrufstellen, die ein Tileset laden: `Editor::LoadTilesetForMap`
  (src/Editor.cpp ~Z. 2667) und die Qt-Playtest-Ladepfade
  (Suche: `tileset->Load(`) — danach `tileset->SetTilesetData(db-Datensatz)`.
- `GameMap::IsPassable(int x,int z)` (src/Game.cpp ~Z. 410):
  pro Layer-Tile zusätzlich `tileset->GetPassage(tileId)==1` → blockiert.
  `TileInfo::solid` danach entweder ganz entfernen oder aus passage befüllen.
- **Richtungsabhängigkeit:** neue Methode
  `bool IsPassable(int x,int z,int dirBit)` — ein Tile blockiert in Richtung
  `dirBit`, wenn `passage4dir[tileId]!=0 && !(passage4dir[tileId]&dirBit)`.
  Aufrufer: Spieler-Bewegung (src/Game.cpp ~Z. 315-330, Achsen-getrennte
  Prüfung → dirBit aus Vorzeichen der Achse ableiten: +x=rechts=4, -x=links=2,
  +z=unten=1, -z=oben=8) und Event-Bewegungen/Move-Routen
  (src/EventSystem.cpp — Suche `IsPassable`-Aufrufe).

**Akzeptanz:** Tileset-Flag „blockiert" im Editor gesetzt → Spieler kann das
Tile im Playtest nicht betreten; „nur ↓ frei" → nur von oben betretbar.
Alte Projektdateien laden ohne Fehler.

**Nicht vergessen:** Map-Properties-Dialog nutzt heute Map-eigenes
Battleback/Nebel (`MapInfo.fog*/battleback*` ~Z. 185ff Database.h) — XP-verwandt
aber NICHT identisch: XP zieht Battleback/Nebel aus dem TILESET. Bestehendes
Map-Verhalten behalten, Tileset-Felder nur als Fallback verwenden, wenn Map
nichts setzt (im TODO-Kommentar am Code vermerken).

---

## PAKET 2 — Qt-Tileset-Flag-Editor (UI für Paket 1) 🔴
**Status: ✅ ERLEDIGT (2026-07-23)** — neues Widget
`qt_editor/QtTilesetGridWidget.{h,cpp}` (8-Spalten-Raster wie XP, 6 Modi
Durchgang/4-Dir/Priorität/Busch/Tresen/Terrain-Tag, Overlay-Malerei,
Links-/Rechtsklick, `flagsChanged()`), verdrahtet in
`QtDatabaseDialog::buildTilesetsTab()` inkl. Autotile1..7/Panorama/Nebel/
Battleback-Feldern + Bildauflösung (textures/ bzw. Graphics/Tilesets/).
In CMakeLists.txt eingetragen. Tokenizer-Check grün.

**Wo:** `qt_editor/QtDatabaseDialog.cpp::buildTilesetsTab()` ersetzen/erweitern.

**XP-Vorbild:** Modus-Knöpfe oben (Durchgang | Durchgang 4-Dir | Priorität |
Busch-Flag | Tresen-Flag | Terrain-Tag), darunter scrollbares Tileset-Raster
(je 8 Tiles/Zeile wie XP? — bei uns: Spalten = Bildbreite/32), Klick ändert
den Flag des Tiles unter dem Cursor.

**Neues Widget** `qt_editor/QtTilesetGridWidget.{h,cpp}`:
- lädt `textures/<tilesetName>` über Projekt-Asset-Pfad (Fallback
  `assets/textures/tileset_demo.png`), rendert Grid mit QPainter
  (Zelle 32×32, optionaler Zoom),
- Overlay je Modus: Passage: grüner Kreis (frei) / rotes ✕ (blockiert);
  4-Dir: Pfeil-Symbole der freien Richtungen, ✕ wenn keins; Priorität: Zahl
  0-5; Busch: „B"; Tresen: „C"; Terrain: Zahl 0-7.
- Klick-Verhalten:
  - Passage: toggle 0↔1
  - 4-Dir: Links-Klick rotiert durch 6 Sinn-Zustände
    (alle frei ● → nur ↓ → nur ← → nur → → nur ↑ → gesperrt ✕),
    Rechts-Klick = alle frei
  - Priorität: +1, Wrap 5→0; Busch/Tresen: toggle; Terrain: +1, Wrap 7→0
- `Q_OBJECT`-Signal `flagsChanged()` → Dialog markiert Dirty/Apply.
- Zusätzlich im Tab: Eingabefelder für `autotile1..7`, `panorama`, `fog`,
  `battleback` (LineEdits mit Datei-Auswahl-Button, Konsistenz mit anderen
  Grafik-Feldern im Dialog prüfen, z.B. QtDatabaseDialog hat makeLine()).

**QL-Regel beachten:** `QL(`/`QStringLiteral(` NUR mit String-Literalen;
Variablen via `QString::fromStdString`. Nach dem Edit: Python-Tokenizer-
Brace-Check auf die Datei (Rezept in Session-Notizen /
`git log --grep Tokenizer`).

**Akzeptanz:** Flags im Dialog sichtbar/klickbar, überleben OK→Speichern→
Projekt-Neuöffnen.

---

## PAKET 3 — Map-Zeichenwerkzeuge (Stift/Rechteck/Ellipse/Flut) 🟡
**Status: ✅ ERLEDIGT** — beim Code-Audit 2026-07-23 als bereits vollständig
implementiert befunden (frühere Session hatte das Paket gebaut, ohne den
TODO-Status zu pflegen). Verifikation gegen die Akzeptanzkriterien:
`qt_editor/QtMapTab.cpp`: `mToolGroup`/`mToolBtns[4]` (Stift/Rechteck/
Ellipse/Füllen, exklusiv via QButtonGroup, deutsche Tooltips) in der
Toolbar neben den Ebenen-Knöpfen; Canvas `tool`-Feld 0..3; Stift =
`applyAt/paintCell` (kein Verlauf bei Nicht-Änderung); Rechteck/Ellipse:
Drag mit Vorschau (paintEvent Rahmen/Ellipse), Release → `applyShape`
(Ellipse via normierter Mittelpunktsgleichung über Zellzentren),
Rechtsklick bricht ab (kein Verlauf); Flutfüllung: `applyFloodAt`
iterativ mit std::queue + seen-Set (500×500-sicher), 4-Nachbarschaft,
nur aktive Ebene; Undo-Bündelung: Form UND Flut laufen als EIN Stroke
(`onStrokeBegin/End` + `mStrokeAccum` erster Startwert pro Zelle) —
Strg+Z rollt komplett zurück. Kein weiterer Handlungsbedarf.

---

## PAKET 4 — F9-Debug-Inspektor (Schalter/Variablen) 🟡
**Status: ✅ ERLEDIGT (2026-07-23)** — als **F10** umgesetzt (F9 bleibt bewusst
das HUD-Toggle — seit PAKET 10 `GameUI::ToggleHud` statt RmlUi). Engine-intern
über die RGSS-Fensterschicht:
`Engine::ToggleDebugWindow/UpdateDebugWindow/RedrawDebugWindowContent`
(src/Engine.cpp ab ~Z. 2380, Members mDbg* in Engine.h). Live: Schalter per
Enter togglen, Variablen per Enter editieren (Ziffern, M = Negativ, Backspace,
Enter = übernehmen, Esc = Abbruch/schließen, Pfeile/←→ = Blättern). 0,25-s-
Live-Refresh. Greift auf `Game::Get().Switches()/Variables()` mit Namen aus
`Database::Get().System().switches/variables` zu. Offen: XP trennt die Listen
(Tabs) — bei uns eine gemeinsame Liste (Schalter oben, Variablen unten),
funktional gleichwertig.

**XP:** Im Testspiel (nicht im Released-Build) öffnet F9 ein Fenster mit zwei
Listen: Schalter (AN/AUS umschaltbar) und Variablen (Zahl editierbar) —
ändert sich LIVE im laufenden Spiel.

**Bei uns:** F9 = HUD-Toggle (Engine::Update → `GameUI::ToggleHud`, seit
PAKET 10; zuvor RmlUi). Lösung wie XP: F9 NUR im Playtest den
Debug-Inspektor, HUD-Toggle auf andere Taste ODER Debug-Fenster als
eigenen Toggle DANEBEN — umgesetzt: **F10** = Debug-Inspektor, F9 = HUD.

**Datenquelle:** Schalter/Variablen-Werte zur Laufzeit stehen im Game-System
(Suche: `mSwitches`, `mVariables` in src/Game.* — `Game_System` hält sie;
Namen aus `Database::Get().System().switches/variables`).

**Darstellung (historisch):** damals „RmlUi-Panel ODER natives RGSS-Overlay"
— umgesetzt als RGSS-Overlay (RgssUI); RmlUi ist seit PAKET 10 entfernt.
Wichtig: nur aktiv, wenn Playtest-Flag gesetzt (Editor-Start /
BATTLE_TEST-Konstante?), im exportierten Player deaktivieren.
Edits: Schalter per Klick togglen; Variable: Dialog/Doppelklick → Zahl.

**Akzeptanz:** Im Playtest F9 → Liste ändert sich live, wenn ein Event einen
Schalter setzt; Toggle im Debugfenster wirkt sofort im Spiel.

---

## PAKET 5 — Animations-Editor + Laufzeit-Wiedergabe 🟡 XL
**Status: ✅ ERLEDIGT (2026-07-23)** — `AnimationData`/`AnimFrame`/`AnimCell`
in Database.h (XP-Felder: Zellenzuschnitt 192×192/5 Spalten, x/y/scale/rot/
opacity, SE je Frame, Flash scope/rgb/duration, position 0-2), Serialisierung
`Data/Animations.json` (ParseAnimationObject + Schreiber in Database.cpp),
`Database::AnimationSet()`+`GetAnimation(id)`. Editor-Tab neu:
QtDatabaseDialog::buildAnimationsTab() mit Frame-Navigator (◀ ▶ +/löschen,
Duplikat-Einfügung hinter aktuellem Frame), QtAnimFrameCanvas (halbe XP-
Auflösung 640×480, Sheet-Bild, Klick=+Zelle, Rechtsklick=−Zelle, Auswahl),
Zellen-Formular, SE/Flash-Formular, Live-Sheet-Nachladen. Laufzeit: Game::
StartMapAnimation/UpdateAnimations/ApplyAnimFrame (16 fps XP-Takt, Sprite-
Pool aus RGSS-Drawable, Flash-Sprite mit Fade, SE über playSeHook →
engine-BGM-Pipeline, Ersatzzelle bei fehlendem Sheet, Sheet-Cache pro ID);
Event-Befehl ShowAnimation (207) verdrahtet (Namens-Fallback auf
command.text). Bekannte Näherungen (im Code vermerkt): Ziel-Event (param1)
wird als Canvas-Mitte interpretiert (XP-Korrektur in 3D-Weltprojektion als
Folgearbeit notiert), Zellenschema fix 192×192/5sp. mSystem.animations bleibt
Legacy-Namensliste.

**Ist:** DB-Tab ist nur Namensliste (`mSystem.animations` = vector<string>).
Laufzeit: `CC::ShowAnimation` (src/EventSystem.cpp ~Z. 647) zeigt nur
schwebenden Text „*" (`onShowAnimation`-Hook existiert, wird NIRGENDS gesetzt).

**XP-Referenz (Animationen-Tab):**
- Animationsgrafik: Spritesheet mit Zellen 192×192 px, 5 Zellen pro Zeile,
  Zellen-Index 0..99 (0-4 erste Zeile ...), Graustufen-Hue-Shift möglich.
- Frames 1..200: jeder Frame = Platzierung mehrerer Zellen (max. 20) auf
  640×320-Canvas (Mittelpunkt 320,160), pro Platzierung: x, y, scale(%),
  rotation, flip, opacity, blend_type.
- pro Frame optional: SE (Datei+Vol+Pitch) und Flash (Bildschirm/Ziel,
  Farbe+Dauer).
- Position relativ zum Ziel: Oben/Mitte/Unten (Dropdown).
- Tasten im XP-Editor: Zelle per Drag auf Canvas, Rechtsklick löscht,
  „Batch-Edit": Zellen-Taktung (Takt-Verschiebung), Play-Probe.

**Sinnvolle 3D-Adaption (NICHT übernehmen: volle 640×320-2D-Canvas-Komplexität):**
1. `AnimationData` in Database.h: name, file, frames: vector<Frame{se_name,
   se_vol, se_pitch, flash_color, cells: vector<Cell{cellId,x,y,scale,rot,
   opacity}>}>. Speichern wie andere DB-Objekte (Database.cpp Muster:
   ParseTroopObject als Vorlage für verschachtelte Arrays).
2. Editor-Tab: Liste links bleibt; rechts: Grafik-Wahl + Frame-Scrollbar +
   Zellen-Canvas (QWidget paintEvent, Zellen aus SpriteSheet blitten) +
   Buttons „Zelle hinzufügen/löschen", SE-Felder, „Abspielen".
3. Laufzeit: `onShowAnimation` verdrahten (Game.cpp setzt Hook): Ziel =
   anvisiertes Event/Spieler; Frames in Takt (XP: 15 fps … eigentlich jeder
   Frame = 1/15 s? — prüfen: XP Default-Skript `Sprite_Animation` = 2
   Ticks/Frame) nacheinander via RgssUI-Sprites rendern (Sprite existiert,
   `RPG::Animation`-ähnlich), Audio via AudioManager, danach
   Interpreter-Warte-Flag aufheben. Waffen-/Skill-Animationen im Kampf:
   **erledigt mit PAKET 12** (2026-07-24).

**Akzeptanz:** Befehl „Animation zeigen" spielt im Spiel die gebaute Sequenz
am Ziel ab und wartet bis zum Ende.

---

## PAKET 6 — Kleinigkeiten (Stand 2026-07-23)
- [x] **XP-Importdialog („Material base") (ERLEDIGT 2026-07-23):**
  `QtAssetBrowserDock` kann jetzt importieren: Button **„Importieren…"**
  (Datei-Mehrfachwahl → Kategorie-Dialog `ImportTargetDialog` mit den
  XP-Ordnern, die die Engine wirklich sucht: Graphics/Tilesets, Autotiles,
  Characters, Animations, Battlers, Battlebacks, Panoramas, Fogs, Pictures,
  Titles, Gameovers, Icons, Transitions, System, Windowskins + Audio BGM/
  BGS/ME/SE; Vorauswahl Audio→SE, Bild→Tilesets) und **Drag & Drop aus
  dem Dateimanager** auf den Baum (`AssetTreeWidget`-Subklasse, externe
  URL-Drops; Ziel = Ordner unter dem Cursor, Drop in die Leere →
  Kategorie-Dialog). Kopieren legt Zielordner an, fragt bei Konflikt
  (Überschreiben/Alle/Überspringen), refreshed danach. Scan-Roots um
  `Graphics/`+`Audio/` erweitert (waren unsichtbar!), `Project::New`
  legt die volle Graphics-Ordnerstruktur bei neuen Projekten an.
  **Bewusste Näherung:** XP-Farbschlüssel-Import (linke/rechte
  Transparenzfarbe) entfällt — die Engine arbeitet mit echtem PNG-Alpha;
  Hinweis steht im Dialog.
- [x] **Prioritaet zur Laufzeit (ERLEDIGT 2026-07-23):** XP liest Prios
  aus $data_tilesets, wenn die Tilemap keine eigene `priorities`-Table
  traegt — jetzt genauso: `RgssSetTilePriorityHooks` (RgssUI.h), Engine
  verdrahtet Paket-1-`TilesetData` der aktiven Karte (RGSS-ID>=384 ->
  visueller Index, Bit7 = Busch-Flag). `DrawTilemap` (pro Tile) und die
  z-Split-Sortierung in `Render()` nutzen den Fallback; `GetMaxPriority()`
  neu. **Entscheidung nativer 3D-Pfad:** es gibt KEINEN nativen 3D-Tile-
  Renderer (Karte optisch komplett = RGSS-Tilemap; 3D = Entities/Spieler
  auf Kollisionsebene). Falls einer gebaut wird: Hoehen-Offset
  (priority*0,5 Tiles) dort spiegeln — als Folgepunkt vermerkt.
- [x] **Busch-Flag-Effekt (ERLEDIGT 2026-07-23, RGSS-Pfad):** Busch-
  Tiles zeichnen ihre untere Haelfte (normale Tiles: `Quad4`-Verlauf,
  Autotiles: untere 16er-Subkacheln) mit 45% Alpha — XP-Optik „im Gras
  stehen". Kodierung via Bit 7 im Prio-Hook (Engine). Echte Sprite-
  z-Maske (Spieler nur unten transparent) bleibt Folgearbeit, sobald
  RGSS-Sprites denselben Hook abfragen (Punkt unten bei Terrain-Tag
  vermerkt).
- [x] **Counter-Flag (ERLEDIGT 2026-07-23):** `EventSystem::TryInteract`
  erweitert: findet kein ActionButton-Event im Normalradius, schaut er ein
  Tile weiter; löst es nur aus, wenn die Mittelkachel zwischen Spieler und
  Event counter-geflaggt ist (alle Ebenen geprüft). XP-Feeling an
  Verkaufstresen.
- [x] **Terrain-Tag (ERLEDIGT 2026-07-23):** Semantik festgelegt:
  0 = kein Ton, 1 = Gras, 2 = Stein, 3 = Wasser (Werte 4..7 frei).
  Umsetzung: `RgssSetFootstepHooks(stepFor, play)` — DrawTilemap loest
  alle 2,4 s den Tile unter Bildschirmmitte (oberster Layer) auf; Engine
  mappt Tag->Name, laedt `Audio/SE/footsteps/<name>(.wav)` (Fallback
  `<name>` direkt) und spielt NUR bei Bewegung (kein Tritt im Stand,
  0,25-Tile Delta).
- [x] **Folge: Sprite-Busch-z-Maske + Terrain 4..7 (ERLEDIGT 2026-07-23):**
  Neu in GameMap: `IsBushAt(worldPos)` (irgendeine Ebene busch-geflaggt)
  und `GetTerrainTagAt(worldPos)` (oberste Ebene mit Tag != 0 gewinnt),
  Game.cpp — `nm -C`-Symbolprobe bestanden (LNK2019-Lektion). **Busch auf
  Charakteren:** PlayMode-Charakter-Marker (Spieler + alle Events) werden
  bei Busch-Tile halbiert gezeichnet — untere Haelfte 45% Alpha, obere
  opak (`drawCharCube`-Lambda in Engine::RenderScene). 3D-Adaption der
  XP-bush_depth-Maske: wir haben keine 2D-Charakter-Sprites im Playmode,
  der Halbschnitt des 3D-Markers ist die getreueste Entsprechung
  (Transparent-Flag 208 multipliziert sich weiter auf beide Haelften).
  **Terrain-Tag-Belegung 0..7 (Defaults, XP-Tags sind frei):** 0 lautlos,
  1 Gras, 2 Stein, 3 Wasser, 4 „hohes Gras" (Sound wie Gras + Encounter-
  Zaehler tickt doppelt so schnell, Game.cpp), 5 Sand, 6 Holz/Bruecke,
  7 Eis; Footstep-Mapping in Engine.cpp erweitert, Dateien weiterhin
  `Audio/SE/footsteps/<name>(.wav)`. Offen bleibt bewusst: Eis-Rutsch-
  Physik (Tag 7) als moegliche Folgearbeit — hier nur der Sound.
- [x] **Player-Export vc_redist-Hinweis (ERLEDIGT 2026-07-23):** README-
  Abschnitt „Fehlersuche: Player/Editor startet nicht (Windows)" erklärt
  die Schließt-sofort-Historie (vc_redist) + Vorschläge (installieren,
  statisch linken, engine.log [FATAL-STARTUP] lesen).
- [~] **XP_Scripts/ schrittweise lauffähig (WEITGEHEND ERLEDIGT
  2026-07-23; Rest = dokumentierte Grenzen):** die 90 Original-Skripte
  gegen unsere RGSS-Implementierung laufen lassen. **Endstatus der
  Stufenliste: (a)+(b)+(c) load_data komplett [Stufen 1-3 inkl. BT_-
  Fallback]; (d) Marshal = bewusste Grenze (unser Slot-System); (e) zwei
  Wahrheiten vermieden (Opt-in-Vorgehen); (f) $game_map-Klasse [Stufe 4f];
  (g) $game_actors/$game_party/$game_troop/$game_screen + Name-Input +
  $game_temp/$game_system [Stufen 4g Teile 1-5]; (h) Scene-Framework als
  Opt-in (Scene_Base/$scene-Tick); (i) Trockenlauf-Evaluation gemacht +
  Rundung Teile 1-2 eingebaut. Verbleibende, bewusst ehrliche Grenzen
  sind unten pro Punkt markiert (Marshal, Interpreter-Übernahme,
  snap_to_bitmap, events-Hash; Font#shadow + Wave-Render inzwischen
  erledigt, siehe PAKET 13).**
  **Stufe 1 ERLEDIGT — `load_data`-JSON-Bruecke:** RubyRgss.cpp hat neu
  `__engine_db_fetch(kind)` (privater Kernel-Helfer, Release-`Database::Get()`
  als generische Ruby-Hashes; mruby-4.0-Fallen behoben: `mrb_intern` ist
  3-arg, `mrb_hash_set` braucht `<mruby/hash.h>`); RgssPrelude `load_data`
  mappt die 13 Kerndateien (`Actors/Classes/Skills/Items/Weapons/Armors/
  Enemies/Troops/States/Animations/Tilesets/System/MapInfos.rxdata`) darauf
  und baut XP-konforme RPG::*-Objekte ([nil]-Shift, Index = ID; Tileset-
  Flag-Tabellen mit +384-Offset; Actor-Parameter linear initial->final über
  99 Level; Troop-Member ohne Koordinaten aufgereiht). XP-Skripte bekommen
  ihre `$data_*` so aus dem Datenbank-Dialog-JSON.
  **Offen (geordnet, nächste Stufen zuerst):** (a) ~~`Map%03d.rxdata`~~
  **ERLEDIGT 2026-07-23 (Stufe 2):** `__engine_db_fetch("map", id)` lädt
  `maps/map<N>.map` (Binaerformat, statisches Map::Load — keine Engine/
  Singleton noetig) und liefert Geometrie + Layer (z-major, roh inkl. -1);
  tileset_id/encounter_* aus `MapInfo` (die .map-Datei fuehrt sie nicht).
  Prelude erkennt den Dateinamen ohne Regexp (mruby-Kern), baut RPG::Map
  mit data-Table(w,h,3) und native ID -> +384-RGSS-Offset; `events = {}`
  bewusst: NPC-Rendering + Interpreter laufen nativ (LoadMapEvents haette
  singleton-Seiteneffekte gehabt — Event-Export als moegliche Stufe 2b
  vermerkt, falls XP-Skripte ev.pages/list wirklich lesen muessen);
  (b) ~~`CommonEvents.rxdata`~~ **ERLEDIGT 2026-07-23 (Stufe 3):** Quelle ist
  `EventSystem::Get().GetCommonEvents()` (das CommonEvent-Struct existierte
  dort — NICHT in Database, kein neues DB-Struct noetig). Export: trigger
  Autorun→1/Parallel→2, switch_id, volle Befehlsliste — unsere
  EventCommandCode-Enum ist exakt XP-kodiert (101..355), daher 1:1;
  params gemischt (reine Ganzzahl-Strings als Integer, Rest als String),
  Prelude setzt [param1..3, text, params...] zusammen und kuerzt
  Trailing-Defaults (0/""), wie XP-Daten;
  (c) ~~`BT_*.rxdata`~~ **ERLEDIGT 2026-07-23 (Fallback):** XP-Kampf-Test-
  Dateien mappen auf die Produktiv-Bruecke (Testdaten fuehren wir nicht —
  dokumentierte Vereinfachung). Damit sind ALLE statisch auftretenden
  load_data-Dateien der 90 Skripte bedient (26 load_data-Aufrufe);
  (d) Marshal.load/save
  (Save files, 32 Aufrufe in den Skripten, alle in Scene_Save/Load/File)
  bleibt bekannte Grenze — unser Slot-System stattdessen;
  (e) **Gap-Analyse 2026-07-23 (Ergebnis):** `$game_switches/-variables/
  -self_switches/-party/-player/-map` existieren nativ (RubyVM.cpp
  gv_set-Block) — die XP-Ruby-Dateien wuerden sie per Klassen-Reopen
  ENTKOPPELN (zwei Wahrheiten), daher XP-Modus nur schrittweise:
  **Stufe 4 = $game_map-XP-Flags erledigt 2026-07-23 `passable?(x,y,d,
  self_ev)/bush?(x,y)/terrain_tag(x,y)` nativ am Game_Map-Modul (Dir d
  2/4/6/8 -> DirBits; GameMap-Overloads IsBushAt/GetTerrainTagAt(x,z)
  neu, Welt-Versionen delegieren); nm-Probe 4 Symbole.** Naechste Stufen:
  (f) ~~$game_map als XP-faehige Instanz~~ **ERLEDIGT 2026-07-23 (Stufe 4f):**
      `Game_Map` ist jetzt eine native KLASSE (war Modul), `$game_map` eine
      Instanz. Neu: `data` (Table(w,h,3) bei Abruf frisch aus der gebundenen
      Karte, native IDs -> +384; Schreib-zurueck bewusst Grenze),
      `display_x/y` + Setter (XP 1/128-Einheit, logischer Scroll-State
      GameMap::mDisplayPos — Kamera bleibt unveraendert, ehrlich),
      `events` (leerer Hash — wie Map-Bruecke begruendet), `refresh`
      (RefreshAllPages), `need_refresh/=` (Sofort-Semantik wie die
      Schalter-Setter; Getter immer false), `map_id` als XP-Alias auf id.
      Bestand (visible?, setup, width/height, passable?/bush?/terrain_tag)
      laeuft unveraendert als Instanzmethoden; Default-Skripte nutzten
      `Game_Map.` nirgends (Bruchfreiheit per grep verifiziert).
      mruby-4.0-Falle direkt gefangen: `mrb_int(v)` ist dort 2-arg-Makro
      (`mrb_int(mrb, v)`) statt 3.x-Funktion — `mrb_as_int` genutzt, und
      in statischen rb_*-Funktionen heisst der State `mrb` (nicht mMrb);
  (g) **TEIL 1 erledigt 2026-07-23: `$game_actors` + `Game_Actor`-Bruecke**
      (RubyVM.cpp + Prelude): native Klasse Game_Actor wrappt die
      GameActor-Laufzeitstruct (Party-Member Quelle; Nicht-Party-Akteure =
      fluechtige Setup-Instanz aus der Datenbank, dokumentierte Naeherung).
      Voll nativ: id/actor_id, exist?, name/=, class_id, level/=, exp/=,
      next_exp (ExpForNextLevel), add_exp (-> Levelups), hp/sp inkl.
      XP-Clamp-Setter, maxhp/maxsp/atk/def/agi (mit Equip-Boni wie XP),
      dead?, recover_all, states + add_state/remove_state, skills +
      learn_skill/forget_skill, weapon_id, armor1..4_id, character_name,
      face_index. Prelude: `Game_Actors`-Sammlung mit Instanz-Cache pro ID
      ($game_actors = XP-Identitaet) + Komfort-Reopen: `int` (aus der
      Akteur-Parameter-Table der load_data-Bruecke), weapon/armor1..4 als
      Objekte. **Erledigt in TEIL 3:** Game_Troop- + Game_Screen-Bruecke,
      Game_Actor-Equip-Mutatoren (change_equip/equip); **in TEIL 4:**
      Name-Input-Verdrahtung (siehe unten).
  (g) **TEIL 2 erledigt 2026-07-23: Game_Party-XP-Vervollstaendigung:**
      nativ neu: item_number/weapon_number/armor_number (XP-Namen,
      Aliase auf die count-Bindings), has_item, all_dead?; interne
      __actor_ids/__item_ids/__weapon_ids/__armor_ids. Prelude-Reopen:
      actors (Game_Actor-Instanzen aus $game_actors = XP-Identitaet),
      actor(id), items/weapons/armors (ID-Listen-Naeherung statt Hash —
      dokumentiert), max_level, average_level, item_can_use? (count +
      consumable via Bruecke), movable?.
  (g) **TEIL 3 erledigt 2026-07-23: `$game_troop`/`Game_Enemy`,
      `$game_screen`/`Game_Picture`, Game_Actor#equip:**
      - **Game_Enemy nativ** (RubyVM.cpp): @battle_index bindet an den
        LIVE-Battler (BattleSystem::Enemies()), sonst fluechtiger Orphan
        aus EnemyData (Muster wie Game_Actor). id/enemy_id, index, exist?,
        name, battler_name/hue, hp/sp mit XP-Clamp-Setzern (schreiben live
        in den Battler inkl. isDead-Sync), maxhp/maxsp/atk/def/agi, dead?,
        recover_all (loescht auch @states, XP-verhalten), exp/gold,
        transform (spiegelt Event-Befehl 336: neue Art uebernimmt Werte
        komplett), animation1/2_id = 0 (EnemyData fuehrt keine — ehrlich).
        States/letter im Prelude als Objekt-Ivars (nativer Kampf kennt
        keine Gegner-Zustaende — dokumentierte Grenze).
      - **Game_Troop**: nativ nur `__enemy_ids(troop_id)` (Live-Battler
        bevorzugt, sonst TroopData.members); Prelude: setup/members/
        troop_id + `$game_troop`. **Kampfstart-Hook** `Game::onBattleStarted`
        (Game.h): feuert in BEIDEN Trichtern (Game::StartBattleByTroop UND
        EventSystem WireInterpreter onBattleProcessing — Wichtig: der
        Event-Befehl laeuft NICHT ueber StartBattleByTroop!); RubyVM
        verdrahtet damit `$game_troop.setup(troop_id)` (XP: macht
        Scene_Battle per Hand), Hook-Aufloesung in Shutdown (haelt `this`).
      - **Game_Screen nativ** direkt an EventSystem::ScreenEffects
        (derselbe Zustand wie Befehle 223-225): start_flash/flash_color,
        start_tone_change/tone (frisches Tone-Objekt), start_shake/shake
        (Integer-Naeherung des XP-Versatz-Getters). Dauer in XP-FRAMES
        (40 fps) -> Sekunden — Umrechnung dokumentiert. Prelude:
        `$game_screen`, pictures (51 Game_Picture), weather als reiner
        Zustand (type 0-3, max=power*10; Rendern via RPG::Weather in
        Ruby-Szenen, kein nativer Hook), update no-op.
      - **Game_Picture** (Prelude): show/move/fade/erase/rotate/\
        start_tone_change ueber die native GameUI-Bildschicht (Laufzeit-ID
        bei show gemerkt). Dokumentierte Naeherungen: zentriertes Zeichnen
        (origin-1-Verhalten), zoom_x/zoom_y gemittelt, rotate als Tween-
        Schritt, Picture-Ton nur als Zustand gespeichert.
      - **Game_Actor#change_equip nativ** (slot 0-4, item nil/ID/Objekt)
        + **#equip im Prelude** (Inventar-Tausch mit $game_party — exakt
        die XP-Aufteilung).
  (g) **TEIL 4 erledigt 2026-07-23: Name-Input-Verdrahtung komplett:**
      Bestand (verifiziert): Event-Befehl 303 oeffnet GameUI-
      Namenseingabe und schreibt zurueck (EventSystem WireInterpreter),
      `Game_Actor#name=` nativ (Teil 1), Save/Load persistiert den Namen
      (JSON `"name"` hin UND zurueck). **Neu:** `UI.open_name_input(
      actor_id = nil, max_chars = 8, prompt = "") { |name| ... }` —
      XP-Befehl-303-Verhalten jetzt auch aus Custom-Szenen heraus: mit
      actor_id Starttext = aktueller Name + Ergebnis-Rueckschreiben,
      nil = reine Eingabe nur per Block. Block GC-sicher geparkt
      (Muster wie UI.open_list_menu), Rueckruf ueber neue public
      RubyVM::CallNameInputResult (Stub-Zweig mitgeliefert + nm-geprueft).
      Damit sind ALLE (g)-Punkte abgeschlossen.
  (g) **TEIL 5 erledigt 2026-07-23: `$game_temp` + `$game_system`:**
      Game_Temp 1:1 als reine Ruby-Datenhalde im Prelude (in XP genauso
      — kein nativer Gegenpart, kein Zwei-Wahrheiten-Risiko); Attribut-
      liste = XP-1.03 vereinigt mit den per grep beweisgefuehrten
      Zugriffen der 90 Originalskripte (inkl. choice_*/num_input_*/
      forcing_battler/in_battle/map_bgm/battle_proc/...). Game_System
      API 1:1 aus der 1.03-Datei uebernommen: zwei Interpreter-Instanzen
      (neue Interpreter-Datenhalde — Befehle bewusst NICHT, s. Punkt i),
      timer/save/menu/encounter-Disabled/message_position/-frame/
      save_count/magic_number, bgm/bgs/me/se _play/_stop/_fade/
      _memorize/_restore (String-Pfade exakt XP, Aufloesung via
      ResolveAudioPath), playing_bgm/bgs; windowskin_name/battle_bgm/
      battle_end_me mit EIGENEM Override (attr) + Lazy-$data_system-
      Ersatz via load_data-Bruecke ($__engine_db_system-Cache — lazy,
      weil der Prelude beim VM-Start noch keine Projekt-DB sieht.
  (h) ~~Scene_*-Framework~~ **ERLEDIGT 2026-07-23 (Opt-in XP-Modus, wie
      im Eintrag festgelegte Architektur):** Aktivierung `UI.xp_scene_mode
      = true` oder Game.ini `XpSceneMode=1` (CustomConfig.xpSceneMode,
      Default AUS = natives Verhalten). Prelude: `Scene_Base` mit
      start/update/terminate + Engine-Tick `__engine_frame` (start
      einmalig -> update pro Frame -> terminate einmalig beim
      $scene-Wechsel), `$scene = nil` Standard. Nativ: RubyVM::Update
      ruft pro Frame `$scene.__engine_frame` (Block 3 nach SceneManager
      und $game.update). Szenenwechsel XP-konform per Zuweisung
      `$scene = Scene_X.new`, wirksam ab dem naechsten Frame (kein
      Rekursions-Stapel). **Ehrliche Abweichung:** die Main.rb-Schleife
      `while $scene != nil; $scene.main; end` darf im XP-Modus nicht
      verwendet werden (Engine ownet den Frame-Loop; main = 1 Tick als
      Kompat-Fassade) — im Prelude-Kommentar dokumentiert.
  (i) interpreter 1-7: bewusst NICHT uebernehmen (unser nativer
      EventSystem-Interpreter bleibt fuehrend); **TROCKENLAUF-EVALUATION
      2026-07-23 (statisch, API-Abgleich gegen Bindings/Prelude):**
      - **Window-Schicht startklar:** BindRgssWindowEx liefert das XP-
        Vollset (contents, cursor_rect, active/pause/stretch, opacities,
        ox/oy, viewport, windowskin als Bitmap, XP-initialize), Bitmap-
        API breit (blt/stretch_blt/fill_rect/draw_text/text_size/font/
        hue_change/...), Input-XP voll (update/press?/trigger?/repeat?/
        dir4/dir8 + alle Tasten), Font.default_* im Prelude, nativer
        Renderer zeichnet das volle XP-Windowskin 192x128 (Hintergrund,
        Rahmen, Cursor inkl. Blinken/Aktiv-Halb, Pause-Indikator).
        Damit API-seitig uneingeschraenkt startbar: Window_Base,
        _Selectable, _Command, _Help, _Gold, _PlayTime, _Steps,
        _MenuStatus, _InputNumber, _NameEdit/_NameInput, _DebugL/R,
        _Skill/_SkillStatus/_Item/_Status/_Target, _Equip*, _Shop*,
        _BattleResult/_BattleStatus, _PartyCommand, _Message
        ($game_temp seit Teil 5 vorhanden).
      - **BLOCKIERT:** Window_SaveFile + Scene_File/Save/Load
        (Marshal — Grenze d; unser Slot-System stattdessen).
      - **Sprite/Spriteset startbar:** Sprite_Character/_Picture/
        _Timer/_Battler, Spriteset_Map (Plane/Tilemap nativ, $game_map.
        data) — ABER $game_map.events ist bewusst leer: NPC-Sprites
        fehlen im Ruby-Spriteset (native Darstellung laeuft ausserhalb);
        Spriteset_Battle findet jetzt $game_troop.members, ohne
        Animations-IDs am Enemy (0).
      - **Bekannte Fehlstellen — RUNDUNG 2026-07-23 (Teil 2):**
        - ~~Bitmap-Argumentformen~~: ENTFAELLT — fill_rect/draw_text
          akzeptieren bereits beide XP-Formen (4 Int + Rect);
          gradient_fill_rect ebenso verbaut.
        - **Font#shadow — ERLEDIGT mit PAKET 13 (2026-07-24):** der
          eingebaute Text-Renderer zeichnet den Schatten jetzt mit
          (1px Suedost-Offset, schwarz, Alpha = min(128, Text-Alpha),
          Source-Over wie XP Color(0,0,0,128)).
        - **Sprite wave_*** (height/amp/length/speed) — ERLEDIGT mit
          PAKET 13 (2026-07-24): Sinusverzerrung im Renderer
          (Streifen-Stueckelung, max. 4px/64 Streifen; nur unrotierte
          Sprites; Bush-Weichzeichner + Rotation unverzerrt — ehrlich
          vermerkt). wave_height bleibt reiner Kompat-Zustand.
        - **Graphics.wait(duration)**: frame-freundlicher Kompat-No-op
          (Blockieren einfrieren lassen wuerde die UI; dokumentiert).
        - **Arrow_*-Battler-Position — GESCHLOSSEN:** Game_Enemy UND
          Game_Actor liefern jetzt x/y als RGSS-Canvas-Projektion der
          Battler-Weltposition via Game::worldToScreenHook (0 ohne
          Hook/ausser Kampf — ehrlich), sowie die XP Game_Battler-
          Attribute screen_x/screen_y (Getter: Setter-Ablage bevorzugt,
          sonst Projektion — XP-Spritesets duerfen legen) plus blink/
          screen_z als Halde im Prelude.
        - **Graphics.snap_to_bitmap/snap:** bewusst OFFEN — synchroner
          GL-Readback aus dem Ruby-Kontext ist zeitkritisch (Frame-
          Timing/Kontext); Transition-Mechanik nutzt den vorhandenen
          Freeze-Snapshot bereits nativ (Graphics.freeze/transition
          funktionieren ohne snap).
      - **NICHT portieren:** Scene_Battle 1-4 (Game_BattleAction/
        Animation/Arrow-Abhaengigkeiten — unser nativer Kampf +
        Battle-Custom-API); Main.rb-Blockierschleife (XP-Modus Ticket h
        nutzt Scene_Base-Tick stattdessen); Interpreter 1-7 (nativ).
      - **Startbar mit Sinn nur im XP-Modus:** Scene_Title/Menu/Item/
        Skill/Equip/Status/Name/Shop/Debug/End/Gameover — als volle
        XP-Nachbildungen auf unserem $game_*/$data_*-Rueckgrat; dop-
        pelte native Oberflaechen dann abschalten (UI.native_*).
- [x] **Animations-Ziel (param1) beachten (ERLEDIGT 2026-07-23):**
  `EventSystem` loest param1 jetzt XP-konform auf (-1 Spieler / 0 dieses
  Event via `mEventId` / >0 Event-ID via `EventSystem::Get().GetEvent`,
  worldPos mit Map-Koordinaten-Fallback) und ruft
  `Game::StartMapAnimationAt(animId, worldPos)`. Neu:
  `Game::worldToScreenHook` (Engine verdrahtet ihn einmalig): projiziert
  die Weltposition mit View/Proj der Laufzeitkamera in NDC -> RGSS-Canvas
  (640x480; stimmt, weil der RGSS-Renderer den Canvas direkt auf den
  Framebuffer streckt - canvasUV == NDC-UV). Animation `position`
  (oben/mitte/unten) wird relativ zur Projektion versetzt (+-80), Ergebnis
  auf 32..608 x 16..464 ge-clampt. Fallback ohne Hook/Kamera/hinter
  Kamera: altes statisches Mittel-Verhalten.

---

## PAKET 7 — CI-Build-Fixes (Windows/MSVC + mruby 4.0.0) ✅ ERLEDIGT 2026-07-23 (CI-Lauf 7: success, Run 30003416040)
Der erste CI-Lauf nach Paket 1–6 schlug auf Windows fehl (Linux-g++ lokal war
grün — alles Windows-spezifische Fallen). Fixes:

1. **`CreateWindow` ↔ windows.h-Makro:** Unter Windows expandiert das Makro
   `CreateWindow`→`CreateWindowW` und zerbricht `RgssUI::CreateWindow`
   (C4003/C2059 in RgssUI.h, Engine.cpp, RubyVM.cpp, RubyRgss.cpp).
   Methode umbenannt zu **`RgssUI::MakeWindow`** (Deklaration RgssUI.h,
   Definition RgssUI.cpp, Aufrufe: Engine.cpp (F10-Debug), RubyVM.cpp
   (`Window.new` Groovy-Pfad), RubyRgss.cpp (`Window.new` RGSS-Pfad)).
   Regel: NIEMALS Member `CreateWindow` nennen — windows.h-Makro.
2. **mruby 4.0.0: `mrb_integer_value` entfernt** → heißt jetzt
   `mrb_int_value(mrb, i)`. Neuer Header **`include/rpgmaker3d/RubyCompat.h`**
   mit `RPG_MRB_INT_VALUE(mrb_, i)` — IMMER 2-argumentig aufrufen (State im
   Install-Scope heißt dort `mMrb`). Kompat: mruby 4.x `mrb_int_value`,
   3.x `mrb_integer_value`, ≤2.x `mrb_fixnum_value`. Alle 83 Aufrufstellen
   umgestellt. Lokal verifiziert gegen echte mruby-4.0.0-Header (geklont
   nach /tmp, `-fsyntax-only`, beide Dateien sauber: RUBY-Zweig UND
   Stub-Zweig).
   **Falle (CI-Lauf 2):** Der ursprüngliche 1/2-Arg-`__VA_ARGS__`-Dispatcher
   im Makro funktionierte unter g++, aber NICHT unter dem MSVC-Legacy-
   Präprozessor (C2065 'mrb' im Install-Scope) → Dispatcher entfernt,
   strikt 2-argumentig. Merksatz: keine __VA_ARGS__-Arg-Zähl-Tricks in
   Headern, die MSVC bauen muss.
3. **`M_PI` unter MSVC:** `src/RgssUI.cpp` definiert M_PI jetzt selbst
   (MSVC braucht sonst `_USE_MATH_DEFINES`).
4. **Qt-Includes im Namespace (Editor-Build-Totalschaden):**
   `QtDatabaseDialog.cpp` hatte `#include <QPainter>/<QMouseEvent>/<cmath>`
   INNERHALB von `namespace qt_editor {` → MSVC parste danach alle Qt-Header
   als `qt_editor::*`, >100 Kaskadenfehler (C1003). Includes nach ganz oben
   verschoben. Regel: KEINE Includes hinter `namespace qt_editor {`.
5. **`QtTilesetGridWidget`:** Header nutzte nicht-existentes `mTiles`
   (`tileCount()` inline) → jetzt `mTilesX * mTilesY`; fehlende Deklaration
   von `tileAt()` ergänzt (+`#include <QPoint>`); doppelten `tileCount()`-
   Body im .cpp entfernt (C2084).

6. **Linker-Verwechslung (CI-Lauf 3):** `GameMap::IsPassableWithRadius
   (pos, radius, dirBit)` (3-Arg-Overload aus Paket 1) war im Header deklariert
   und von `GamePlayer::Update` benutzt, die Definition in Game.cpp fehlte
   aber (beim Staging verloren gegangen) → LNK2019. `-fsyntax-only` erkennt
   das nicht! Neu in Game.cpp implementiert: gleiche Abtaststruktur wie die
   2-Arg-Version, aber `dirBit` wird an JEDEM Samplepunkt durchgereicht
   (Kanten-Flags/passage4dir greifen so auch, wenn der Spielerkreis sie
   streift). Lokal per `nm -C Game.o` verifiziert, dass BEIDE Overloads
   definiert sind. Merksatz: nach dem Syntaxcheck neu eingeführte
   Member-Definitionen kurz mit `nm -C` auf Vorhandensein prüfen.

7. **Qt-Download im Workflow (CI-Lauf 4):** kein Codefehler — der
   aqt-Download von Qt 6.9.1 brach mit `Bad7zFile` bei qtdeclarative ab
   (korruptes Archiv vom Windows-Mirror). Workflow gehärtet:
   `aqtinstall==3.1.*` gepinnt, Download auf qtbase/qtsvg/qttools/
   qttranslations beschränkt (`--modules` + `--archives`; qtdeclarative
   brauchen wir nicht — Editor nutzt nur Widgets/OpenGLWidgets aus qtbase)
   und 1 Retry mit aufgeräumtem Zielverzeichnis bei Fehlschlag.
   WICHTIG: Bot darf KEINE Workflow-Dateien pushen (fehlende workflows-
   Permission) → gehärteter Stand liegt als Vorlage in
   **`ci/Main.yml.template`**; Maintainer muss ihn nach
   `.github/workflows/Main.yml` kopieren (oder im Web-Editor einfügen),
   sonst läuft der CI weiter mit der alten Qt-Installationsroutine.
   Nachschlag (CI-Lauf 5+6): Die „packages not found"-Meldung lag NICHT an
   qtbase als ungültigem Modulnamen (Fehldiagnose!), sondern daran, dass
   **aqt 3.1.x das Qt-6.9.1-Repo-Format („extensions") nicht auflösen kann**
   — auch qttools schlug fehl. 3.3.0 fand die Archive dagegen problemlos;
   dort versagte nur py7zr am qtdeclarative-Entpacken. Finale Form:
   `aqtinstall==3.3.*` + `--external 7z` (systemeigenes 7-Zip statt py7zr
   — etablierte Umgehung der Bad7zFile-Falle) + zweistufig: erst
   `--modules qttools --archives qtbase qttools opengl32sw d3dcompiler`,
   Fallback = Vollinstallation ohne Filter.

Merksatz für die nächsten Pakete: nach jedem Push **sofort CI grün machen**
(4 Commits waren ungetestet gestapelt).

---


## PAKET 8 — Player & Spielansicht (Feinschliff nach XP-Paritaet) 🔧
Adressiert die Nutzerfrage: „Was muss man unbedingt aendern — Player,
Editor, Spielansicht?"

- [x] **Vollbild (KRITISCH, war komplett wirkungslos):** `Window` kann
  jetzt `SetFullscreen(bool)/ToggleFullscreen()/IsFullscreen()` (SDL
  `_DESKTOP`-Vollbild, Foreign-Qt-Host = sauberer No-op, Fehler geloggt)
  + `SetSize`. Der Player wendet endlich BEIDE Quellen an:
  `project.json "fullscreen"` UND neues CLI-Flag `--fullscreen`; dazu
  **Alt+Enter-Toggle im Engine::Update** (XP-Verhalten, beide Builds —
  SDL-Player und Editor-/Qt-Pfad).
- [x] **Seitenverhaeltnis der Spielansicht (KRITISCH):** Der RGSS-Canvas
  (640x480) wurde bisher linear aufs volle Fenster gestreckt — auf einem
  16:9-Standardfenster (1280x720) in die Breite gezerrt (x-Faktor 2,0 /
  y-Faktor 1,5 = Kacheln nicht mehr quadratisch). `RgssUI::Render`
  abbildet jetzt auf den groessten zentrierten 4:3-Ausschnitt
  (glViewport-Letterbox); `RgssRenderer::SetClip` rechnet Scissor korrekt
  in denselben Bereich um (view-Offset, framebuffer-absolut). Sichtbar
  bleibt die 3D-Szene in den Seitenbereichen (bewusst: „2D-Welt auf
  3D-Unterlage", zur Engine-Identitaet passend und nicht schwarz
  zugekleistert).
- [x] **Charaktere als XP-Sprites statt Farb-Cubes (SICHTBARSTER
  Schritt):** PlayMode rendert Spieler/Events jetzt als kamerazugewandte
  (Billboard-)Quads mit dem XP-Spritesheet aus
  `Graphics/Characters/<Name>` (4 Richtungen x 4 Laufphasen; Zeile =
  Richtung 2/4/6/8, Spalte = Laufphase, 0 = Standbild, Takt ~0,13 s wie
  XP@40fps). Quellen: Party-Leader (`Game_Actor#graphicName` ←
  Datenbank-`characterName`) fuer den Spieler, AKTIVE Event-Seite
  (`graphicName`, `graphicIndex`, `walkAnime`, `stepAnime`) fuer NPCs —
  Laufzeitaenderungen per Event-Befehl 320 (Charaktergrafik aendern)
  greifen sofort. `graphicIndex > 0` adressiert 8er-Sheets (4x2
  Unterbloeecke a 4x4, VX-Ace-Stil). Weltgroesse = Framepx/32
  (XP-Tile-Fuss), Busch-Tiles (GameMap::IsBushAt) zeichnen die untere
  Haelfte mit 45% Alpha, Transparenz-Befehl 208 wirkt auf den Spieler.
  Negativ-Fall gecacht (kein Lade-Spam); ohne Datei bleibt die bewaehrte
  Quader-Darstellung (gruen Player / farbige NPCs + Bob) als Rueckfall
  erhalten. Texturen/Quads werden gecacht (kein Per-Frame-Upload).
- [ ] **Editor-QA-Liste (beim naechsten Windows-Lauf abhaken):**
  Playtest-Knopf → Player-Start dauert?, Kartenliste doppelklick →
  Map-Tab wechselt?, Datenbank-Tab Sounds (SoundTestDialog ok),
  Event-Seiten-Editor: Grafikauswahl, Move-Route-Dialog.
- [x] **Editor-Bequemlichkeit:** Zuletzt geoeffnete Projekte im
  Datei-Menue waren bereits fertig (QSettings, max 8, Validierung mit
  Warnsymbol, „Liste leeren"). Neu hinzugekommen ist **„Spiel
  exportieren"** (Datei-Menuepunkt + Ribbon-Knopf im Datei-Tab):
  kopiert die Player-exe als `Game.exe` (XP-Anmutung), den Projektordner
  als `Game/` (Top-Level ohne `saves/` und `.git/`), alle neben der
  Player-exe liegenden DLLs (Qt-DLLs ausgenommen – die gehoeren nur zum
  Editor) und eine `LIESMICH.txt` (Start + vc_redist-Hinweis) nach
  `<Ziel>/<Spielname aus project.json>/`. Vorher Speicherfrage
  (Export liest die Festplatte), Ordnername wird windowstauglich
  bereinigt, Existiert-schon-Rueckfrage, Fehlerzaehlung mit
  Log/Meldung. Spielerseite dazu: `ParseProjectPath` im Player erkennt
  neben der exe liegendes `./Game/project.json` automatisch — ein
  Doppelklick auf Game.exe startet ohne Argumente.

## PAKET 9 — Kampfszene (XP-Feinschliff) ✅ ERLEDIGT 2026-07-23

Vorhandenes Fundament: Gegner-Bilder aus Graphics/Battlers/<battlerName>
als Canvas-Pictures, Text-Statuszeilen oben, natives XP-Kampfmenue.

- [x] **Fliegende Schadens-/Heilungszahlen (XP-Popups):** Neuer zentraler
  Hook `BattleSystem::onBattlerHpChanged(const Battler&, int amount)` wird
  aus `Battler::ApplyDamage`/`Recover` gemeldet (effektive HP-Aenderung,
  >0 = Schaden, <0 = Heilung) und deckt damit Angriffe, Fertigkeiten,
  Items UND Kampf-Ereignis-Befehle ab. Die Engine
  (`SpawnBattleFeedbackPopup`) legt an der Position des Ziels (Gegner:
  Verteilungsformel der Battler-Bilder, Bild y=0.30 -> Popup y=0.235;
  Akteure: Party-Statuszeile y=0.10 -> Popup y=0.155) eine farbige Zahl
  ab (Schaden Akteur rotstichig / Gegner weiss-gelb, Heilung gruen „+"),
  die per `MoveScreenText`-Tween (easeOutQuad) nach oben schwebt und mit
  ihrer Lebensdauer (0,95 s) fadet. Hook wird in `Engine::Shutdown`
  geloest.
- [x] **XP-Statusfenster unten (Gesicht + Name + HP-/MP-Balken + K.O.):**
  `GameUI::BattleStatusEntry` (name/hp/maxHp/mp/maxMp/dead/faceName/
  faceIndex) + `SetBattleStatusEntries`/`ClearBattleStatus`; die Engine
  fuellt den Schnappschuss im 0,25-s-Kampfstatus-Tick (Party-Textzeile
  entfernt, Gegner-Zeile oben bleibt). `DrawBattleStatus` (ImGui-Overlay,
  Stil wie MessageWindow) rendert unten eine Leiste mit bis zu 4 Slots:
  Gesicht aus Graphics/Faces/<faceName> (4x2-Sheet per faceIndex, sonst
  Einzelbild; Negativ-Cache bei fehlender Datei), Name (K.O. rot),
  HP-Balken gruen→gelb→rot nach Fuellstand, MP-Balken blau, Zahlen.
  Pfad-/Ordnerseite: `Graphics/Faces/` in `ResolvePicturePathFor` UND in
  `Project::Create` (neue Projekte legen den Ordner mit an);
  `UI.h` bekam Fwd-Dekl `class Texture` + `<memory>/<unordered_map>`.
- [x] **Treffer-Flash/Blink der Gegner-Bilder + Ziel-Blinken beim
  Waehlen:** `ScreenPicture` bekam Flash- (Farb-Blitz mit Staerke-alpha)
  und Blink-Zustand (periodische Alpha-Pulse ~1,9 Hz) samt
  `FlashPicture(id|name, color, duration)` und
  `SetPictureBlinking(name, on)`; Update-/DrawPictures wenden beides an
  (Tint-Mix Richtung flashColor, blinkender Alpha-Puls, ohne ImGui
  No-Op). Die Engine flasht im `onBattlerHit`-Hook: Treffer/Miss weiss,
  Crit orange, Heilung gruen — und laesst im Gegner-Zielmenue
  („Welchen Gegner?“) das Bild des Gegners unter dem Cursor flackern
  (Cursor-Index == Gegner-Index; Aufrauemen an beiden Cleanup-Stellen).
- [x] **Kritische Treffer + Ausweichen (XP-Kampfregel):** Neuer
  Hook-Typ `BattleHitKind{Damage, Crit, Heal, Miss}` (ersetzt den
  HP-only-Hook von Teil 1) + `Battler::ApplyDamage(dmg, kind)` und
  `NotifyMiss()`. Angriff und Schadens-Fertigkeit wuerfeln zuerst
  Ausweichen (5%, Popup „Ausgewichen!“, eigenes Message-Text) und dann
  Crit (1/16, dreifacher Schaden, Popup „KRITISCH! -x“ in Orange,
  groesser). Event-Befehle melden weiterhin Damage/Heal wie gehabt;
  das teilte sich die Verteilung (Popups/Flash) automatisch.
- [x] **battlerHue (Farbton 0..360):** CPU-Pixel-Shift beim Erstladen des
  Battler-Bildes — `Texture::CreateFromRGBA(w, h, rgba)` (neu, ersetzt
  bestehende GL-Textur) + `ApplyHueShiftRGBA` (HSL-Drehung pro Pixel,
  Alpha und graue/farbton-neutrale Pixel bleiben unberuehrt) im
  Picture-Cache-Namespace. `ScreenPicture` bekam `hue`,
  `ShowPicture(...)` einen optionalen Parameter `int hue = 0` (beide
  Ueberladungen — Ruby-Binding und alle Alt-Aufrufer bleiben unveraendert
  kompatibel), `LoadPictureTexture` cached pro (Pfad, Farbton) mit Key
  `path|hue=N` (hue == 0 benutzt weiterhin den reinen Pfad-Key). Die
  Engine reicht `EnemyData::battlerHue` im `$battler`-Block durch.
  Dazu gehoert der Editor: Gegner-Tab des Datenbank-Dialogs bekam den
  XP-Regler **„Farbton" (0..360)** — und die Persistenz-Luecke wurde
  geschlossen: `Enemies.json` speichert `battlerHue` jetzt auch (Laden
  + RGSS `battler_hue` existierten schon, nur Speichern fehlte).

## PAKET 10 — RmlUi-Ablösung der Anzeige ✅ ERLEDIGT 2026-07-23

Nutzer-Strategie (2026-07-23): „wenn alles fertig ist wird RmlUI nicht
mehr benötigt." Umgesetzt: **RmlUi (+ FreeType) ist vollständig aus
Code, Build und third_party entfernt** (~1.900 Dateien gelöscht). Die
gesamte Spielanzeige läuft im GameUI-ImGui-Overlay; `RPGMAKER3D_ENABLE_IMGUI`
ist jetzt Default ON (bei OFF: Draw = No-Op, Logik läuft — dokumentiert).

- [x] **Anzeige-Parität im ImGui-Overlay hergestellt:**
  `MenuWindow::Draw()` (Titel mittig, im Spiel rechts oben — ehemalige
  #menu_box-Position; Einträge als Selectable mit Cursor, deaktivierte
  ausgegraut, Maus-Klick bestätigt; Schnappschuss der Items wegen
  aufbauender onPick-Callbacks) + `GameUI::DrawModalWindows()` mit
  Priorität Menü → `DrawNumberInput()` (Ziffernzeile mit Klammer-Stelle)
  → `DrawNameInput()` (Unterstrich-Cursor); Choices zeichnet schon immer
  `MessageWindow::Draw` im Nachrichtenfenster (Sprecherzeile ergänzt).
  HUD: `GameUI::mHudVisible` + `ToggleHud()/SetHudVisible()`; F9 wird
  direkt im `Engine::Update` abgefragt (Player UND eingebetteter
  Qt-Playtest); `DrawPlayHud` zeigt zusätzlich FPS.
- [x] **RmlUi komplett entfernt:** Dateien `RmlUiSystem.h/.cpp` +
  `third_party/rmlui` (22 MB), `third_party/rmlui_glue`,
  `third_party/freetype` gelöscht; `RPGMAKER3D_ENABLE_RMLUI` nirgends
  mehr referenziert (grep-bewiesen). Engine: Init/Shutdown/Update/
  ProcessEvent/Render ohne mRmlUi; Qt-Widget: RmlUi-Input-Blöcke
  entfernt (Qt-Tasten laufen ohnehin ueber `Input::OnKeyChanged`,
  modale Eingaben ueber `UpdateModalInput` — nichts geht verloren);
  QtEditorWindow (Menü/Ribbon „Spiel-HUD umschalten" →
  `GameUI::ToggleHud`); RubyVM-Bindings `UI.hud_visible[?]`/`native_hud=`
  steuern jetzt `GameUI::SetHudVisible` (kein API-Bruch fuer Skripte).
- [x] **Build & Doku:** CMakeLists (Option + freetype/rmlui-Block,
  Quellen/Link/Defines an 4 Stellen entfernt; IMGUI-Option ON mit
  neuer Beschreibung), ci/Main.yml.template 2× IMGUI=ON (⚠ echter
  Workflow .github/workflows/Main.yml muss vom Nutzer gespiegelt
  werden — Bot pusht keine Workflow-Dateien), README +
  docs/ARCHITECTURE.md + docs/QT-EDITOR.md + Custom.h-Kommentare.
- [x] **NACHTRAG 2026-07-24 — Kritischer Fix: ImGui-Frame-Lebenszyklus
  nachgezogen.** Die PAKET-10-Umstellung hatte ImGui-Aufrufe
  (`ImGui::Begin/GetIO/...`) in allen Draw-Pfaden, aber **nirgendwo
  `ImGui::CreateContext`/`NewFrame`/`Render`** (auch schon im
  Basis-Commit nicht; die CI baute bisher nur mit IMGUI=**OFF**, wo
  Draw ein No-Op ist). Mit IMGUI=ON waere der erste Draw (schon der
  Titelbildschirm) ein NULL-Kontext-Zugriff = Absturz gewesen.
  Jetzt: `Engine::InitImGui/ImGuiBeginFrame/ImGuiEndFrame/ShutdownImGui`
  (geht nur mit dem Define an). Host-unabhaengig (SDL-Player UND
  Qt-GameView): bewusst **kein** SDL-Backend — Eingaben laufen nativ
  (Input/UpdateModalInput), Qt pumpt keine SDL-Events; DisplaySize +
  DeltaTime werden pro Frame manuell gesetzt (Quelle: `mWindow`,
  im Qt-Host via `Window::SetForeignSize` gepflegt). GL3-Backend mit
  eingebettetem gl3w-Loader (kein Loader-Konflikt mit glad, kein
  CMake-Eingriff noetig); `imgui_impl_sdl2.cpp` dafuer aus beiden
  CMake-Quellisten entfernt. Zeichen-Reihenfolge: Szene → GameUI-
  DrawData → RgssUI (dokumentierte Ordnung „Ruby-UI oberste Schicht“
  stimmt jetzt real). Draw-Aufrufe laufen nur bei `mImGuiReady`.
  **Konsequenz:** Erst MIT diesem Fix werden die ImGui-Anzeigen
  (Titel/Messages/Menues/HUD/Battler-Bilder/Kampfstatus/Bildschirm-
  effekte, PAKETe 9–11) ueberhaupt sichtbar — ohne ihn waere jeder
  IMGUI=ON-Build beim Start abgestuerzt.

## PAKET 11 — XP-Bildschirmeffekte sichtbar ✅ ERLEDIGT 2026-07-23

Die Event-Befehle 223/224/225 (Farbton/Blitz/Erschütterung) schrieben
bislang nur in den `ScreenEffects`-Zustand — **gerendert wurde nichts**;
236 (Wetter) war ein reservierter No-op-Kommentar. Jetzt sichtbar:

- [x] **Bildschirm-Farbton (223) + Blitz (224):** `GameUI::DrawScreenEffects()`
  — unsichtbares Vollbild-Fenster ganz unten in der ImGui-Ordnung, also
  ÜBER der 3D-Welt, aber UNTER Menüs/Nachrichten (XP färbt Fenster nicht
  mit). Blitz: `flashColor` mit abklingendem Alpha (Timer 1→0). Farbton:
  Vorzeichen-getrennter Veil — positives Signal legt die Farbe, negatives
  dunkelt ab, Grauanteil legt Sepia. **Dokumentierte Näherung:** XP
  verschiebt pro Kanal −255..+255 + Grau-Mischung; exakte Kanal-Mathe
  bräuchte einen Post-Process-Shader (Szene rendert direkt ins Host-FBO,
  kein Composite-Pass) — mit Alpha-Mischung bewusst angenähert.
- [x] **Erschütterung (225):** Kamera-Jitter im Follow-Cam-Block der
  Engine (funktioniert im Player UND eingebettetem Qt-Playtest),
  Amplitude = `shakePower × 0,10 × (Timer/Duration)` auf den Bildachsen
  x/y — die ganze 3D-Welt zittert, HUD/Menüs bleiben ruhig (wie XP).
- [x] **Wetter (236) — vorher komplett ohne Wirkung:** Zustand in
  `ScreenEffects` (Typ 0 Keins / 1 Regen / 2 Sturm / 3 Schnee, Stärke
  0–9 mit sanfter Rampe in `Update`, 1,5 s), `GameUI::DrawWeather()`
  zeichnet deterministische Partikel (`WeatherHash01`, kein `rand()`
  pro Frame): Regen = schräge Streifen, Sturm = mehr/schneller/schiefer,
  Schnee = treibende Flocken mit Sinus-Drift. Globales Overlay → läuft
  auf der Karte UND im Kampf automatisch. `SetTimeOfDay` bleibt
  bewusst reserviert (kein XP-Befehl).

## PAKET 12 — Kampf-Animationen (XP Waffen-/Skill-/Item-Animation) ✅ ERLEDIGT 2026-07-24

Schliesst den „später“-Vermerk aus PAKET 5: Im Kampf loesten
Angriff/Fertigkeit/Item bislang nur Popups/Flash aus — keine
Animationssequenz. XP-Referenz (Scene_Battle phase4): der Angreifer
spielt seine `animation1_id` (Waffen-Animation) bzw. die der Fertigkeit
am Ziel ab; die Runde wartet sichtbar auf das Ende.

- [x] **Datenbank: `animationId` komplett + Persistenz-Luecken
  geschlossen.** `WeaponData`/`ItemData` hatten das Feld bereits, aber
  es wurde **nie gespeichert** (Weapons.json/Items.json schrieben es
  nicht — Editor-Werte gingen verloren). Neu: `SkillData.animationId`
  (XP `animation_id`; der alte Freitext `animation` bleibt als
  Legacy-Fallback per Namensabgleich). Parse + Save in Database.cpp
  fuer alle drei Typen ergaenzt.
- [x] **Editor:** Skills-Tab „Animations-ID“-Spin (0–999, wie Waffen)
  + der alte Text ist als „Legacy-Animationsname“ weiter editierbar;
  Gegenstaende-Tab „Animations-ID“-Spin ergaenzt.
- [x] **Laufzeit:** neuer Hook `BattleSystem::onBattleAnimation(target,
  animId)` — gefeuert VOR dem Schaden in `ProcessTurn` (Angriff nur bei
  Akteur-Angreifern ueber die ausgeruestete Waffe — XP: Gegner-
  Standardangriff hat keine Sequenz, Ziel-Flash laeuft ueber
  `onBattlerHit`; Schaden-/Heil-Skills; Schaden-/Heil-Items).
  `Game::StartAnimationAtCanvas(animId, x, y)` spielt die Sequenz
  direkt an einer RGSS-Canvas-Position (Refaktor: gemeinsames
  `InitRunningAnimation`; Kartenpfad via Weltprojektion unveraendert).
  Engine verdrahtet Positionen wie Battler-Bilder/Popups (Gegner
  y=0,30, Akteure Statuszeile y=0,90); Sequenz laeuft als
  RGSS-Sprites (z=9999 → ueber Battler-Bildern UND 3D). Timing:
  `BattleState::Action` wartet zusaetzlich auf `IsAnimationPlaying()`
  (XP: naechster Kaempfer erst nach Animations-Ende). SE je Frame ueber
  den vorhandenen `playSeHook`.

**Akzeptanz:** Akteur greift mit Waffe (Animations-ID > 0) an →
Schwung-Sequenz am Gegner-Bild; Fertigkeit spielt ihre Animation am
Ziel; Runde geht erst nach Ende weiter. Ohne hinterlegte Animation
(ID 0/legacy unbekannt) bleibt alles beim alten Popup/Flash-Verhalten.

## PAKET 13 — RGSS-Renderer-Lücken: Font#shadow + Sprite-Wave ✅ ERLEDIGT 2026-07-24

Die beiden letzten dokumentierten RGSS-Render-Grenzen (Zustand und
Bindings lagen längst vor, nur der Renderer ignorierte sie) sind jetzt
geschlossen — damit ist die RGSS-Basisklassen-Oberfläche bis auf die
bewussten Auslassungen (Marshal, snap_to_bitmap, Interpreter 1-7,
events-Hash) render-vollständig.

- [x] **Font#shadow im eingebauten Text-Renderer:** `RgssBmpDrawText`
  zeichnet jetzt bei `font.shadow == true` jedes Zeichen zweimal —
  erst die Schattenkopie (1px Suedost-Offset, schwarz, Alpha =
  min(128, Text-Alpha) wie XP `Color(0,0,0,128)`), dann das Zeichen.
  Mischung laeuft ueber das vorhandene Source-Over-Compositing von
  `BmpPut`. Gleiche Bold/Italic-Flags in beiden Passes.
- [x] **Sprite#wave im Renderer:** `DrawSprite` verzerrt unrotierte
  Sprites bei `waveAmp != 0` jetzt als horizontale Streifen (max.
  4 px hoch, bis 64 Streifen): x-Offset = `waveAmp * sin(wavePhase*pi/
  180 + 2*pi*Quellzeile/waveLength)` — derselbe Zusammenhang, den
  `Sprite#update` (nativ) seit laengerem phasenweise vorantraegt
  (`wave_speed / [2*wave_length, 1].max`). Ehrlich vermerkte Grenzen:
  rotierte Sprites bleiben unverzerrt, der Bush-Weichzeichner bleibt
  im Wave-Pfad ohne Alpha-Fade (Kombination ausserhalb der
  XP-Default-Skripte), `wave_height` bleibt Kompat-Zustand ohne
  Render-Wirkung.
- [x] **Kommentar- und Grenzen-Pflege:** RgssUI.h-Kommentare
  (shadow/wave) auf den neuen Stand gezogen; XP_Scripts-Grenzenliste
  (Rundung 2026-07-23 Teil 2) von „Offen" auf „ERLEDIGT" umgestellt.

**Akzeptanz:** `font.shadow = true` zeigt sichtbare 1px-Schatten unter
RGSS-Text; ein `Sprite` mit `wave_amp > 0` wellt sich sichtbar und
laeuft phasenverschoben weiter (mit `wave_speed`/`wave_length`
steuerbar); ohne Nutzung bleibt alles pixelgleich.

## PAKET 14 — XP-Übergänge (Graphics.freeze/transition an Szenenwechsel) ✅ ERLEDIGT 2026-07-24

Das Graphics-transition-Grundgeruest (Ruby-Bindings, Maske-Laden,
DrawTransition-Shader, Freeze-Snapshot host-sicher am Render-Ende)
lag bereits vor — es fehlte die **Einbindung in die echten
Szenenwechsel**: Kartenwechsel per Transfer-Befehl (201) und
Titel↔Spiel sprangen hart um.

- [x] **Engine-Uebergangs-Arbiter (`RequestTransition`/`UpdateTransitionRequest`):**
  XP-Vorlage `Scene_Map#transfer_player`
  (Graphics.freeze → wechseln → Graphics.transition(10)). Weil der
  Freeze-Snapshot bewusst host-sicher erst am ENDE des naechsten
  Render entsteht (kein Backbuffer-Readback nach Swap — waere
  spec-seitig undefiniert), wartet der Arbiter genau einen Tick:
  Render des Freeze-Ticks zeichnet noch die alte Ansicht + Snapshot
  vom alten Bild, dann Swap + Crossfade (15 Frames ≈ XP-10 bei
  60 fps, ohne Maskengrafik). Neu: `RgssGraphicsHasSnapshot()`-Takt,
  `EventSystem_SetTransferTransitionHandler` (Fallback: bisheriger
  Sofortpfad fuer headless/Tests). Sicherheitsnetze: doppelte
  Anfrage flusht die Vorige statt sie zu verlieren; 30-Tick-Timeout
  wechselt ohne Fade, falls der Render-Takt stockt (kein Haenger);
  im Editor ohne laufenden Playtest sofortiger Swap (kein Overlay
  sichtbar).
- [x] **Verdrahtet:** Transfer-Befehl (201, Position + Karte im Swap),
  Titel „Neues Spiel" (Titel → Karte), Titel „Weiterspielen"
  (Ladebildschirm → Karte). `Graphics.transition(dauer, datei,
  vague)` aus Ruby laeuft unveraendert weiter (Maskengrafik aus
  `Graphics/Transitions/` via RgssResolveGraphic).
- [x] **Nebenbefund erledigt:** `ClearAll` hat Freeze-Snapshot- und
  Masken-Texturen bislang nicht freigegeben (Leck pro Playtest-Stopp)
  — jetzt glDeleteTextures vor dem State-Reset.
- [x] **Ehrliche Grenzen:** Rueckkehr zum Titel (`ReturnToTitle`) und
  Kampfbeginn bleiben ohne Crossfade: `SetPlaying(false)` raeumt den
  RGSS-Kanal per ClearAll hart ab (degradiert glatt zum No-op, ist so
  abgesichert), und ein Kampf-Fade brauchte eine Setup-Verschiebung
  — bewusst ausgenommen, dokumentiert. Der Editor-Playtest-Startknopf
  (Qt) startet unveraendert sofort.

**Akzeptanz:** Tuer-/Portal-Event mit Transfer-Befehl fadet sichtbar
weich von alter zu neuer Karte (≈0,25 s), Titel-Neustart fadet in die
Startkarte; ohne Assets zusaetzlich noetig (Standard-Crossfade).

## PAKET 15 — Kampfszene: XP-Sieg-Sequenz ✅ ERLEDIGT 2026-07-24

Drei Feinschliff-Luecken der Kampfszene gegenueber XP
(`Scene_Battle#start_phase5` / Gegner-Collapse): die Datenbank-ME
`battleEndMe` wurde zwar geladen/gespeichert, aber **nie abgespielt**;
die Victory-Phase endete starr nach 2 s (unabhaengig davon, ob die
Sieg-/EXP-/Level-Up-Nachricht noch offen war oder schon bestaetigt);
tote Gegner verschwanden hart im selben Frame.

- [x] **Sieg-ME:** neuer zentraler Hook `BattleSystem::onVictoryMe` —
  feuert in `CheckVictory` mit `System().battleEndMe`; Engine injiziert
  ihn EINMAL in `Initialize` (`PlayEventAudio(name, 2=ME)`). Bewusst
  nicht ueber `onVictory` geloest: die kampfstart-seitigen Setup-Pfade
  (Event-Befehl, StartBattleByTroop, RubyVM) ueberschreiben onVictory/
  onMessage regelmaessig — der Audio-Hook bleibt davon unberuehrt.
- [x] **Ergebnis-Quittierung statt Starr-Timer:** neuer Hook
  `BattleSystem::isMessageBusy` (Engine: `GameUI::Message().IsBusy()`).
  Victory endet jetzt, sobald die Nachricht quittiert ist (Mindest-
  Darstellzeit 0,6 s; 15 s Sicherheitsnetz ohne/haengenden Hook — statt
  der starren 2,0 s). XP-Verhalten: Ergebnisfenster wartet auf Eingabe.
- [x] **XP-Collapse (Todes-Fade):** tote Gegner ($battler-Bilder) werden
  nicht mehr sofort entfernt, sondern faden ueber ~0,45 s weich aus
  (`TweenPictureOpacity` auf die neu gemerkten Picture-IDs), erst dann
  endgueltig weg. Steuerzustand `mBattlerDying`/`mBattlerPicIds` in der
  Engine; alle drei Cleanup-Pfade (Kampfende, Titel, Geist-Reset)
  leeren ihn mit.
- [x] Shutdown nullliert die neuen Hooks (onVictoryMe haelt this).

**Akzeptanz:** Letzter Gegner faellt → Collapse-Fade des Bildes +
„001-Victory01" (oder Projektwahl) spielt → Sieg-/EXP-/Level-Up-Text
bleibt stehen, bis der Spieler bestaetigt — dann erst EXP/Gold-Mechaik
abschliessen (SyncBack/onVictory) und Kampfende. Genau wie XP, nur
rund.

## PAKET 16 — Move-Routen: XP-Vervollstaendigung ✅ ERLEDIGT 2026-07-24

Der Routen-Umfang deckte bisher nur die Hälfte der XP-`RPG::MoveCommand`-
Codes ab (Gehen/Drehen/Warten); Sprünge, 90°/180°/Zufalls-/Spieler-
Drehungen, Schalter, Tempo/Häufigkeit, Durchgehbarkeit/Transparenz,
Grafikwechsel, SE und Script fehlten komplett — und die zwei Parser
(Befehl 209 + Custom-Seitenroute) waren doppelt gepflegte Kopien.

- [x] **Zentraler Parser:** `EventSystem_ParseMoveRouteText` (freie
  Funktion, deklariert in EventSystem.h) — EINE Quelle für den Event-
  Befehl 209 UND die Custom-Seitenroute (`StartCustomRoute`).
  Rueckwaertskompatible Tokens (siehe docs/EVENTS-XP.md):
  `U D L R F T A X TD TL TR TU W(n)` + neu `B J(dx,dz) R90 L90 T180 TX
  TT TA S+id S-id V(n) Q(n) H1 H0 P1 P0 G name[,idx] E name SC <rest>`.
  `SC` frisst den Zeilenrest (letzter Schritt), `G`/`E` das Folgetoken.
- [x] **Enum auf echte XP-Codes korrigiert:** `Random` 10→9,
  `TowardPlayer` 29→10, `AwayFromPlayer` 30→11 (kollidierten sonst mit
  den neuen `ChangeSpeed`/`ChangeFrequency` 29/30 — Switch-Doppelcase).
  Neue Stufen 13/14, 20–22, 24–30, 35–39, 42/43; `MoveRouteStep` um
  `param2` (Sprung-dz) und `text` (Grafik/SE/Script) erweitert.
- [x] **Laufzeitfelder am MapEvent:** `moveSpeedRt`, `moveFrequencyRt`
  (beeinflussen jetzt Schrittpausen: Tempo 3 == bisheriges festes
  0,05 s; Loop-Pause über Frequenz), `through` (Zustandskompat.),
  `transparent`, `routeGraphic/routeGraphicIndex` (Grafik-Override);
  Schalter-Schritte wirken auf `Game::Switches()`, SE über den
  Audio-Hook (kind 3), Script über `s_scriptRunner`.
- [x] **3D-Renderer konsumiert die Overrides** (Engine.cpp
  Char-Pass): `routeGraphic/routeGraphicIndex` schlagen die Seitengrafik,
  `transparent` setzt Alpha = 0 (Sprite UND Quader-Rueckfall).
- [x] **Editor-Routendialog** (QtEventEditorDialog): 19 neue Schritte,
  generisches Argumentfeld (Sprung/Schalter-ID/Tempo/Haeufigkeit/
  Grafik/SE/Script) neben dem Warten-Spin; Serialisierung nicht mehr
  per fragilem Anzeigetext-Match, sondern kanonisches Volltoken in der
  `Qt::UserRole` jedes Eintrags (verlustfrei hin und zurück; G/E/SC
  werden beim Laden korrekt wieder zusammengesetzt).

**Bewusst offen (XP-Rest):** Anime-Flags 31–34 (walk/step-Anime-Override
im Laufzeit-Renderer), Opacity/Blend 40/41, Async-Script 44/45; `through`
ist Zustandskompatibilität (Routenfuehrung prüft ohnehin keine
Kollision). Sprung ist wie bisher ein harter Positions-Sprung ohne
Parabel-Animation.

**Akzeptanz:** Route „R R L B J(1,0) R90 T180 TX TT W20 S+5 V5 Q2 H1
H0 P1 P0 G 001-Fighter01,2 E 057-Right02 SC <code>" wird aus Befehl 209
UND aus der Seiten-Autonomieroute identisch geparst und ausgeführt;
Editor zeigt jeden Schritt lesbar und schreibt ihn unveraendert zurueck.

## PAKET 17 — Kampf-Zustaende (States): XP-Verhalten im Kampf ✅ ERLEDIGT 2026-07-24

Die Infrastruktur existierte komplett (States-Tab im Editor, States.json
Load/Save, `GameActor.states` inkl. Ruby-Bindings, Event-Befehl 313) —
aber **nichts davon wurde im Kampf ausgewertet**: Battler kannten keine
Zustaende, Restriktionen kamen nie zum Tragen, Gift machte keinen
Schaden, Skills konnten keine Zustaende verhaengen.

- [x] **Battler-Laufzeitmodell:** `states` + `stateTurns` (Setup kopiert
  aus der Party, SyncBack am Kampfende; Tod loescht alle Zustaende wie
  XP; EnemyRecoverAll ebenfalls). Helfer: `HasState/AddState/RemoveState`,
  `CurrentRestriction()` (hoechste Prioritaet gewinnt, XP),
  `TotalHpDrainRate()`, `MostSevereStateName()`.
- [x] **Restriktionen (VX-Ace-Numerierung des Editors):** 4 „kann sich
  nicht bewegen" -> Zug entfaellt mit Meldung (kein Befehlsfenster);
  1/2/3 = Zwangsangriff auf Feindseite/beliebige/eigene Seite
  (ueberschreibt die Wahl, XP auto-battle).
- [x] **Schlupfschaden (XP slip_damage):** `hpDrainRate` x MaxHP am
  eigenen Zug des Vergifteten, inkl. Tod durch Gift (Cleanup wie XP).
- [x] **Auto-Entfernung:** Timing „Nach Aktion" (1) am eigenen Zug,
  „Rundenende" (2) aller Kaempfer beim Rundenwechsel, jeweils nach
  Ablauf von `holdTurn` Runden, mit Meldung.
- [x] **Skill-Zustaende:** `SkillData.plusStates/minusStates` (XP
  plus/minus_state_set) — Verhaengung nur bei Treffer mit Wurf gegen
  die Resistenz-Raenge A..F (100/80/60/40/20/0 %) aus dem neuen
  `stateRanks`-Feld an ActorData/EnemyData (fehlt = C, XP-Default);
  Heilung (Esuna-Art) immer sicher. Persistenz + Editor-Felder
  (Skills: zwei ID-Listen; Akteure/Gegner: „ID=Grad"-Textfeld).
- [x] **Kampfende:** `removeAtBattleEnd`-Zustaende loesen sich auf
  (XP battle_only), persistente (Gift) begleiten den Akteur — werden
  jetzt auch im **Spielstand** gespeichert/geladen (`"states":[ids]`,
  Rueckwaertskompatibel optional).
- [x] **Event-Befehl 333** (Gegner-Zustand aendern) war Log-Stub —
  arbeitet jetzt auf dem Battler-Modell (Meldungen inklusive).
- [x] **HUD:** hoechstpriorisierter Zustandsname im XP-Statusfenster
  (Akteure) und in der Gegner-Zeile.
- [x] **Demo:** Fallback-Skill „Giftstich" (verhaengt Poison),
  Krieger lernt ihn ab Level 3.

**Bewusst offen (XP-Rest):** Items mit Zustands-Effekten (Daten+Editor),
Gegner-Skill-Auswahl (XP actions-Tabelle — Gegner haben nur
Standardangriff), Stat-Raten der Zustaende (maxhp/str/... rate —
unser StateData-Modell hat diese Felder nicht), Waffen-Zustaende,
313 zur Kampf-Laufzeit syncen (wirkt erst ueber Setup beim
naechsten Kampfbeginn), Gift-Schaden beim Map-Laufen (XP schadet
pro Schritt auf der Karte).

**Akzeptanz:** Giftstich trifft -> Ziel „erleidet Poison" (Chance nach
Rang) -> verliert jede eigene Runde 5 % MaxHP mit Meldung; Schlaf
(restriction 4) -> Zug entfaellt „kann nicht handeln (Sleep)";
Verwirrung (1..3) -> automatischer Angriff; Haltezeit abgelaufen oder
Kampfende -> Zustand loest sich mit Meldung; Statusname steht im
Statusfenster.

## PAKET 18 — Gegner-Skilltabelle (XP RPG::Enemy.actions) ✅ ERLEDIGT 2026-07-24

Gegner konnten nur den Standardangriff auf einen zufaelligen Akteur —
die XP-Verhaltenstabelle (Basis-Aktionen/Fertigkeiten mit Bedingungen
und Rating) fehlte komplett (Daten, Engine, Editor).

- [x] **Datenmodell:** `EnemyData::Action` (kind 0=Basis/1=Fertigkeit;
  basic 0 Angriff 1 Verteidigen 2 Flucht 3 Nichtstun; skillId; rating
  1..10; Bedingungen Runde turnA+turnB*x, eigene HP <= hpBelow %,
  hoechstes Party-Level >= level, Schalter switchId). Load/Save in
  Enemies.json (Objekt-Array, robust optional — alte Projekte lesen
  sich unveraendert); Editor-Unterformular im Gegner-Tab (Liste mit
  lesbarer Zeile, Hinzufuegen/Entfernen, Felder mit Write-Through,
  Art-Umschaltung aktiviert Basis-/Fertigkeits-Combo).
- [x] **Engine-Auswahl (`MakeEnemyAction`, XP Game_Enemy#make_action):**
  alle Bedingungen erfuellt -> Lostopf; XP-Regel: nur Eintraege mit
  rating > Tabellenmaximum - 3 ziehen, gleichverteilt. Skills nur, wenn
  der Gegner sich die MP leisten kann (sonst nicht verfuegbar, XP
  usable?). Gegner OHNE Tabelleneintraege behalten exakt das bisherige
  Verhalten (Standardangriff). Sind alle Eintraege durch Bedingungen
  gesperrt, tut der Gegner nichts (XP).
- [x] **Ausfuehrung:** Angriffe/Skills laufen ueber die bestehenden
  Pfade — inklusive Skill-Animation, MP-Kostenabzug UND PAKET-17-
  Zustands-Effekte (vergiftende Gegner-Skills wirken jetzt korrekt);
  Zielwahl nach Scope (Schaden = zufaelliger Akteur, Unterstuetzung =
  zufaelliger eigener Trupp-Mitstreiter / selbst). Verteidigen nutzt
  den Guard-Pfad (halbierter Schaden bis zur naechsten Aktion).
- [x] **Gegner-Flucht (basic 2):** neues `Battler::escaped` — zaehlt
  fuer den Sieg wie tot, feuert aber KEIN onEnemyDefeated (kein
  Collapse-Fade) und bringt kein EXP/Gold (beides XP).
- [x] **Demo:** Fallback + SampleProject — Bat wirkt „Giftstich"
  (PAKET 17, Skill 3) mit rating 4 sobald HP <= 80 %, verteidigt
  gelegentlich (rating 3); Slime bleibt purer Angreifer.

**Bewusst offen:** seltene XP-Feinheiten (force_action 339 ignoriert
die Tabelle weiterhin; mehrere Aktionen pro Runde pro Gegner, falls
ein RPG-Projekt das nutzt); Rating-Auswahl folgt exakt der XP-Regel
max-3.

**Akzeptanz:** Bat mit voller HP greift an; faellt unter 80 % HP, kommt
„Giftstich" in den Lostopf (Ziel vergiftet -> PAKET-17-Kette:
Meldung + Schlupfschaden + Status im Fenster); verteidigt zwischendurch
(halbierter Schaden); Editieren im Gegner-Tab speichert eine
`"actions":[{...}]`-Zeile, die nach Reload identisch wieder erscheint.

## PAKET 19 — XP `Game_Event` + `$game_map.events`-Hash ✅ ERLEDIGT 2026-07-24

`rb_game_map_events` lieferte bewusst einen **leeren Hash** („NPCs laufen
nativ ueber das EventSystem") — XP-Skripte konnten Events damit nicht
lesen oder steuern (`$game_map.events[7].moveto(x, y)` scheiterte).

- [x] **Native `Game_Event`-Klasse:** Wrapper nach dem Game_Actor-Muster
  (nur `@ev_id`/`@map_id` als Ivars, native Aufloesung frisch je Aufruf
  ueber `EventSystem::GetEvent` — vektor- und kartenwechselfest; nicht
  aufloesbare Events liefern nil statt zu crashen). Methoden: `map_id`,
  `id`, `valid?`, `name`, `x`, `y` (XP 2D = ev.z), `direction`,
  `through`/`through=`, `transparent`/`transparent=` (PAKET-16-
  Laufzeitfelder — Move-Routen und Skripte teilen sich jetzt denselben
  Zustand), `move_speed`/`move_speed=` (clamp 1..6), `moveto(x, y)`
  (Semantik von Event-Befehl 202 inkl. Blick-Reset nach unten, XP),
  `erase`, `erased`/`erased?`, `refresh` (= RefreshAllPages).
- [x] **`$game_map.events`:** echter Hash `{id => Game_Event}` mit
  Dauer-Cache auf der Game_Map-Instanz (gleiche Objekte wie XP, Ivars
  ueberlebensfaehig); bei abweichender Karten-ID Neuaufbau. Events
  ohne Seiten (Platzhalter) uebersprungen, erased bleibt drin (XP).
- [x] Kein Stub-Pfad betroffen (keine neuen oeffentlichen
  RubyVM-Methoden); Prelude unveraendert (definiert kein Game_Event —
  keine Kollision; `RPG::Map#events = {}` ist die Daten-Klasse).

**Bewusst offen (XP-Rest von Game_Event):** `start`/`unlock`, Trigger-
Logik und Interpreter-Steuerung aus Ruby heraus (laeuft nativ), volle
Game_Character-Oberflaeche (bush_depth, screen_z, animation usw. —
Klasse leitet bewusst von Object ab, solange Game_Character nicht
existiert).

**Akzeptanz:** `SC $game_map.events[1].moveto(3, 4)` im Routen-/Event-
Script versetzt das Demo-Event sichtbar; `$game_map.events[1].through =
true` wirkt identisch zum Routen-Token H1; nach Kartenwechsel liefert
`$game_map.events` den Hash der neuen Karte.

## PAKET 20 — Item-Zustaende + Menue-Heilung (XP plus/minus_state_set) ✅ ERLEDIGT 2026-07-24

Nach PAKET 17 (Kampf-Zustaende) fehlten die XP-Item-Effekte: Items
konnten weder Zustaende verhaengen noch heilen — ein Antidot war
unmoeglich; und die Menue-Nutzung kannte nur HP/MP.

- [x] `ItemData.plusStates/minusStates` + Persistenz (Load/Save) +
  Editor Felder im Items-Tab (wie im Skills-Tab).
- [x] **Fund:** `ItemData.scope` wurde vom Editor gesetzt, aber NIE
  gelesen/geschrieben — die Reichweite ging bei jedem Save verloren.
  Mitfix: scope jetzt in Items.json (Parse + Save, clamp 0..7).
- [x] **Kampf:** `ApplySkillStates` zum generischen `ApplyStateSets`
  refaktoriert (Skill ruft es weiterhin); beide Item-Pfade (Schadens-
  wie Heil-Item) wenden die Sets am Ziel an (verhaengen mit
  Resistenz-Wurf, heilen sicher — XP).
- [x] **Menue:** Benutzbar-Gate um Zustands-Effekte erweitert
  (Item OHNE Heilwerte aber MIT minusStates = benutzbar, z. B.
  Gegengift); Anwendung auf `GameActor.states` mit Meldung
  (verhaengen im Menue direkt wie XP-Inventar-Items; Heilung ebenso).
  Meldetext faellt ohne Heilwerte sauber auf „benutzt <Item>." zurueck.
- [x] **Demo:** „Gegengift" (heilt Poison, id 3) in Fallback-DB und
  SampleProject.

**Bewusst offen:** Wiederbelebungs-Scopes (OneAllyDead/AllAlliesDead —
Zielwahl lebender Mitglieder bleibt vorgegeben), Parameter-Boni von
Items; Menu-Skills (Skills aus dem Fertigkeits-Menue auf der Karte)
sind ein eigener Block.

**Akzeptanz:** Bat vergiftet einen Akteur (PAKET 18) -> Menue oeffnen,
„Gegengift" auf den Vergifteten -> „ist nicht mehr Poison" mit
Entscheiden-SE; Anzahl sinkt; Zustand bleibt weg (Spielstand-haeltig
seit PAKET 17).

## Arbeitsregeln (für Agenten-Sessions)

**Strategie (Nutzer, 2026-07-23):** RmlUi war eine Uebergangsloesung und
wurde mit **PAKET 10 vollständig entfernt** — die gesamte Spielanzeige
läuft im GameUI-ImGui-Overlay (Default ON), XP-Fenster im nativen
RgssUI-Canvas. Neue Anzeige-Features nur in diesen beiden Pfaden.

1. **Nur** Branch `arena/019f6f2a-engine-program`; vor jedem Commit:
   `git log --oneline -1` + `git fetch origin arena/019f6f2a-engine-program -q`
   + HEAD==FETCH_HEAD prüfen (stiller Reset kam vor!).
2. Nach jedem Paket: compilernahe Checks
   (`g++ -std=c++17 -fsyntax-only -Iinclude -Ithird_party ... <geänderte .cpp>`,
   Qt-Dateien: Python-Tokenizer-Brace-Check, **beide ImGui-Varianten** –
   mit und ohne `-DRPGMAKER3D_ENABLE_IMGUI`), dann Commit (DE, ausführlich) + Push.
3. `QL(`/`QStringLiteral(` nur mit Literalen.
4. mruby-Zweig lokal nicht baubar → jede neue RubyVM-/Rgss-Methode auch im
   `#else`-Stub spiegeln; mruby-APIs gegen 4.0.0-Header verifizieren.
5. CI: Bot kann Workflow nicht dispatchen → User bitten: Actions →
   „Build Windows EXE" → Run workflow (Branch + SHA nennen).
