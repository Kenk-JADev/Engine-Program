# Qt-Editor (RPGMAKER3D_EDITOR_QT)

Der Editor ist eine **native Qt-App** (QMainWindow + Dock-Panels + Tabs).
Der **ImGui-Editor ist entfernt**. Der **Player** bleibt SDL2 + GameUI-ImGui-Overlay (PAKET 10: RmlUi vollständig entfernt).

## Architektur

```
qt_editor/
  QtMain.cpp            QApplication + GL 3.3-Core Surface-Format
  QtEditorWindow.*      QMainWindow: Menues, Toolbar, Docks, Statuszeile,
                        QTabWidget (Game View | Code), QTimer (~60 Hz)
  QtGameViewWidget.*    QOpenGLWidget:
                        - laedt glad via QOpenGLContext::getProcAddress
                        - ruft Engine::InitializeEmbedded()
                        - paintGL() -> Engine::Render()
                        - Qt-Key/Mouse-Events -> rpg::Input
                        - Linksklick-Raycast-Selektion
  QtCodeWorkspace.*     Ruby/C++ Code-Workspace (Syntax-Highlighting)
  QtSyntaxHighlighter.* QSyntaxHighlighter fuer Ruby + C++
  QtMapEditorDock.*     Map-Liste, Props, Tile-Palette, Paint-State
  QtDatabaseEditorDock.* QTableView Actors/Items/Enemies + System
  QtKeyMap.h            Qt::Key -> rpg::Key Mapping
```

### Zentral-Tabs (statt ImGui Game Scene)

| Tab | Inhalt |
|-----|--------|
| **Game View** | 3D-Ansicht, Entity-Picking, Playtest |
| **Code (Ruby / C++)** | Script-Editor + C++ API-Referenz |

Docks: Hierarchie, Eigenschaften, Konsole.

## Engine-Seite

- `Window::CreateForeign(w,h)` – Fenster-Handle ohne SDL/GL
- `Engine::InitializeEmbedded(w,h)` – ohne SDL-Fenster / ohne ImGui
- QTimer treibt `Update(dt)`, `paintGL` ruft `Render()` (kein `Engine::Run()`)
- Selektion: `Engine::SetSelectedEntity` / Highlight in `RenderScene`

## Build

Voraussetzung: Qt 6 (z.B. per [aqtinstall](https://github.com/miurahr/aqtinstall)):

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.9.1 win64_msvc2022_64 -O C:\Qt
```

```powershell
cmake -B build-qt -S . `
  -DRPGMAKER3D_EDITOR_QT=ON `
  -DRPGMAKER3D_BUILD_EDITOR=ON `
  -DRPGMAKER3D_ENABLE_IMGUI=OFF `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.9.1\msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build-qt --config Release
```

**Wichtig**: Qt-DLLs sind `/MD` – nicht mit `x64-windows-static` (`/MT`) mischen.

Deployment:

```powershell
C:\Qt\6.9.1\msvc2022_64\bin\windeployqt.exe build-qt\Release\RPGMaker3D.exe
```

## Status

Fertig:
- [x] Native Qt-Fenster mit Docks
- [x] Engine embedded in QOpenGLWidget
- [x] QTimer-Game-Loop, Playtest, Undo/Redo
- [x] Hierarchie + Eigenschaften
- [x] 3D-Klick-Selektion
- [x] **ImGui-Editor komplett entfernt** (nicht mehr im Build)
- [x] **Code Workspace** (Ruby/C++) ersetzt Game-Scene-Fokus
- [x] **Map-Editor-Dock** (Kartenliste, Props, Tile-Palette, Malen im Game View)
- [x] **Database-Editor** (QTableView: Actors/Items/Enemies + System)
- [x] **Syntax-Highlighting** (Ruby + C++ via QSyntaxHighlighter)
- [x] **Engine-Input-Bruecke** im Qt-Modus (Maus/Tastatur, F9 = GameUI-HUD)

- [x] **Map-Editor**: echte Tileset-Texturen, Pinsel + Rechteck
- [x] **Event-Editor-Dock**: Events / Seiten / Befehle
- [x] **Database**: Skills / Weapons / Classes
- [x] **Code**: Suche, Hot-Reload, Ruby-Fehleranzeige
- [x] **Theme „XP Classic“** (PAKET 35): helle MAKER-Werkzeug-Optik
- [x] **Radierer-Bugfix** (PAKET 35): Radierer loescht wirklich (siehe unten)
- [x] **Theme-bewusste Arbeitsflaechen** (PAKET 40): Palette/Karte folgen
  dem App-Theme statt hart verdrahteter Dunkeltoene (siehe unten)

## Feinschliff (PAKET 40)

Nach dem XP-Classic-Theme (PAKET 35) fielen zwei Flaechen auf, die noch
hartkodiert dunkel malten — jetzt sind sie theme-bewusst:

- **Tileset-Palette** (`QtTilesetGridWidget`): Hintergrund = `AlternateBase`
  des Themes (XP-Classic: helle Tafel), Hinweistext aus `Text`-Rolle,
  Hover = hell auf dunklem / Akzent auf hellem Theme, Auswahlrahmen =
  Theme-`Highlight` (2 px) mit weissem Innenrand fuer Saetze mit
  aehnlicher Grundfarbe.
- **Karten-Canvas** (`QtMapTab`): Hintergrund = leicht dunkleres `Base`,
  Leer-Felder nochmals abgedunkelt (statt Vollschwarz), Rasterlinien
  kontrastsicher in beide Richtungen (hell auf dunkel/dunkel auf hell).
- Radierer-Checkstatus + „Tile: N“-Anzeige waren bereits korrekt
  verdrahtet (geprueft beim PAKET-35-Audit).

## Theme & Fehler-Audit (PAKET 35)

### Theme „XP Classic“

`QtMain.cpp` — `ApplyXPEditorTheme()` ersetzt das fruehere dunkle
„Fusion Dark“-Theme durch eine helle Redaktions-Oberflaeche im Stil
klassischer MAKER-Werkzeuge:

- **Palette**: helles Werkzeuggrau-Blau (Window `222,229,241`),
  weisse Eingabeflaechen, XP-Blau als Akzent (`62,110,175`).
  Eigenes Farbset — **kein** Nachbau fremder Skin-/Ressourcen-Dateien.
- **QSS** fuer MenuBar, Menues, TabBar, Dock-Titel (Verlauf),
  ToolBar `#mainToolBar` (Check-Markierung XP-blau), Buttons
  (`:checked` = gedrueckter Werkzeug-Button), GroupBox, Eingabefelder
  mit Fokus-Rahmen, Baum-/Listenselektion, Scrollbars, StatusBar,
  Splitter.
- **Ausnahme bewusst**: Der Code-Editor (`QtCodeWorkspace`) behaelt
  seinen dunklen Hintergrund (`#1e1e1e`), weil die
  Syntax-Farbtabelle dafuer abgestimmt ist.

### Gefundene und behobene Fehler

- **Radierer malte statt zu loeschen** (`QtMapTab.cpp`):
  Der Radierer setzt `paintTile = -1`, aber `paintCell()` und
  `applyFloodAt()` mappen `< 0` auf `0` — es wurde also Tile 0
  (linkes oberes Tile des Sets) gemalt bzw. gefuellt, statt Zellen zu
  leeren. Beide Stellen schreiben jetzt den Wert unveraendert
  (`-1` = leer). `QtGameViewWidget` war nicht betroffen (nutzt
  `mPaintTile` direkt).

### Auditiert und fuer gut befunden

- Flutfuellung: durch Seen-Set begrenzt (kein Endloslauf).
- Undo/Verlauf: `paintCell` erzeugt bei „keine echte Aenderung“
  keinen Verlaufseintrag; Verlauf auf 100 Strokes begrenzt, Strokes
  anderer Karten/Groessen werden beim Undo verworfen.
- Alle `std::stoi`-Stellen im Editor try/catch-abgesichert.
- `Map::Load` besitzt seit PAKET 27 Validierung.
- `Map::Resize` klemmt seit PAKET 41 auf 1..1024 (Editor-Dialog ohnehin
  1..999) — defekte Karten-/Szenendateien koennen weder SIGFPE
  (idx % width == 0) noch Speicherexplosionen ausloesen; dasselbe gilt
  fuer `Engine::LoadScene` (JSON) und `Map::CreateFallback`.

Offen (spaeter):
- [ ] Gizmo-Transform im Game View
- [ ] Asset-Browser

## Legacy

Die alten Dateien `src/Editor.cpp`, `EditorStyle`, `EditorToolbar`, `AudioPreview`
werden **nicht mehr gebaut**. Optional: `RPGMAKER3D_ENABLE_IMGUI=ON` aktiviert nur
noch das historische GameUI-ImGui-Overlay (kein Editor).

## Lizenz

Qt Widgets: LGPL/kommerziell. Dynamisches Linken (`windeployqt`) ist unproblematisch.

## CI (GitHub Actions)

Der aktuelle CI (`.github/workflows/Main.yml`) baut ohne installiertes Qt:
CMake deaktiviert den Qt-Editor soft und nutzt SDL-`main.cpp` (Player + Core).

**Vollstaendiger Qt-CI** (aqtinstall + `x64-windows` + `windeployqt`) liegt in:

`BUILD-WINDOWS-WORKFLOW.yml`

Maintainer: Inhalt nach `.github/workflows/Main.yml` kopieren und pushen
(der Arena-Bot hat oft keine `workflows`-Permission).
