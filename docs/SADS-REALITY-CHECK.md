# SADS „Project Ruby" — Reality-Check gegen den echten Code

> **Anfrage:** Bewertung der Ideen-Spezifikation (SADS v1.0, „Project Ruby"):
> Worauf ist zu achten, was kann/muss realistisch eingebaut oder verbessert
> werden — gemessen am **tatsächlichen Stand** dieses Repos (HEAD `af6572b`,
> Stand 2026-07-29, inkl. Analyse-Fixpaket aus `docs/ENGINE-GAME-LOGIK-ANALYSE.md`).
>
> Format: je Spezifikations-Abschnitt → ✅ vorhanden / 🟡 teilweise /
> ❌ fehlt / ⚠️ Konflikt — plus konkreter Handlungsinhalt.

---

## 1. Gesamtbewertung

Die Spezifikation ist **in ihrer Zielrichtung deckungsgleich mit dem, was
dieses Repo bereits baut** (Ruby-Skripting, datengetriebene Projekte,
XP-Philosophie, modularer Editor). Sie ist aber in weiten Teilen
**überambitioniert** (Vulkan/Deferred/Jolt/IK/Netzwerk) und stellenweise
**inkonsistent zur eigenen Projektgeschichte** (das Repo hat ein ausdrückliches
„Low-Spec: OpenGL 3.3"-Designprinzip in `docs/ARCHITECTURE.md`).

Drei strategische Entscheidungen musst du treffen, BEVOR du weitere
Punkte einbaust — sie steuern alles andere:

### Entscheidung A: „Ruby-First" heißt bei dir was genau?
- **Spec sagt:** „Gameplay wird **ausschließlich** in Ruby entwickelt",
  „Die Engine besitzt **keine Spielregeln**", „>90 % der Logik in Ruby".
- **Code sagt:** Das Gegenteil ist bewusst gebaut: Die komplette
  **XP-Parität läuft nativ** in C++ (`EventSystem.cpp` ~115 kB,
  `BattleSystem.cpp` ~51 kB, `Game.cpp` ~59 kB). Ruby ist eine
  **Erweiterungsschicht obendrauf** (SceneManager, `$game`, UI-Hooks,
  Custom-Szenen). `RPGMAKER3D_ENABLE_RUBY` ist sogar **default OFF**.
- **Empfehlung: Den Hybrid behalten und die Spec umformulieren.**
  Begründung: XP-Kompatibilität (Editor-Katalog, Event-Befehle,
  Kampfregeln) ist das Alleinstellungsmerkmal — als Ruby-Port wäre sie
  langsamer, schwerer zu debuggen und doppelt zu pflegen. Formuliere statt
  „Engine hat keine Spielregeln": **„Die XP-Systeme sind native Plattform;
  alles Projekt-Eigene >90 % in Ruby."** Das ist das XP-Modell (RGSS sitzt
  AUF den Engine-Defaults) und genau das, was `Game.ini`
  (`NativeMessage=0`, `NativeBattleMenu=0` → Ruby übernimmt)
  architektonisch schon vorsieht. **Das ist der stärkste Punkt deiner
  Engine — nicht wegrefaktorieren.**

### Entscheidung B: Ruby 3.x ≠ mruby
- **Spec sagt:** „Skriptsprache Ruby 3.x".
- **Code sagt:** **mruby** (CI: `MRUBY_ROOT=third_party/mruby`, extern
  gebaut, nicht im Repo). mruby ist eine **Untermenge**: keine Gems, keine
  `require`-Kultur, reduzierte Stdlib, kein vollständiges Encoding/Regex.
- **Worauf achten:** Skript-Doku und Beispiele müssen **mruby-kompatibel**
  bleiben (kein `require 'json'`, kein `Fiber`-Missverständnis, keine
  `ObjectSpace`-Tricks). CRuby embedding ist praktisch nicht wartbar
  (Windows-Deployment, DLL-Chaos). **Empfehlung: Spec auf „Ruby (mruby)"
  korrigieren**, sonst versprichst du ein Ökosystem, das du nicht liefern
  kannst.

### Entscheidung C: Grafik-Zielbild
- **Spec sagt:** Vulkan primär, DX12 optional, Deferred+HDR+SSAO+SSR,
  Cascaded Shadows.
- **Code sagt:** OpenGL 3.3 Forward, Phong/Lambert, 1 Directional-Shadowmap
  (+ optionale Point-Cubemaps). Das Low-Spec-Prinzip ist ein **Feature**
  (RPG-Maker-Zielgruppe = schwache Rechner).
- **Empfehlung:** Renderer-Ausbau erst **nach** XP-Parität (TODO-Liste ist
  voll). Wenn irgendwann: erst modernes GL (Instancing existiert faktisch
  nicht, Culling fehlt — DAS ist die echte Baustelle), nicht Vulkan.

---

## 2. Abschnittsweiser Abgleich (alle 23 Kapitel)

| # | Spec-Punkt | Status | Worauf achten / was konkret einbauen |
|---|---|---|---|
| 1–3 | Projektidee, Ziele, „*-First"-Prinzipien | 🟡 | Prinzipien Modularität/lose Kopplung sind im Code **sichtbar umgesetzt** (Hooks statt Includes: `EventSystem_SetMapChangeHandler`, `GameUI::SetScriptDialogRouter` — gutes Vorbild!). Aber: Singleton-Flut (`Game::Get()`, `BattleSystem::Get()`, `GameUI::Get()`) ist die faktische Kopplung — bei weiterem Wachstum Interfaces einziehen. |
| 4 | Technische Basis | 🟡⚠️ | C++**17** (nicht 20 — ok, kein Muss). Audio: **miniaudio** (statt OpenAL — besser für kleine Engine, OGG/WAV/FLAC/MP3 läuft über eingebettete Decoder). Modelle: nur **OBJ** + eigenes `.anim`-Morph-Manifest (KEIN glTF). Physik: **keine** (Tile-Kollision + Ebenen-Raycast). „SQLite/YAML": nicht vorhanden und **nicht nötig** (JSON reicht; siehe §18). |
| 5 | Gesamtarchitektur (3 Schichten) | ✅ | Entspricht exakt `docs/ARCHITECTURE.md`. Editor-Modulliste: vorhanden sind Map/Events/Datenbank/Code/Assets-Docks; **fehlen** Terrain/Material/Animation/UI-Editor, Plugin-Manager, Profiler. |
| 6 | „Engine besitzt keine Spielregeln" | ⚠️ | Siehe Entscheidung A — **bewusst anders gebaut** und richtig so. Spec-Text anpassen. |
| 7 | Ruby-Laufzeit implementiert alles | 🟡 | Bindings-Fläche ist bereits breit: **~362 `mrb_define_*`-Aufrufe** (Engine/Game/Input/Audio/Map/Camera/UI/Rui/RgssWindow/Actor + RGSS-Tab in `RubyRgss.cpp`). Fehlen gegenüber Spec-Liste: Quests/AI existieren nur als Event-/Skript-Kultur (kein Framework), **Coroutine/Timer/Profiler/Filesystem/Netzwerk** als API. |
| 8 | ECS mit RubyBehaviour | 🟡 | ECS existiert (`Scene.h`: Transform/Sprite/ModelRenderer/Material/Light/ParticleEmitter/Camera + **`ScriptComponent` — das ist dein `RubyBehaviour`, aber aktuell TOT**: es wird nirgends instanziiert oder getickt). **Quick-Win #1 (s. §3).** RigidBody/Collider/Animator als Components fehlen — über Morph-Anim (PAKET 46–48) und Tile-Kollision fachlich abgedeckt. |
| 9 | Szenensystem (Start/Update/Late/Fixed/Render/Dispose) | 🟡 | Ruby-`Scene_Base` hat `start/update/terminate`; C++-`Scene` ist reiner ECS-Container. **Fehlen: `LateUpdate`/`FixedUpdate`-Konzept.** Quick-Win #2: in `RubyVM::Update` nach `SceneManager.update` noch `late_update` aufrufen; Fixed-Step-Akkumulator für Logik-only-Hooks (AI/Physik-Ersatz) ist ~15 Zeilen in `Engine::Update`. |
| 10 | Rendering (Deferred, HDR, Bloom, SSAO, SSR, CSM, LOD, Culling, Instancing) | ❌/🟡 | Vorhanden: Forward, Shadowmaps, Transparency, Post-Overlay (Farbton/Wetter via RUI), **kein Culling, kein Instancing** (Karte = 1 gebautes Mesh — ok). **Achtung, echte Perf-Baustelle im Ist-Code:** per-Frame `MeshFactory::CreateCube/Quad`+`BuildGPU()` in `RenderScene` (Licht-Marker/Sprites/Gizmo → glGen/glDelete pro Frame). Fix: gecachte Meshes (Muster existiert: `GetCharacterFrameQuad`). Bloom/HDR/SSAO: **nicht vor v1.0** — Konflikt mit Low-Spec-Ziel. |
| 11 | Physik (Jolt/Bullet, Character, Trigger, NavMesh) | ❌ | **Für ein Tile-RPG bewusst richtig weggelassen.** Was fehlt und gebraucht wird: (a) Raycast auf **Entities/Map** statt nur Ebene (Klick-Bewegung, Projektile, Sichtlinien) — `Raycast.cpp` hat nur `IntersectPlane`; (b) Trigger-Volumina lassen sich schon jetzt als Events mit `EventTouch` abbilden. Jolt/Bullet hier reinzuziehen = Wartungs-Bombe ohne Gameplay-Gewinn. |
| 12 | Animation (Skeletal, BlendTrees, IK, RootMotion) | 🟡 | Vorhanden: **Keyframe-Morph-Animation** (OBJ-Sequenzen, `.anim`-Manifest mit `speed`/Clips, Instanz-Pose pro Entität, Event-Befehle 508/509, Auto-Start). Charaktere = XP-Spritesheets (Billboards). Skeletal/IK: ❌ — **passt nicht zur XP-Sprite-Ästhetik**; besser: Morph-API ins Ruby-Binding heben (Clip per Skript starten/stoppen — Bindings fehlen noch, nur Event-Befehle). |
| 13 | Audio (3D, Spatial, Mixer, Busse) | 🟡 | miniaudio-Backend mit BGM/BGS/ME/SE-Kanälen, Stream-Flag, Fades, Lautstärke-Mix (Game.ini + `Audio.bgm_volume=`). **Direkter Folgepunkt zum gestrigen Fix:** `Update3DAudio` läuft jetzt, aber der **Listener folgt nicht der Kamera** — `SetListenerPosition/Orientation` wird nie aus `Engine::Update` gesetzt. Quick-Win #3 (2 Zeilen, Kamera→Listener). Audio-Busse/Effekte: ❌, niedrige Prio. |
| 14 | UI (Fenster, Buttons, Container, Themes, Widgets) | 🟡 | **Drei UI-Schichten existieren bereits** — das ist dein größtes Architektur-Risiko: GameUI (nativ, Bildschirm-Effekte/Messages), RgssUI (Ruby-Fenster, XP-Kultur), **RUI** (eigenes retained Framework, PAKET 31: Panel/Window/Label/ListView/Gauge, Skin, Maus). **Achtung:** Spec „UI First" realistisch = **auf RUI konsolidieren** (steht so in `docs/RUI.md`), nicht eine vierte Schicht bauen. Buttons/Container/Tabellen/Bäume: in RUI nachziehen. |
| 15 | Editor-Module | 🟡 | Vorhanden (Qt): Map-Editor + Kartenliste, Event-Editor mit XP-Katalog, Datenbank (alle 13 XP-Tabs), Code-Workspace, Asset-Browser, Sound-Test, Karten-Eigenschaften. Fehlt: Terrain (Map-Editor ist der Tile-Editor — umbenennen genügt fast), Material-/Shader-/Animation-/UI-Editor (klein anfangen: .anim-Manifest-Editor wäre konkret nützlich), Plugin-Manager (Liste + an/aus), Profiler-Panel (FPS + ms/Tick reicht völlig am Anfang). |
| 16 | Custom-Code-IDE | 🟡 | Vorhanden: `QtCodeWorkspace` (Ruby-Tabs, Syntax-Highlighting, XP-Stil ohne Run-Button, **Speichern+Hot-Reload via Ctrl+R schon eingebaut!**, Fehleranzeige mit Zeile). Fehlt: Autocomplete/API-Browser (realistisch: statische Wortliste aus den `mrb_define_*`-Namen generieren), Debugger/Breakpoints (nur über `Engine.log` — echten mruby-Debugger nicht versprechen), Unit-Tests (mruby `mrbtest` denkbar, Prio niedrig). |
| 17 | Plugin-System (nur Ruby) | 🟡 | **Keim existiert:** `scripts/plugins/*.rb` wird nach den Kern-Skripten geladen (`01_hello_plugin.rb` als Vorlage). **Achtung/Fallen:** (a) keine Versions-/Manifest-Info, keine Deaktivierung, keine Fehlerisolierung — ein kaputtes Plugin bricht den kompletten Skriptlauf; (b) Ladereihenfolge nur alphabetisch (prefix-Konvention dokumentieren); (c) „Plugins isoliert" per mruby-`mrb_state` pro Plugin wäre Overkill — stattdessen: Modul-Namespace-Konvention (`module PluginName … end`) + Plugin-Metadaten als Kommentarkopf (`# @name`, `# @version`), im Editor aufgelistet. Das ist der **realistische MV-Style-Plugin-Manager** und in einem Paket machbar. |
| 18 | Dateistruktur | 🟡⚠️ | Aktuell: `Graphics/`, `Audio/`, `database/`, `maps/`, `scripts/plugins/`, `saves/`, `assets/{models,textures}`, `Game.ini`. Spec will `Assets/Models|Materials|…`. **Konflikt: Die XP-Konvention (Graphics/Characters etc.) ist bereits verankert** (Pfade in `ResolvePicturePathFor`, `RgssResolveGraphic`). → **Nicht umbauen.** Stattdessen Spec an den Ist-Stand angleichen: doppelte Wurzeln (`Graphics/` für 2D-XP-Assets, `assets/` für 3D) dokumentieren — sonst brichst du jedes Bestandsprojekt. SQLite: ❌ unnötig — JSON + `SQLite`-Ersatz wäre hier Overkill. |
| 19 | API-Liste | 🟡 | Vorhanden als Bindings: Engine, Game, Input, Audio, Map, Camera, UI, Rui, Actor, Graphics/Audio/Input (XP-Schicht), plus `Battle`-Modul. Fehlt: `Filesystem` (existiert C++-seitig als `Platform` — kleines Binding: `File.exist?`, `File.read` über Sandbox-Pfade), `Timer/Coroutine` (s. Quick-Win), `Profiler`, `AssetManager` (Resolver bereits intern vorhanden → als `Assets.resolve(name)` exponieren), `Network` (❌ bewusst), `Console/Logger` (teilw. `Engine.log`). |
| 20 | Spiellschleife | ✅/🟡 | Ist-Loop (Input → Transition-Arbiter → Events/Scene → Game → Battle → **Ruby am Ende** → Render) funktioniert. **Achtung — Abweichung zur Spec:** Spec will „Ruby Update" VOR Physik/Animation/Render. De facto tickt Ruby **nach** den nativen Systemen — das ist fürs Zusammenspiel korrekt (Ruby sieht frische Zustände), sollte aber dokumentiert bleiben, weil Skripte sonst „ein Frame hinten" annehmen. „Projekt speichern beim Beenden": macht der Player **nicht** (bewusst — Savegames manuell); Auto-Save-on-Quit wäre ein nettes Game.ini-Flag. |
| 21 | Sicherheit (Ruby kein Direktzugriff, Plugin-Isolation) | 🟡 | Erfüllt im Kern: mruby-Code sieht nur gebundene Funktionen, keine C++-Pointer. **Aber:** „nicht dokumentiert = nicht öffentlich" erfordert eine **generierte API-Doku** aus den Bindings — sonst weiß niemand, was „öffentlich" ist. Plugin-Isolation: s. #17 (Konvention statt Sandbox). Zusätzlich: `Eval`-Fläche (Event-Befehl „Script") ist mächtig — in Doku kennzeichnen. |
| 22 | Langfristige Ziele | 🟡 | „Modding ohne Neukompilierung": **schon erreicht** (alles Datengesteuert + Ruby). „Offene Dateiformate": erreicht (JSON, `formatVersion` in Event-Dateien — weiterführen: Savegame-Version-Feld!). „90 % Ruby": s. Entscheidung A. „Vollständige Doku": fehlt — `scripts/plugins/README.txt`-Kultur ausbauen. |
| 23 | Zusammenfassung | — | Inhaltlich tragfähig, nach unten skalieren (s. §4). |

---

## 3. Quick-Wins (hoher Nutzen, kleiner Eingriff) — in dieser Reihenfolge

1. **3D-Audio-Listener an Kamera koppeln** (`Engine::Update`, ~4 Zeilen):
   `mAudio->SetListenerPosition(cam.GetPosition()); mAudio->SetListenerOrientation(cam.GetForward(), cam.GetUp());`
   — macht den gestrigen Audio-Fix erst räumlich „aktiv" und die
   `AudioSource`-Idee der Spec erlebbar (BGS mit Entfernungspegel).
2. **`ScriptComponent` beleben** (Spec-Kapitel 8!): In `Scene::Update`
   Entities mit `ScriptComponent` finden → vorhandenes
   `RubyVM->CallGameHook("entity_update_<className>")` oder eine
   registrierte Ruby-Klasse `class Behaviour; def update(entity_id, dt)`.
   Editor: im Entity-Inspektor Pfad/Klasse setzbar. Damit ist „RubyBehaviour"
   *exakt wie in der Spec* real — heute ist die Klasse ein toter Stub.
3. **`LateUpdate` + Fixed-Step-Hook in RubyVM::Update** — Szenen-API der
   Spec (Kap. 9) komplett machen; Fixed z. B. 40 Hz Akkumulator, ruft
   `SceneManager.fixed_update` wenn definiert (guarded via `respond_to?`).
4. **Plugin-Manifest + Editor-Liste:** Kommentarkopf-Konvention
   (`# @plugin Name 1.0`, `# @author X`), `ScriptManager` sammelt Metadaten,
   Cargo-Kulanz: Alphabet-Ladereihenfolge + Fehler eines Plugins abbrechen
   lassen ohne Rest mitzureißen (pro Datei eigener `Begin/Rescue`-Report —
   `ValidateAllScripts` tut Syntax schon, Laufzeitfehler separat loggen).
5. **Per-Frame-Mesh-Erzeugung ablösen** (`RenderScene`: Licht-Würfel,
   Sprite-Quads, Gizmo) → statisch gecachte Meshes (Perf-Befund aus der
   Analyse). Größter spürbarer FPS-Gewinn im Editorfenster.
6. **Raycast→AABB/Mesh** (`Raycast.cpp` erweitern + `Physics.raycast`-
   Ruby-Binding) — Grundlage für Maus-Interaktion, Sichtlinien-AI,
   „Klicken statt Tastatur"-Mods.
7. **API-Doku-Generator:** Skript, das aus `RubyVM.cpp` die
   `mrb_define_*`-Zeilen extrahiert → `docs/API.md`. Macht „API First"
   wahr und füttert später Autocomplete (Kap. 16).
8. **Savegame-Versionsfeld** (`Save(...)` schreibt `"formatVersion":1`)
   — passt zur `formatVersion`-Idee der Event-Dateien und rettet dich vor
   zukünftigen Breaking Changes (Spec „offene Formate").

## 4. Nicht jetzt (bewusst warnen) — und warum

| Spec-Wunsch | Warum nicht (jetzt) |
|---|---|
| Vulkan/DX12, Deferred, HDR/Bloom/SSAO/SSR, CSM | Verletzt das eigene Low-Spec-Prinzip; die reale Render-Baustelle ist Culling/Instancing/GL-3.3-Hygiene, nicht die API. Frühestens nach XP-Parität + stabilisiertem Player. |
| Jolt/Bullet, Fahrzeuge, NavMesh | Tile-Welt mit `GameMap::IsPassable*`-Familie deckt XP-Regeln exakt ab; Physik-Engine würde Kollision/Teleports/Event-Routen gegenlaufen lassen. |
| Skeletal Animation, IK, Root Motion | XP-/MV-Charakterästhetik ist Sprite-Billboard; Morph-Clips (OBJ) decken „bewegliche 3D-Objekte" ab. |
| Netzwerk/Multiplayer | Kein Bedarf im XP-Paritäts-Ziel; zieht Auth/Session/Persistence nach — verteilt die Engine, bevor sie steht. |
| SQLite/YAML | JSON-Dateien + `JsonUtils` reichen; zweites Format = zweite Wahrheit. |
| echter Debugger/Autocomplete-Engine in IDE | Wartungsfalle; stattdessen generierte API-Liste + Log-kultur. |

## 5. Architektur-Regeln, auf die du beim Weiterbau achten solltest

(verdichtet aus den echten Befunden dieser Woche)

1. **Jeder Datenkanal Editor→Runtime braucht genau EINE Quelle.** Die
   Defekte `project.json`-Startwerte und `Tileset.tilesetName` entstanden,
   weil zwei Dateien dieselbe Info trugen und die Runtime nur eine las.
   Regel: Wer eine neue Eigenschaft einführt, verdrahtet Editor-Speichern,
   DB-Load **und** Runtime-Verbrauch im selben Paket (+ Fallback, wenn
   Altprojekte das Feld nicht haben).
2. **Tick-Vollständigkeit prüfen:** Jede `X::Update(dt)` muss einen
   Aufrufer haben — der Audio-Bug (`Update` ohne Aufrufer) ist das
   Musterbeispiel. Bei neuen Subsystemen: Aufrufstelle im selben Commit.
3. **Schreibpfade dürfen nie raten:** `SaveScene`→`map1.map`-Desaster.
   Regel: Speicher-Funktionen nehmen IDs explizit entgegen; „aktuelle"
   IDs werden nie beim Schreiben geraten.
4. **UI auf EINE tragende Schicht konsolidieren (RUI)** statt vierte zu
   bauen; GameUI/RgssUI als Kompatibilitätsschichten darüber/daneben.
5. **Neue Features immer zuerst gegen `TODO_XP_PARITY.md` priorisieren**
   — die XP-Parität ist das verkaufbare Kernversprechen; die SADS ist die
   ***Fern***-Vision (v2.0+), nicht die nächste Woche.
6. **mruby-Realität im Spec-Fix:** „Ruby 3.x" → „Ruby (mruby-Subset,
   dokumentierte API)", sonst Erwartungsbruch bei Nutzern.

---

*Erstellt von Arena Agent Mode — Abgleich SADS v1.0 „Project Ruby" ↔ Repo-Stand
`af6572b` (2026-07-29). Zahlen aus dem Ist-Code (z. B. 362 Bindings, Komponentenliste,
Plugin-Loader `ScriptManager.cpp:495`, Hot-Reload `QtCodeWorkspace` Ctrl+R).*
