# Engine- & Game-Logic-Analyse — RPG Maker 3D

> **Anfrage:** Analyse des Codes nach Engine Logic und Game Logic — läuft das in
> der Engine (mit Grafik) erstellte Spiel überhaupt korrekt?
> **Stand:** 2026-07-29, HEAD `e80656e` (PAKET 48), Branch `arena/019fadb3-engine-program`
> **Methode:** Vollständige statische Code-Analyse + Syntax-Compile aller 44
> `src/*.cpp`-Dateien (`g++ -fsyntax-only`, Include-Pfade wie Makefile/CMake).

---

## 1. Kurzfazit

| Frage | Antwort |
|---|---|
| Kompiliert die Engine? | **Ja.** 43/44 Dateien sauber. Nur `src/Editor.cpp` bricht unter Linux (`<gtk/gtk.h>`) — diese Datei ist **nicht mehr im CMake-Build** (ImGui-Editor entfernt, Qt ist der Editor-Host). Betroffen ist nur das veraltete `Makefile` (s. §7). |
| Läuft ein in der Engine erstelltes Spiel? | **Grundsätzlich ja.** Die komplette Kette Projekt → Datenbank → Karte → Events → Scripts → Renderer ist verzahnt. Das `SampleProject` ist intern konsistent (Mapgrößen ↔ MapInfos, Kollisions-Flags ↔ belegte Tiles). |
| …aber wirklich *korrekt*? | **4 echte Defekte gefunden, die ein erstelltes Spiel konkret falsch/abstürzend gemacht hätten — alle in dieser Session behoben** (s. §6). Der gravierendste: Audio-Update fehlte komplett (Musik lief/fadete nie korrekt, stille Sound-Leaks). |

---

## 2. Architektur: Wo Engine Logic endet und Game Logic beginnt

```
┌────────────────────────────── GAME LOGIC ──────────────────────────────┐
│ Ruby-Skripte (scripts/*.rb)        XP-Events (MapNNN_events.json)      │
│  SceneManager/Scene_Map/…           EventSystem + EventInterpreter     │
│  Game_Klassen, $game, UI.*          BattleSystem (Zustandsautomat)     │
│                                     Game-Singleton (Party, Player,      │
│                                     Switches/Variables, GameMap, Save) │
├──────────────────────────── BRÜCKE (C++ Bindings / Hooks) ─────────────┤
│ RubyVM (mruby, BindUI/BindGame/…) + GameUI/RgssUI/RUI + CallGameHook   │
├────────────────────────────── ENGINE LOGIC ────────────────────────────┤
│ Engine (Loop) · Window (SDL2/Qt-Foreign) · Renderer (GL 3.3, Forward,  │
│ Schatten) · Map/Tileset (Gitter→3D) · Scene/ECS · Input · AudioManager │
│ (miniaudio) · ResourceManager · Framebuffer · Project/Database (JSON)  │
├────────────────────────────────────────────────────────────────────────┤
│ Hosts: player_main (SDL2) · main (SDL2, Legacy) · qt_editor (Qt 6)     │
└────────────────────────────────────────────────────────────────────────┘
```

**Die Trennung ist sauber dokumentiert und im Code weitgehend eingehalten:**
C++ besitzt Frame-Loop, Rendering, Fenster, Eingabe, Audio, Datenhaltung.
Spiellogik (Szenen, Dialoge, Menüs, Quests) läuft in Ruby **und** — als
RPG-Maker-XP-Paritätsschicht — im nativen Event-/Battle-System von C++.
Das ist ein bewusst hybrides Modell: Was XP als „Event-Befehl" kennt, ist
nativ (Performance, Editor-Integration), was „custom" ist, läuft per Skript.

---

## 3. Engine Logic — Analyse der Kernmodule

### 3.1 Bootstrap (`main.cpp`, `player_main.cpp`, `Engine::InitializeInternal`)

- Zwei Einstiege: `RPGMaker3D` (Editor, SDL-Legacy — im CMake-Build durch
  `qt_editor/QtMain.cpp` ersetzt) und `RPGMaker3D_Player` (bereinigter
  Pfad: `--project`-Parsing inkl. Kurzform, `--battletest[=N]`,
  XP-Export-Layout `./Game/project.json`, `SetSaveDirectory` **vor** Start,
  Ruby-Syntax-Vorabprüfung aller Skripte mit sichtbarer Fehlermeldung).
- `InstallStartupTerminateHandler` + try/catch um den kompletten Start —
  kein lautloses `std::terminate` mehr. **Gut.**
- Init-Reihenfolge korrekt: GL-Loader → Subsysteme (Scene/Project/Map) →
  Datenbank-Defaults → Projekt laden → DB aus Projekt → Skripte →
  Tileset → GameMap-Bindung. Keine zirkulären Abhängigkeiten.

### 3.2 Frame-Loop (`Engine::Run` / `Update` / `Render`)

```
Run:   dt = clock (geklemmt ≤ 0,1 s) → Update(dt) → Render() → SwapBuffers
       FPS-Zähler alle 0,5 s. SDL_QUIT/Fenster-Close sauber verschaltet.
```

**`Update(dt)` — Reihenfolge (engine-seitig korrekt priorisiert):**

1. `Input->Update()` (Edge-Detection `IsKeyPressed` baut auf Frame-Anfang)
2. XP-Transition-Arbiter (`UpdateTransitionRequest` — Freeze→Swap→Fade)
3. Alt+Enter Vollbild · SDL-Event-Pumpe (nur Nicht-Qt-Hosts)
4. F5 Playtest-Toggle (Editor), F10 Debug-Inspektor
5. Kamera: Follow-Cam (weich gedämpft, Snap bei Teleport; Screen-Shake
   **nach** der Dämpfung addiert — Absicht, korrekt) oder Free-Fly
6. Partikelsysteme, GameMap↔Map-Bindung auffrischen
7. RUI-Manager-Tick (Maus + Tastatur-Navigation)
8. **PlayMode-Block (Game Logic Einspeisepunkt):**
   - Modale Eingabe zuerst (`GameUI::UpdateModalInput` — Choices/Zahl/Name
     haben Vorrang, Verbrauchsreihenfolge stimmt)
   - Esc → Pausenmenü (nur wenn nichts modal, nicht im Kampf,
     `NativeGameMenu=1`) — der historische Doppel-Toggle-Bug ist per Kommentar
     dokumentiert und behoben
   - E/Enter → `EventSystem::TryInteract` · E/Enter/Space → Message-Advance
   - Hooks: SE-Playback, Welt→Canvas-Projektion, Tile-Prioritäten,
     Schritt-Sounds nach Terrain-Tag, Windowskin-Auto
   - `Game::Get().Update(dt)` → darin: `GameMap.Update`,
     `EventSystem::Update`, Random-Encounter-Zähler (mit
     Teleport-Sprung-Erkennung >4 Felder — sauber)
   - `Player().Update` (nur wenn kein Menü/Kampf)
   - `BattleSystem::Get().Update(dt)` + Kampfstatus/Battler-Pictures/
     Todes-Fades/Ziel-Blinken
   - `RubyVM->Update(dt)` → `SceneManager.update` → `$game.update(dt)` →
     optional `$scene.__engine_frame` (XpSceneMode)
9. Game-Over-Arbiter + Kampfstatus-Geister-Aufräumen (läuft bewusst
   **außerhalb** des PlayMode-Blocks — Status kann nicht hängenbleiben)
10. `AudioManager::Update(dt)` ← **war bis zu dieser Session der fehlende
    Aufruf — jetzt behoben (§6.1)**
11. `GameUI::Update`, F9-HUD, Lighting-Sync (Scene→Punktlichter),
    `Scene::Update`

**`Render()`** — Host-FBO wird vorher gemerkt und nach Schatten-Pässen
restauriert (der historische „Game View schwarz"-Bug ist dokumentiert und
gelöst). Danach `RenderScene()`: Shadow-Pass (direktional + optional
Point-Cubemaps) → Skybox → Grid (nur Editor) → **Map** (nur wenn
`Game::Map().IsVisible()`) → ECS-Entities (Model/Material/Licht/Partikel/
Billboard-Sprites) → Editor-Gizmo → **PlayMode: Player + Event-Charaktere
als XP-Spritesheet-Billboards** (4×4-Frames, Richtungszeile, Laufphasen,
8er-Sheets à la VX-Ace, Busch-Halbschnitt, Quader-Fallback) → RUI-Canvas
(GL-Target) → RGSS-Fenster (oberste Schicht).

**Bewertung der Engine Logic:** Die Pipeline ist robust gegen die
historischen Fehlerklassen (FBO-Verlust, ImGui-Kontext, dt-Spikes,
doppelte Menü-Toggles, Map-Rand). dt-Klemmung auf 0,1 s verhindert
Teleport-Artefakte bei Haltepunkten. Es gibt **kein Fixed-Timestep** für
die Logik — für ein RPG-Maker-Spiel (kein Physik-Determinismus) akzeptabel,
die Geschwindigkeiten sind dt-basiert korrekt skaliert.

**Einziger struktureller Schwachpunkt (Performance, kein Fehler):**
Licht-Marker, Sprites und Gizmo erzeugen **jeden Frame** neue Meshes inkl.
`BuildGPU()` (glGenVertexArrays/glBufferData) und zerstören sie sofort
wieder (`~Mesh` löscht korrekt — kein Leak, dank PAKET-41-Fix). Bei vielen
Licht-Entities/Sprites kostet das messbar Treiber-Zeit. Empfehlung: gecachte
Shared-Quads/Cubes wie `GetCharacterFrameQuad` (das es bereits vormacht).

---

## 4. Game Logic — Analyse

### 4.1 Game-Singleton (`Game.cpp`)
- `NewGameAt`: Switches/Variables/SelfSwitches/Timer-Reset, Anfangsgruppe
  (System.initialParty, Fallback Actor 1), Map-Setup, **Spawn-Clamping in
  Kartenbounds + Suche nach begehbarem Spawn** (Kreis-Suche) — sehr
  anwenderfreundlich.
- `Update`: Timer, XP-Animationen, Spieler-Lock bei blockierendem
  Event/Message/Kampf (**Konsistenzregel zentral**, nicht verstreut),
  Encounter-Statik mit Reset-Hooks.
- Save/Load: JSON-Slots im **Projektordner** (`saves/saveN.json`), nie im
  exe-Arbeitsverzeichnis — inkl. SelfSwitch-Key-Rundreise, Map-Refresh-Hook
  nach Load. Slot-Kopfdaten für den XP-Speicherbildschirm.

### 4.2 GameMap (Kollision)
- Richtungsbewusste `IsPassable(x,z,dir)` gegen `TilesetData` (Passage/
  4dir) + Radius-Prüfung (4 Orthogonal + 4 Diagonalpunkte) +
  `EventSystem::IsBlockingAt` (solide Events) + Eck-Klemmregel für
  Diagonalen. Bush-Abfrage für den „im Gras"-Effekt. **Plausibel und
  XP-nah.** Ungebundene Karte ⇒ passierbar (Editor-Start) — dokumentiert.

### 4.3 EventSystem (`EventSystem.cpp`, ~115 kB)
- Voller XP-Befehlsinterpreter (Indent-Klammerung statt XP-604, dokumentiert),
  Autorun/Parallel/Touch-Trigger in `Update`, `TryInteract` inkl.
  **Tresen-Regel** (Counter-Tile ⇒ Event ein Feld dahinter), Move-Routen-
  Parser (PAKET 16, 32 Schritte), `RefreshEventPage` = XP-`refresh`
  (Seitenwechsel: Motion canceln, Grafik/Transparenz zurücksetzen,
  Speed/Freq der neuen Seite).
- **Wichtige Robustheit:** JSON `worldPos` ist nur ein Start-Hinweis; beim
  ersten Page-Refresh wird die Sichtposition aus den Kachel-x/z neu
  abgeleitet — stale Editor-Werte korrigieren sich selbst (im
  Sample gefunden und als ungefährlich verifiziert).
- `EnsureDemoEvent`: Demo-NPCs nur, wenn **keine** Events existieren —
  kollidiert nicht mit echten Projekten.
- Save/LoadMapEvents ↔ Qt-Editor-Format `formatVersion: 2` konsistent.

### 4.4 BattleSystem (`BattleSystem.cpp`, ~51 kB)
- Zustandsautomat Start→Input→Turn→Action→Victory/Escape/GameOver;
  Troop-Seiten pausieren über Hooks den Kampf; XP-Aktionswahl
  (Angriff/Fertigkeit/Gegenstand/Verteidigen/Flucht) über
  `GameUI::OpenBattleCommands`, abschaltbar per `NativeBattleMenu=0`;
  States (PAKET 17: Restriction, Slip-Damage, Auto-Removal, Ranks),
  Verteidigen-Halbierung, EXP/Level-Up-Rücksync in die Party, Sieg-ME,
  Game-Over-Hook. Flucht-Sperre respektiert.

### 4.5 RubyVM / ScriptManager / Szenen
- `RubyVM::Update` treibt **nur** Ruby (SceneManager, $game, $scene-Modus),
  Exception-Capture pro Aufruf — ein Skriptfehler killt nicht den Frame.
- Skripte: `ExecuteAllScriptsOnce` (Neustart-sicher via
  `InvalidateExecutedScripts`), Syntax-Vorabcheck, Default-Skript-
  Generator für frische Projekte.
- **Konfigurationsfalle:** `RPGMAKER3D_ENABLE_RUBY=OFF` (CMake-Default!)
  ⇒ keine Skript-Schicht — das eingebaute (native) Spiel läuft trotzdem
  (Titel/Karte/Kampf/Menü sind nativ), aber `SceneManager` & Co. sind tot.
  CI baut ON; lokal muss man es wissen (im README dokumentiert, s. §7).

---

## 5. Der kritische Pfad: „Im Editor gebautes Spiel läuft im Player"

Verfolgt wurde jede Datei, die der Editor für ein Spiel erzeugt:

| Asset (Editor-Ausgabe) | Konsument zur Laufzeit | Befund |
|---|---|---|
| `project.json` (Startkarte/Position) | `Project::Load` … und **jetzt** `Database::Load`-Fallback | ⚠️ **war ignoriert** (Fix §6.2) |
| `database/*.json` (13 Tabs) | `Database::Load` (Actors…System) | ✅ vollständig verdrahtet |
| `maps/mapN.map` (binär, Header validiert, ≤1024, ≤64 Layer) | `Engine::LoadRuntimeMap` (+ Fallback-Karte PAKET 25) | ✅ robust |
| `maps/MapNNN_events.json` | `LoadMapEvents` bei Start/Kartenwechsel | ✅ |
| `database/Tilesets.json` (`tilesetName`, Flags) | **jetzt** `ApplyTilesetForMap` (Textur+Flags) | ⚠️ **Name/ID waren ignoriert** (Fix §6.3) |
| `Graphics/Characters/<name>` | XP-Billboard-Renderer (Cache inkl. Negativ-Cache) | ✅ |
| `Graphics/Battlers`, `Pictures`, `Titles`, `System/windowskin` | GameUI/RUI-Resolver | ✅ |
| `Audio/{BGM,BGS,ME,SE}` | `ResolveAudioPath` → miniaudio (Stream für BGM/BGS) | ✅ nach Audio-Fix |
| `scripts/*.rb` | ScriptManager → mruby | ✅ (Flag beachten) |
| `Game.ini` | `LoadCustomConfigForProject` bei jedem Start (idempotent) | ✅ |
| `scene.json` (Editor-Entities) | `LoadScene` (handrollierter JSON-Parser, geclampt) | ✅ (Parser fragil, aber abgefangen) |

**Antwort auf die Ausgangsfrage:** Ja, das erstellte Spiel lief — mit drei
qualitativen Einschränkungen, die genau die Trennung „Engine Logic ↔ Game
Logic" betrafen (Audio-Tick, Startwerte, Tileset-Auflösung) — plus einem
Datenkorruptions-Risiko beim Speichern. Alle vier sind behoben.

---

## 6. Gefundene Defekte — in dieser Session behoben

### 6.1 🔴 Audio-Tick fehlte komplett (`Engine::Update`)
`AudioManager::Update(dt)` (Kommentar im Code: *„Public update function
(call from Engine::Update)"*) wurde **nirgends** aufgerufen — weder im
SDL-Player noch im Qt-Host.
**Folgen für ein erstelltes Spiel:** `Stop(handle, fadeOut>0)` setzt nur
`fadingOut=true`; die Fade-Progression und das endgültige Stoppen stecken
in `UpdateFade`. ⇒ `EndTitleMode()` → `FadeOutBGM(0.3f)` stoppte die
Titelmusik **nie** (lief bis zum nächsten Track-Wechsel weiter, alter Track
blieb sogar dann als Leiche höhrbar, da auch der Crossfade nie lief);
`CleanupFinishedSounds` nie ⇒ `mSounds` wächst mit jedem SE unbegrenzt,
`ma_sound`-Objekte sammeln sich bis Shutdown.
**Fix:** `if (mAudio) mAudio->Update(dt);` im unbedingten Teil von
`Engine::Update` (läuft damit in Editor **und** Player; Qt-Tick ruft
dieselbe Funktion).
*Dear-Hinweis: Im SampleProject unhörbar, weil dort keine Audiodateien
mitgeliefert werden (`bgmName:""`). Deshalb fiel es nie auf.*

### 6.2 🔴 Startwerte aus `project.json` wurden zur Laufzeit ignoriert
`Project::Load` parst `startMapId/startX/startY` brav nach `mInfo` — aber
**jede** Runtime-Stelle (`Game::NewGame`, `player_main`, `SetPlaying`,
Map-Wechsel) liest ausschließlich `Database::Get().System()` — und die kam
nur aus `database/System.json`, das frische Editor-Projekte nicht haben.
Ein Nutzer, der im Projektdialog die Startposition ändert, startete trotzdem
immer auf Karte 1 bei (0,0).
**Fix:** In `Database::Load` ein Fallback: existiert keine `System.json`,
werden `startMapId/startX/startY` (und als Fenstertitel der Projektname)
aus `project.json` übernommen.

### 6.3 🔴 Karten-Grafik (Tileset-Textur) hart verdrahtet
Die 3D-Karte lud **immer** `tileset_demo.png` (fest codierte Pfadliste) und
zog die XP-Flags immer von Tileset **ID 1**. `MapInfo.tilesetId` und
`TilesetData.tilesetName` aus den Editor-Tabs wurden für die Darstellung
komplett ignoriert — ein Projekt mit eigenem Tileset-Namen zeigte die
falsche (oder Checker-)Textur; ein Kartenwechsel auf eine Karte mit anderem
Tileset behielt Textur **und Kollisionsflags** der Startkarte (Optik/Kollision
wären auseinandergelaufen).
**Fix:** Neue `Engine::ApplyTilesetForMap(mapId)`: MapInfo→tilesetId→
TilesetData→Auflösung in `<Projekt>/Graphics/Tilesets/` (XP-Struktur),
`<Projekt>/assets/textures/`, optional mit/nach Extension; Demo-Fallback
behalten; Flags via `SetTilesetData` gekoppelt. Aufgerufen beim Engine-Start
(ersetzt den Altpfad) **und bei jedem `LoadRuntimeMap`-Kartenwechsel**.

### 6.4 🟠 `Engine::SaveScene` überschrieb still `maps/map1.map`
Jeder Aufruf von `SaveScene` (u. a. das **Playtest-Backup**
`__editor_play_backup.json` bei *jedem* Playtest-Start und „Szene speichern
unter") schrieb die gerade geladene Karte zusätzlich binär nach
`GetMapPath(1)`. War im Editor gerade Karte 2 offen, wurde **Karte 1 mit
dem Inhalt von Karte 2 zerstört**. Die Qt-Aufrufer speichern die Binärkarte
längst selbst mit korrekter ID (inkl. Kommentar dort, der genau diesen
Fehler beschreibt) — das Fragment war ein übersehener Legacy-Rest.
**Fix:** Entfernt; `SaveScene` schreibt nur noch die Szenen-JSON (die ohnehin
die komplette Karten-Sektion inkl. Layer/Tiles enthält — das Playtest-
Restore braucht die .map-Datei nicht).

**Verifikation aller Fixes:** `g++ -fsyntax-only` auf `src/Engine.cpp`,
`src/Database.cpp`, `include/rpgmaker3d/Engine.h` — sauber.

---

## 7. Offene Risiken / Technische Schulden (nicht angefasst, priorisiert)

| # | Befund | Ort | Schwere | Empfehlung |
|---|---|---|---|---|
| 1 | **`Makefile` veraltet**: kompiliert `src/Editor.cpp` mit (bricht ohne GTK), nimmt `imgui_impl_sdl2`, verlinkt GLEW-Pfade, definiert `RPGMAKER3D_ENABLE_IMGUI` nicht. Realer Build = CMake/CI. | `Makefile` | Mittel | Makefile auf CMake-Quellen angleichen oder als „nur Referenz" kennzeichnen |
| 2 | Per-Frame `MeshFactory::CreateCube/Quad` + `BuildGPU()` + Destruktor (glGen/glDelete je Frame je Entity) für Licht-Marker, Sprites, Gizmo | `Engine::RenderScene` | Mittel (Perf) | Statische gecachte Meshes (Muster: `GetCharacterFrameQuad`) |
| 3 | `RPGMAKER3D_ENABLE_RUBY=OFF` default ⇒ Skript-Schicht lautlos tot. Log-Meldung nur einmalig beim Init. | `CMakeLists.txt` | Mittel | Default ON sobald mruby-Checkout vorhanden, sonst Start-Dialog im Player |
| 4 | KEIN `mRubyVM`-Null-Guard nötig, aber `main.cpp` (SDL-Legacy) ruft `Database::Load` **vor** `engine.Initialize`, obwohl Initialize das selbst tut — doppelte Ladevorgänge/Pfade bei abweichendem `--project` | `src/main.cpp` | Niedrig | Legacy-Pfad mit Player-Pfad angleichen |
| 5 | `GameUI::ShowPicture` Title: Grafik `$title` greift auf `ResolvePicturePathFor` — Non-System.json-Projekte haben `titleGraphicName="001-Title01"` ⇒ Resolver-Fehlschlag ohne Fehlermeldung (leer) | `StartTitleMode` | Niedrig | Log-Zeile bei nicht auflösbarer Titelgrafik |
| 6 | Handrollierte JSON-Parser (LoadScene, Game-Saves) — Escape-/Unicode-Ecken, aber alle Zahlen geclampt (PAKET 41) | mehrere | Niedrig | Mittelfristig echtes JSON (nlohmann) |
| 7 | AGENTS.md behauptet „Stand PAKET 17", Code/HEAD ist PAKET 48 — Fortschrittsdoku driftet. | `AGENTS.md` | Niedrig | Doku aus Commit-Log neu generieren |
| 8 | dt-Logik ohne Fixed-Step; `dt=0` bei extremen FPS (>1000) lässt Timer stehen (kosmetisch) | `Engine::Run` | Niedrig | Optional: dt-max-FPS-Cap oder Logik-Substeps |
| 9 | Kampf-Battler-Pictures hart auf 4 Gegner begrenzt (`i<4`), XP erlaubt 8 | `Engine::Update` | Niedrig | Limit anheben oder dokumentieren |
| 10 | RgssUI hat keinen eigenen Update-Tick — Fenster-Animationen (Openness) laufen render-seitig; funktional ok, aber Feinsteuerung (z. B. Pause-unabhängige Animation) nicht möglich | `RgssUI.cpp` | Niedrig | Bei Bedarf `RgssUI::Update(dt)` ergänzen |

---

## 8. Empfohlene nächste Schritte

1. **Vollständigen CI-Lauf abwarten** (Windows-Editor + Player, RUBY=ON) —
   die drei Laufzeit-Fixes sind bewusst minimalinvasiv, sollten aber einmal
   gegen den Qt-Editor-Playtest gegengeprüft werden (Audio-Fade im
   Windows-Build hörbar testen: Titel-BGM → „Neues Spiel").
2. Perf-Paket: Gizmo/Licht-Mesh-Caches (Punkt 2).
3. Makefile entschärfen (Punkt 1) — aktuell ein Fallstrick für Linux-Nutzer,
   die `make` statt CMake versuchen.
4. TODO_XP_PARITY-Backlog weiter abarbeiten (Item-Zustände, force action…).

---

*Erstellt von Arena Agent Mode — Analyse-Stand HEAD `e80656e` + Fixes aus dieser Session
(`src/Engine.cpp`, `src/Database.cpp`, `include/rpgmaker3d/Engine.h`).*
