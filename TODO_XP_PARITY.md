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
**Status: OFFEN**

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
**Status: OFFEN — größtes Paket**

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

## PAKET 6 — Kleinigkeiten (Danach)
- [ ] **XP-Importdialog („Material base"):** Asset-Browser vorhanden
  (`qt_editor/QtAssetBrowserDock.*`); XP kann zusätzlich Datei→Projektordner
  importieren inkl. transparenter Farbe (nur ViaScript: Bitmap hat Colorkey?
  — prüfen ob nötig; vermutlich reicht Drag&Drop in den Asset-Browser).
- [ ] **Priorität zur Laufzeit rendern** (aus Paket-1-Daten): native
  3D-Darstellung nutzt Höhen-Offset pro Priorität; RGSS-Tilemap kann es
  schon (`priorities`-Table) — native Pfad offen.
- [ ] **Busch-Flag-Effekt:** Spieler-Sprite unten „im Gras" (halbe Deckkraft/
  Z-Maske) wenn auf Bush-Tile steht (Daten kommen aus Paket 1).
- [ ] **Counter-Flag:** Event-Auslösung ÜBER ein Tresen-Tile hinweg
  (ActionButton-Trigger mit +1 Tile Distanz wenn Counter) — XP-Feeling.
- [ ] **Terrain-Tag:** definieren, was unsere Engine damit tut
  (z.B. Schritt-SE, Busch-Alternativen) — erst nach Paket 1 Daten verfügbar.
- [ ] **Player-Export:** `vc_redist`-Hinweis ins README/Release-Notes
  (User-Problem 2026-07: Ziel-PC ohne vc_redist → Exe schließt sofort).
- [ ] **XP_Scripts/ schrittweise lauffähig:** die 90 Original-Skripte gegen
  unsere RGSS-Implementierung laufen lassen; jedes noch-fehlende API hier
  eintragen. Bekannte dokumentierte Grenze: `load_data`/Marshal (rxdata).

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
