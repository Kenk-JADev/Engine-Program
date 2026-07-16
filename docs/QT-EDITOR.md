# Qt-Editor (RPGMAKER3D_EDITOR_QT)

Der Editor ist eine **native Qt-App** (QMainWindow + Dock-Panels + Tabs).
Der **ImGui-Editor ist entfernt**. Der **Player** bleibt SDL2 + optional RmlUi.

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
  QtCodeWorkspace.*     Ersetzt den alten ImGui "Game Scene"/Script-Schwerpunkt:
                        - Ruby: Projekt-Scripts editieren (ScriptManager)
                        - C++: Engine-API-Referenz + Snippets (read-only)
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
- [x] **Code Workspace** (Ruby editierbar, C++ API-Referenz) ersetzt Game-Scene-Fokus

Offen:
- [ ] Map-Editor-Panel als Qt-Dock
- [ ] Database-Editor (QTableView)
- [ ] RmlUi-Input-Bruecke im Qt-Modus
- [ ] Optionale Syntax-Highlighting-Erweiterung (QSyntaxHighlighter)

## Legacy

Die alten Dateien `src/Editor.cpp`, `EditorStyle`, `EditorToolbar`, `AudioPreview`
werden **nicht mehr gebaut**. Optional: `RPGMAKER3D_ENABLE_IMGUI=ON` aktiviert nur
noch das historische GameUI-ImGui-Overlay (kein Editor).

## Lizenz

Qt Widgets: LGPL/kommerziell. Dynamisches Linken (`windeployqt`) ist unproblematisch.
