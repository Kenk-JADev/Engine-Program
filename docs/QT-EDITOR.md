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
