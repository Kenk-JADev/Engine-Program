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
das RmlUi-HUD-Toggle). Engine-intern über die RGSS-Fensterschicht:
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

**Bei uns:** F9 = HUD-Toggle (QtGameViewWidget.cpp ~Z. 102 → RmlUi toggle).
Lösung wie XP: F9 NUR im Playtest den Debug-Inspektor, HUD-Toggle auf andere
Taste ODER Debug-Fenster als eigenen Toggle DANEBEN (F9 bleibt Debug,
HUD-Toggle z.B. F8 — in QtEditorWindow.cpp + Hilfetexten anpassen:
QtEditorWindow.cpp Z. 145, 320, 1431!).

**Datenquelle:** Schalter/Variablen-Werte zur Laufzeit stehen im Game-System
(Suche: `mSwitches`, `mVariables` in src/Game.* — `Game_System` hält sie;
Namen aus `Database::Get().System().switches/variables`).

**Darstellung:** als RmlUi-Panel im „game"-Kontext ODER als natives
RGSS-Overlay (RgssUI existiert seit d548352; dort wäre ein Debugfenster 20
Zeilen). Wichtig: nur aktiv, wenn Playtest-Flag gesetzt (Editor-Start /
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
   Interpreter-Warte-Flag aufheben. Waffen-/Skill-Animationen im Kampf später.

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
- [~] **XP_Scripts/ schrittweise lauffähig (IN ARBEIT, Stufe 1 am
  2026-07-23):** die 90 Original-Skripte gegen unsere RGSS-Implementierung
  laufen lassen; jedes noch-fehlende API hier eintragen.
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
      Objekte. **Noch offen:** Game_Troop- + Game_Screen-Bruecke (Kampf-
      Zustand/Pictures/Flash); Game_Actor-Equip-MUTATOREN (change_equip),
      Name-Input-Verdahtung (name= liegt aktuell nur auf der Runtime);
  (g) **TEIL 2 erledigt 2026-07-23: Game_Party-XP-Vervollstaendigung:**
      nativ neu: item_number/weapon_number/armor_number (XP-Namen,
      Aliase auf die count-Bindings), has_item, all_dead?; interne
      __actor_ids/__item_ids/__weapon_ids/__armor_ids. Prelude-Reopen:
      actors (Game_Actor-Instanzen aus $game_actors = XP-Identitaet),
      actor(id), items/weapons/armors (ID-Listen-Naeherung statt Hash —
      dokumentiert), max_level, average_level, item_can_use? (count +
      consumable via Bruecke), movable?.
  (h) Scene_*-Framework: XP Main.rb treibt `while $scene != nil` —
      unsere Engine ownet den Frame-Loop; Bruecke = $scene bereitstellen
      + Scene.update pro Frame aufrufen (Architektur-Entscheid: Opt-in-
      XP-Modus, Default bleibt nativ);
  (i) interpreter 1-7: bewusst NICHT uebernehmen (unser nativer
      EventSystem-Interpreter bleibt fuehrend); Windows_*/Sprite_* der
      Originalskripte sind gegen RgssUI weitgehend API-kompatibel und
      werden beim ersten Trockenlauf evaluiert.
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


## Arbeitsregeln (für Agenten-Sessions)
1. **Nur** Branch `arena/019f6f2a-engine-program`; vor jedem Commit:
   `git log --oneline -1` + `git fetch origin arena/019f6f2a-engine-program -q`
   + HEAD==FETCH_HEAD prüfen (stiller Reset kam vor!).
2. Nach jedem Paket: compilernahe Checks
   (`g++ -std=c++17 -fsyntax-only -Iinclude -Ithird_party ... <geänderte .cpp>`,
   Qt-Dateien: Python-Tokenizer-Brace-Check), dann Commit (DE, ausführlich) +
   Push.
3. `QL(`/`QStringLiteral(` nur mit Literalen.
4. mruby-Zweig lokal nicht baubar → jede neue RubyVM-/Rgss-Methode auch im
   `#else`-Stub spiegeln; mruby-APIs gegen 4.0.0-Header verifizieren.
5. CI: Bot kann Workflow nicht dispatchen → User bitten: Actions →
   „Build Windows EXE" → Run workflow (Branch + SHA nennen).
