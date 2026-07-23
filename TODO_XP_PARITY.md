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
**Status: OFFEN**

**Wo:** `qt_editor/QtMapTab.cpp` (Canvas `mousePressEvent` ~Z. 180,
`onPaint` ~Z. 95-165: dort wird pro Zelle `tileColor(id)`-Vorschau gezeichnet).

**Umsetzung:**
- Enum `MapTool { Pen, Rect, Ellipse, Flood }` in QtMapTab.h; 4 exklusive
  QToolButtons in der Tab-Toolbar (neben den Ebenen-Knöpfen `mModeBtns[4]`).
- Pen = bisheriges Verhalten. Rect/Ellipse: Drag → Vorschau-Rahmen,
  Loslassen → Fläche mit aktivem Tile füllen (Ellipse: Mittelpunktsformel).
- Flood: Klick → klassischer Flood-Fill (Stack, kein Rekursions-Overflow;
  Karten bis 500×500 → iterative std::queue) auf gleiche ID, nur aktive Ebene.
- Undo: Es gibt `src/CommandHistory.cpp` — prüfen, ob Tile-Edits schon als
  Command laufen; Form-Füllungen als EIN Command bündeln (Diff-Liste alter
  IDs), damit Strg+Z nicht 400 Einzelzellen zurückrollt.
- XP hat zusätzlich Auswahl/Kopieren — als PAKET 3b optional, NICHT blockierend.

**Akzeptanz:** Rechteck aus Wasser-Tile ziehen füllt exakt das Rechteck;
Strg+Z macht es in einem Schritt rückgängig.

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
- [ ] **Folge: Sprite-Busch-z-Maske + Terrain 4..7:** Bush-Flag auch auf
  CHARAKTER-Sprites anwenden (nur untere Haelfte alpha) und Semantik
  fuer die restlichen Tags 4..7 belegen (Vorschlag: Gegner-Encounter
  pro Tag, Brueken-Fahrzeug-SE).
- [x] **Player-Export vc_redist-Hinweis (ERLEDIGT 2026-07-23):** README-
  Abschnitt „Fehlersuche: Player/Editor startet nicht (Windows)" erklärt
  die Schließt-sofort-Historie (vc_redist) + Vorschläge (installieren,
  statisch linken, engine.log [FATAL-STARTUP] lesen).
- [ ] **XP_Scripts/ schrittweise lauffähig:** die 90 Original-Skripte gegen
  unsere RGSS-Implementierung laufen lassen; jedes noch-fehlende API hier
  eintragen. Bekannte dokumentierte Grenze: `load_data`/Marshal (rxdata).
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
