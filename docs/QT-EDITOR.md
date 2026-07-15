# Qt-Editor (RPGMAKER3D_EDITOR_QT)

Der Editor laeuft als **native Qt-App** mit echten, separaten Fenstern
(QMainWindow + Dock-Panels). Das ersetzt langfristig den ImGui-Editor, der
alles nur im Game-Fenster rendert. Der **Player** und das **Playtest-HUD**
bleiben unveraendert (SDL2 + RmlUi/ImGui im GL-Kontext).

## Architektur

```
qt_editor/
  QtMain.cpp            QApplication + GL 3.3-Core Surface-Format
  QtEditorWindow.*      QMainWindow: Menues, Toolbar, Docks, Statuszeile,
                        QTimer (~60 Hz) -> Engine::Update(dt) + Repaint
  QtGameViewWidget.*    QOpenGLWidget:
                        - laedt glad via QOpenGLContext::getProcAddress
                        - ruft Engine::InitializeEmbedded()
                        - paintGL() -> Engine::Render()
                        - Qt-Key/Mouse-Events -> rpg::Input
  QtKeyMap.h            Qt::Key -> rpg::Key Mapping
```

Engine-Seite (minimal-invasiv, rueckwaertskompatibel):

- `Window::CreateForeign(w,h)` – Fenster-Handle ohne SDL/GL (Host besitzt Kontext)
- `Engine::InitializeEmbedded(w,h)` – gleicher Initialisierungsweg wie
  `Initialize()`, aber ohne SDL-Fenster; ImGui-Init + SDL-Event-Pump werden in
  `RPGMAKER3D_EDITOR_QT`-Builds ausgelassen
- QTimer treibt `Update(dt)`, `QOpenGLWidget::paintGL` ruft `Render()`
  (kein `Engine::Run()`)

## Build (Entwicklerrechner)

Voraussetzung: Qt 6 (z.B. per [aqtinstall](https://github.com/miurahr/aqtinstall)):

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.9.1 win64_msvc2022_64 -O C:\Qt
```

Konfigurieren – **wichtig**: Qt-DLLs sind `/MD`, daher NICHT mit dem
`x64-windows-static`-Triplet (`/MT`) kombinieren:

```powershell
cmake -B build-qt -S . `
  -DRPGMAKER3D_EDITOR_QT=ON `
  -DCMAKE_PREFIX_PATH="C:\Qt\6.9.1\msvc2022_64" `
  -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows
..\vcpkg\vcpkg.exe install sdl2:x64-windows glm:x64-windows
cmake --build build-qt --config Release
```

Deployment (Qt-DLLs neben die EXE):

```powershell
C:\Qt\6.9.1\msvc2022_64\bin\windeployqt.exe build-qt\Release\RPGMaker3D.exe
```

## CI (GitHub Actions) – manueller Schritt noetig

Der Bot-Account darf Workflow-Dateien nicht aendern. Damit der CI die
Qt-Editor-EXE baut, in `.github/workflows/Main.yml` **vor** "Configure CMake"
einfuegen:

```yaml
      - name: Install Qt for editor
        shell: pwsh
        run: |
          python -m pip install aqtinstall
          aqt install-qt windows desktop 6.9.1 win64_msvc2022_64 -O "$env:GITHUB_WORKSPACE/qt"
          echo "QT_DIR=$env:GITHUB_WORKSPACE/qt/6.9.1/msvc2022_64" | Out-File -FilePath $env:GITHUB_ENV -Append
```

und im CMake-Schritt hinzufuegen:

```
-DRPGMAKER3D_EDITOR_QT=ON `
-DCMAKE_PREFIX_PATH="${{ env.QT_DIR }}" `
```

sowie das Paket im "Prepare package"-Schritt um
`& "$env:QT_DIR\bin\windeployqt.exe" build/Release/RPGMaker3D.exe` ergaenzen
(und das SDL/vcpkg-Triplet fuer den Qt-Build auf `x64-windows` aendern –
siehe Hinweis oben bzgl. /MD vs. /MT).

## Status / Roadmap

Fertig (PoC):
- [x] Native Qt-Fenster mit Docks (Hierarchie, Eigenschaften, Konsole), Menues, Toolbar
- [x] Engine embedded in QOpenGLWidget, eigener GL-Kontext, glad via Qt
- [x] QTimer-Game-Loop, FPS in Statuszeile, Playtest-Toggle (`Engine::SetPlaying`)
- [x] Eingaben (Tastatur/Maus/Wheel) vom Qt-Widget in `rpg::Input`
- [x] Ingame-UI (RmlUi) rendert weiter im Game-View

Fertig (Slice 2 – benutzbare Basisfunktionen):
- [x] Datei-Menue komplett: Projekt neu/oeffnen/speichern, Szene laden/speichern (QFileDialog)
- [x] Erstellen: Wuerfel/Ebene/Licht (undo-bar via `CreateEntityCommand`)
- [x] Undo/Redo/Loeschen ueber die `CommandHistory` der Engine (Kommando-Name im Menue-Text)
- [x] Szenen-Hierarchie-Dock (QTreeWidget, live, Live-Selektion)
- [x] Eigenschaften-Dock: Name (`Scene::SetEntityName`) + Transform editierbar, 2-Hz-Sync

Offen (Portierung aus `Editor.cpp`):
- [ ] Map-Editor-Panel + Tileset-Auswahl als Qt-Docks
- [x] 3D-Klick-Selektion im Game-View (Raycast) + Auswahl-Highlight im Renderer
- [ ] Database-Editor (QTableView-Modelle)
- [ ] RmlUi-Input-Bruecke im Qt-Modus (RmlUi bekommt aktuell keine Events)
- [ ] Undo/Redo evtl. zusaetzlich an QUndoStack spiegeln (Command-Property-Edits)

## Lizenz-Hinweis

Qt (Widgets) steht unter LGPL/kommerzieller Lizenz. Durch **dynamisches**
Linken (Qt-DLLs, `windeployqt`) ist LGPL-Nutzung unproblematisch – genau so
ist dieser Build konfiguriert. Kein statisches Qt verwenden.
