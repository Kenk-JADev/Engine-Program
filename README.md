# RPG Maker 3D Engine

Eine modulare 3D-Game-Engine im Stil von RPG Maker, aber mit modernem Renderer und Ruby-Scripting.

## Ziele

- **Low-Spec-freundlich**: OpenGL 3.3, instanzierbares Rendering, optionale Low-Poly-Modi
- **Ruby-Scripting**: Eingebettete `mruby`-VM für Spiellogik, Events, Kampfsysteme
- **Qt-Editor**: Native Fenster, Game View, **Code Workspace (Ruby + C++)**
- **Asset-Support**: PNG-Tilesets, OBJ/GLTF-Modelle, OGG/MP3/WAV-Audio, Shader

> **Hinweis:** Der alte Dear-ImGui-Editor ist **entfernt**. Einziger Editor-Host ist Qt
> (`docs/QT-EDITOR.md`). Der Tab **Code** ersetzt den früheren Game-Scene-/Script-Fokus
> und ist für Ruby-Spiellogik sowie C++-Engine-API gedacht.

## Architektur

```
┌─────────────────────────────────────────────┐
│   Qt Editor (Game View + Code Ruby/C++)     │
├─────────────────────────────────────────────┤
│              Scripting (mruby)              │
├─────────────────────────────────────────────┤
│  Scene Graph  │  Renderer  │  Audio  │ ECS  │
├─────────────────────────────────────────────┤
│  SDL2 (Player) / Qt GL (Editor) │ OpenGL 3.3│
└─────────────────────────────────────────────┘
```

## Ordnerstruktur

- `src/` – Engine-Quellcode
- `include/rpgmaker3d/` – Öffentliche Header
- `qt_editor/` – Qt-Editor (Hauptfenster, Game View, Code Workspace)
- `ruby/` – Beispiel-Scripts und Runtime-Scripts
- `assets/` – Shaders, Texturen, Modelle, Audio
- `third_party/` – glad, stb_image, miniaudio, imgui, …
- `docs/` – Architektur- und API-Dokumentation

## Build (Windows mit Visual Studio 2022)

1. vcpkg installieren:
   ```cmd
   git clone https://github.com/Microsoft/vcpkg.git
   .\vcpkg\bootstrap-vcpkg.bat
   .\vcpkg\vcpkg install sdl2:x64-windows-static sdl2-image:x64-windows-static glew:x64-windows-static
   ```

2. Projekt generieren:
   ```cmd
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release
   ```

## Build (Linux)

```bash
sudo apt install build-essential cmake libsdl2-dev libglew-dev libgl1-mesa-dev
mkdir build && cd build
cmake ..
make -j
```

## Build (mit mruby)

Für Ruby-Scripting muss mruby gebaut werden:

```bash
cd third_party
git clone https://github.com/mruby/mruby.git
cd mruby
make
```

Dann in CMake `RPGMAKER3D_ENABLE_RUBY=ON` setzen.

## Schnellstart (Qt-Editor)

```bash
cmake -B build -S . -DRPGMAKER3D_EDITOR_QT=ON
cmake --build build -j
./build/RPGMaker3D   # bzw. build/Release/RPGMaker3D.exe
```

Im Editor:
- Tabs unten (Browser-Stil): **Spielansicht** (3D), **Landkarte** (2D), **Spiel** (Playtest), **Skript** (Ruby/C++)
- Ribbon oben: **Datei / Werkzeuge / Ansicht / Fenster / Debug**
- `Datei > Neues Projekt` / `Projekt öffnen`

## Bedienkomfort (Easy to use)

- **Symbole überall**: Ribbon, Schnellzugriff, Menüs, zentrale Tabs und die
  Landkarten-Werkzeuge nutzen erkennbare Qt-Standardsymbole (Diskette = Speichern,
  ▶ = Playtest, Papierkorb = Löschen, Pfeile = Rückgängig/Wiederholen …) –
  ganz ohne Asset-Dateien.
- **Willkommens-Dialog** beim Start (ohne geladenes Projekt): Neues Projekt,
  Projekt öffnen oder direkt ein **zuletzt geöffnetes Projekt** per
  Doppelklick. Abschaltbar über „Beim Start anzeigen“
  (wieder einschalten: `Hilfe > Willkommens-Dialog öffnen`).
- **Zuletzt geöffnete Projekte**: `Datei > Zuletzt geöffnete Projekte`
  merkt sich die letzten 8 Projektordner (inkl. „Liste leeren“).
- **Statuszeile**: zeigt permanent die aktive Karte (Name, ID, Größe) und das
  Maus-Feld auf der Landkarte; links laufen weiter die Kontext-Hinweise.
- **Tastenkürzel-Übersicht**: `Hilfe > Tastenkürzel anzeigen …`.
- **Kartenliste (Map-Dock)**: Rechtsklick-Menü mit *Karteneigenschaften …*,
  *Als Startkarte festlegen* (★-Markierung in der Liste), *Neue Karte*,
  *Karte löschen*; Doppelklick öffnet direkt die Karteneigenschaften.

## Landkarte (2D ↔ 3D-Mapsystem)

Die Landkarte malt direkt in `Engine::GetMap()` — dieselbe Karte, aus der das
3D-Mapsystem seine Geometrie baut. Oben sitzen die **XP-Ebenen-Buttons
1 / 2 / 3 / EV**: die ersten drei wählen die Tile-Ebene (Karten wachsen
automatisch auf 3 Ebenen), **EV** schaltet in den **Ereignis-Modus**: Events
werden als Kästchen eingezeichnet, **Doppelklick** auf ein leeres Feld legt ein
neues Event an und öffnet den XP-Event-Dialog, Doppelklick auf ein Event
bearbeitet es, **Entf** löscht das gewählte Event. Das Events-Dock wird dabei
live synchronisiert. Links sitzt die **XP-Tileset-Palette** mit echten
Tile-Bildern (erster Eintrag: Radierer) — sie ist mit dem Map-Dock und dem
3D-Pinsel in beide Richtungen synchronisiert.

Bedienhilfen der Landkarte:
- **Rückgängig / Wiederholen** (`Strg+Z` / `Strg+Y`, Toolbar-Buttons):
  jeder Mal-Schritt (Ziehen über mehrere Felder) lässt sich zurücknehmen;
  der Verlauf gilt pro Karte und übersteht Tab-Wechsel.
- **Rechtsklick-Menü**: Startposition hierher setzen, Ereignis anlegen /
  bearbeiten / löschen (im EV-Modus) und Karteneigenschaften öffnen.

## Karteneigenschaften

Ribbon **Werkzeuge → Karteneigenschaften** (oder **Doppelklick** auf eine Karte
in der Map-Liste) öffnet den XP-Dialog: Name, **Tileset**, **Größe** (Resize
erhält den Inhalt links-oben, wie XP), Scroll-Typ, Encounter (Schritte +
Trupp-Liste), **BGM/BGS mit ▶-Anhören** und „Rennen verboten". OK speichert
Karte + Datenbank und aktualisiert 2D/3D sofort. Das gewählte **BGM/BGS
wird im Spiel automatisch abgespielt**, sobald die Karte betreten wird
(wie in XP mit „Automatisch abspielen"); die Dateien liegen in
`Audio/BGM` bzw. `Audio/BGS` im Projektordner.

## Skript-Editor

Der Skript-Tab folgt dem XP-Aufbau: links die Skriptliste, rechts der Editor.
Neu / **Umbenennen (F2 oder Doppelklick auf den Eintrag)** / Löschen /
Suchen (Strg+F) / **Snippets** (fertige Bausteine, u. a. XP-Beispiele mit
`$game_switches`, `$game_party`, `$game_player` und HUD-Steuerung) /
Ausführen / Hot-Reload. Umlaute sind überall erlaubt.

## Playtest

- **F5** – startet **`RPGMaker3D_Player.exe --project <Pfad>`**
  (exakt wie beim Spieler; das Player-Target ist `RPGMAKER3D_BUILD_PLAYER=ON`, Standard)
- **Umschalt+F5** – schneller eingebetteter Test direkt in der Spielansicht
- Player von Hand: `RPGMaker3D_Player --project SampleProject` (oder Pfad als 1. Argument)
- **Speicherfrage vor jedem Test**: Da das Spiel alles von der Festplatte liest,
  fragt der Editor vorher: *Speichern & Starten / Ohne Speichern starten /
  Abbrechen*. Wer immer speichern will, aktiviert die Merk-Checkbox
  (einstellbar auch im Spiel-Tab).
- **Spiel-Tab = Playtest-Übersicht**: Projekt-Info (Name, Ordner, Spieltitel,
  Startkarte, Zähler für Karten/Ereignisse/Skripte), Status der Player-exe,
  Auto-Speichern-Option und ein Knopf **„Alle .rb jetzt prüfen“**, der alle
  Ruby-Skripte ohne Start auf Syntaxfehler testet (Datei + Zeile).
- **HUD**: Im Editor ist das In-Game-HUD standardmäßig aus (die Statuszeile
  zeigt FPS/Karte/Projekt); beim Playtest wird es sichtbar, **F9** schaltet es
  jederzeit um.

## Editor (Engine-exe) vs. Player (Game-exe)

Beide Programme lesen **dieselben Projektdateien** – was du im Editor baust,
spielt der Player unverändert:

| Inhalt | Editor schreibt nach | Player liest von |
|---|---|---|
| Projekt | `<Projekt>/project.json` | `<Projekt>/project.json` |
| Szene | `<Projekt>/scene.json` | `<Projekt>/scene.json` |
| Karten | `maps/map<ID>.map` (aktive Karten-ID, nicht fest 1) | `maps/map<Startkarten-ID>.map` |
| Karten-Events | `maps/Map<ID>_events.json` | `maps/Map<ID>_events.json` |
| Datenbank | `<Projekt>/database/*.json` | `<Projekt>/database/*.json` |
| Ruby-Skripte | `<Projekt>/scripts/*.rb` | `<Projekt>/scripts/*.rb` |
| **Spielstände** | **`<Projekt>/saves/save<N>.json`** | **`<Projekt>/saves/save<N>.json`** |

Wichtig dabei:
- **Speicherort der Spielstände**: liegt immer im Projektordner
  (`<Projekt>/saves/`), nie relativ zum Arbeitsverzeichnis der exe –
  egal, von wo aus Player oder Editor gestartet werden.
- **Speicherinhalt** (Format `version: 2`, lesbarer JSON): Gold, Karten-ID,
  Position, Party (Akteure mit Level/HP/MP/EXP), **alle** 5000 Switches und
  5000 Variables, **Self-Switches** (A/B/C/D), Items, Waffen, Rüstungen und
  die Speicher-Anzahl. Ältere Stände (`version: 1` + Legacy-Binär) werden
  weiterhin geladen.
- **Ruby-Startprüfung**: Player und Playtest parsen vor dem Start **alle**
  `.rb`-Dateien mit dem echten Ruby-Parser, **ohne sie auszuführen**.
  Syntaxfehler werden mit Datei + Zeile gemeldet (Player: Dialog + Konsole,
  Editor: Konsole-Log). Laufzeitfehler landen mit **Backtrace**
  (Aufrufkette, Datei/Zeile) im Log.
- Fenstertitel des Players = **Spieltitel** aus der Datenbank (System-Tab).

### Fehlersuche: Player/Editor startet nicht (Windows)

Wird die exe bzw. `RPGMaker3D_Player.exe` auf einem fremden Rechner
ausgeführt und **schließt sich sofort** (kurze Console, dann nichts), fehlt
fast immer die **Microsoft Visual C++ Redistributable (x64, VS 2022)**
(`vc_redist.x64.exe` von microsoft.com). Die Engine ist dynamisch gelinkt –
auf Entwicklerrechnern ist die Laufzeit durch Visual Studio vorinstalliert,
auf Ziel-PCs nicht. Workarounds:

1. **vc_redist installieren** (empfohlen, einmalig pro Rechner), oder
2. exe **statisch linken** (CMake-Option `-DCMAKE_POLICY_DEFAULT_CMP0091=NEW`
   + `/MT`), wenn der Build angestoßen wird, oder
3. wenn die Start-Härtung greift, liegt neben der exe eine **`engine.log`**
   mit einer Zeile `[FATAL-STARTUP] ...` – der genaue Grund steht darin.

Das war übrigens auch die Ursache beim gemeldeten „Programm öffnet nicht"-
Problem (Juli 2026): fehlendes vc_redist auf dem Zielrechner.

## Events (XP-Befehlssatz)

Der Event-Interpreter unterstützt den **vollständigen RPG-Maker-XP-Befehlssatz**
(Codes 101–355, siehe `docs/EVENTS-XP.md`): Text, Auswahl mit „Wenn“-Zweigen,
Zahlen-/Namenseingabe, Bedingungen (13 Typen), Schleifen, Bewegungsrouten,
Bildschirmeffekte, Kampfverarbeitung mit Sieg/Flucht/Niederlage-Zweigen usw.

Die Audio-Befehle (**BGM/BGS/ME/SE abspielen**, Fadeout BGM/BGS, SE stoppen)
spielen dabei **echtes Audio** ab – gesucht wird in `<Projekt>/Audio/BGM|BGS|ME|SE/`
(XP-Struktur, Groß- oder Kleinschreibung) und `assets/audio/<art>/`; fehlt eine
Datei, erscheint eine Warnung mit dem gesuchten Namen im Log.

Im Editor: Dock **Events** → Doppelklick auf ein Event öffnet den **XP-artigen
Event-Dialog** (Seiten, Bedingungen, Grafik, Autonome Bewegung, Optionen,
Auslöser und die Befehlsliste mit `@>`-Einrückung). Der Befehlsdialog bietet
alle XP-Befehle auf drei Seiten.

## Spielmenü, Speichern/Laden & Laden (Shop) im Spiel

Alles ist jetzt **im Player sichtbar und spielbar** (Anzeige im
GameUI-ImGui-Overlay, Pfeiltasten/W-S wählen, E/Enter bestätigen, Esc zurück):

- **Titelbildschirm (Player)** – nach dem Start zeigt der Player den
  XP-Titel mit dem Spieltitel aus der Datenbank: **Neues Spiel /
  Weiterspielen / Beenden**. „Weiterspielen" ist nur aktiv, wenn ein
  Savegame existiert, und öffnet die Lade-Ansicht des Speicherbildschirms;
  Abbruch führt zurück zum Titel. Das HUD ist auf dem Titel ausgeblendet,
  **Titelgrafik** (`Graphics/Titles/<Name>` im Projekt, Name aus dem
  System-Tab) und **Titel-BGM** laufen wie in XP; aus dem Spielmenü führt
  „Spiel beenden → Zum Titelbildschirm" jederzeit hierher zurück.
- **Spielmenü (Esc)** – wie in XP mit allen Hauptpunkten:
  **Gegenstände** (Heil-Items per Enter direkt **benutzen**: Ziel wählen,
  Wirkung sofort, Gegenstand verbraucht; andere zeigen ihren Beschreibungstext),
  **Fertigkeiten** (Heil-Skills aus dem Menü einsetzbar, kosten MP),
  **Ausrüstung** (Waffe + Schild/Helm/Körper/Accessoire wechseln – Tausch
  läuft über das Inventar, Equip-SE), **Status** (Level, HP/MP mit
  Maximalwerten, EXP), **Speichern**, **Spiel beenden**
  (**Zum Titelbildschirm** / **Spiel verlassen** / Abbrechen), **Zurück**.
  Max-HP/MP (und Angriff/Abwehr/Agilität) werden aus der
  **Parameter-Kurve** des Akteurs berechnet: Im Akteure-Tab legst du
  Startwert (Lv 1), **Endwert** (bei Max-Level) und die
  **Wachstumskurve A–E** fest (A = sehr schnelles, E = sehr langsames
  Wachstum, C = linear – wie beim XP, nur als Auswahl statt Grafik).
  Außerdem pflegt der Akteure-Tab die **Start-Ausrüstung** (Waffen-/
  Rüstungs-IDs, kommagetrennt) und der Klassen-Tab die
  **Fertigkeiten ab Level** (Format `Level:Fertigkeits-ID`, kommagetrennt):
  Die Klassen werden jetzt auch wirklich in `Classes.json` gespeichert
  (vorher gingen EXP-Werte beim Neuladen verloren), und jeder
  **Level-Aufstieg** – im Kampf wie über „EXP/Level ändern" –
  lernt fällige Fertigkeiten automatisch mit Meldung.
  Respektiert die
  Event-Befehle „Menüaufruf/speichern (nicht) erlauben" (134/135).
- **Kartenwechsel funktioniert**: Der Transfer-Befehl (201) und das Laden
  eines Savegames wechseln jetzt wirklich Karte + Events + BGM (vorher
  blieb die alte Karte sichtbar). Spiele mit mehreren Karten laufen damit
  durchgängig wie in XP.
- **Speicherbildschirm** (Menü, Event-Befehl 352 oder `UI.open_save_screen`):
  **4 Slots** im XP-Stil mit Infozeile aus dem Savegame: Kartenname,
  erstes Gruppenmitglied + Level, Gold und Speicherzähler. Leere Slots sind
  beim Laden gesperrt. Nach dem Speichern spielt der Save-SE aus dem
  System-Tab der Datenbank.
- **Laden/Shop (Event-Befehl 302)**: XP-Ablauf **Kaufen / Verkaufen /
  Abbrechen** – Preise kommen aus der Datenbank, Verkauf bringt den halben
  Preis, Käufe ziehen das Gold sofort ab und legen die Ware ins Inventar.
  Bei zu wenig Gold ertönt der Buzzer-SE. Der Laden kennt jetzt **Gegenstände,
  Waffen und Rüstungen**: Waren-Text `1,2,w1,a3` (Zahl = Item, `w<ID>` =
  Waffe, `a<ID>` = Rüstung); beim Verkaufen landen alle drei Arten mit
  Anzahl in der Liste.
- **Sichtbar gemacht**: Auch **Auswahl (102)**, **Zahleneingabe (103)** und
  **Namenseingabe (303)** zeigen jetzt ein eigenes XP-Fenster im Player
  (vorher nur Tastatursteuerung ohne Anzeige). **Bilder** (231,
  `UI.show_picture`) werden ebenfalls im Player dargestellt – jetzt **bis zu
  8 gleichzeitig** und mit **Drehung** (232 Bildausschnitt bewegen/drehen bzw.
  `UI.tween_picture`); gesucht wird in `<Projekt>/Graphics/Pictures|Titles/
  Gameovers|Battlers/` (XP-Struktur), `assets/pictures/` u. a.
  (png/jpg/jpeg/bmp/tga).
- **Kampf-Ereignisse (XP-Trupps-Seiten)**: Im Trupps-Tab legst du bis zu
  **12 Seiten** pro Trupp an – jede mit **Spanne** (Kampf = einmal je Kampf,
  Runde = einmal je Runde, Moment = sofort bei Erfüllung bzw. erneut nach
  Nicht-Erfüllung), **Bedingungen** (Schalter AN, Runde `a + n×b`,
  Akteur-HP ≤ x %, Gegner-HP ≤ x % – alle angekreuzten müssen erfüllt sein)
  und dem auszuführenden **Gemeinsamen Ereignis** (Tab „Gem. Events“ mit
  vollem Befehlseditor: Texte, Schalter, HP/EXP ändern, alles wie sonst).
  Feuert eine Seite, **pausiert der Kampf**, bis die Befehlsliste fertig ist.
- **Kampfsystem im XP-Stil**: Sobald ein Akteur an der Reihe ist, öffnet sich
  das **Befehlsmenü** – **Angriff / Fertigkeit / Gegenstand / Verteidigen /
  Flucht** (Flucht gesperrt bei „Kann nicht fliehen“, Menüaufruf im Kampf
  gesperrt). Fertigkeiten zeigen MP-Kosten (zu wenig MP = ausgegraut),
  Gegenstände ihre Anzahl; danach folgt die **Zielwahl** mit HP-Anzeige
  (Gegner- oder Verbuendeten-Liste, Esc = ein Schritt zurück). Heil-Skills
  (Scope auf die eigene Gruppe) heilen Verbündete, Schadens-Items (negative
  HP-Recovery, z. B. Bomben) verletzen Gegner, **Verteidigen halbiert** den
  Schaden bis zur nächsten eigenen Aktion. Oben im Bild steht während des
  ganzen Kampfes die **Statusanzeige** (Gegner-Zeile mit HP, darunter die
  Gruppe mit HP/MP und K.O.-Markierung) und die **Gegner-Grafiken** – bis zu
  4 Battler-Bilder aus `<Projekt>/Graphics/Battlers/` (Dateiname = Feld
  „Battler-Grafik" im Gegner-Tab, besiegte Gegner verschwinden sofort). Die
  Akteure kämpfen mit ihren **echten Werten aus der Datenbank**
  (Parameter-Kurve A–E aus dem Akteure-Tab + Waffen-/Rüstungs-Bonus),
  HP/MP bleiben nach dem Kampf erhalten, und der Sieg schreibt EXP gut –
  inklusive **Level-Aufstiegen mit Meldung** (EXP-Kurve der Klasse, Formel
  wie in RPG Maker VX Ace). Auch der Event-Befehl „EXP ändern“ (315) nutzt
  diese Kurve und meldet Level-Aufstiege. **Niederlage** (ohne „Niederlage
  möglich" bei 301) zeigt **GAME OVER** (Grafik aus `Graphics/Gameovers/`
  + ME aus dem System-Tab) und kehrt nach Bestätigung zum Titelbildschirm
  zurück (Editor: Playtest-Stopp) – danach startet „Neues Spiel" wieder mit
  frischen, vollen Werten.
- **Kampftest** wie im XP-Datenbank-Dialog: Button **„Kampftest"** im
  Trupps-Tab der Datenbank (startet die Player-exe, vorher die übliche
  Speicherfrage) oder per Kommandozeile:
  `RPGMaker3D_Player --project <Pfad> --battletest=<Trupp-ID>` startet das
  Spiel und geht sofort in den Kampf gegen den Trupp (Anfangsgruppe aus dem
  System-Tab); `--battletest` ohne Zahl nimmt Trupp 1.

## Ruby-API (Spiel aus Skripten steuern)

Das Prinzip entspricht RPG Maker XP: **Die exe liefert nur den C++-Kern**
(Renderer, Audio, Eingabe, Event-Interpreter) – **die Spielsteuerung erfolgt
in Ruby** aus dem Skript-Editor heraus. Alle Skripte liegen in
`<Projekt>/scripts/*.rb` (plus `plugins/*.rb`) und werden **alphabetisch nach
Dateiname** geladen, daher XP-artig `00_Config.rb` … `main.rb` zuletzt.
Dateien sind UTF-8 (Umlaute erlaubt). Vor dem Start parsen Player und Editor
jedes Skript mit dem echten Ruby-Parser (Fehler = Datei + Zeile);
Laufzeitfehler melden sich mit **Backtrace** im Log.

### XP-Spielobjekte ($game_*)

Wie in XP stehen die Spiel-Daten als globale Objekte bereit. **Alle Setter
aktualisieren sofort die Event-Seiten** (XP: `$game_map.need_refresh`) – eine
Seite mit der Bedingung „Schalter 3 ist AN" klappt also in dem Moment um, in
dem das Skript den Schalter setzt:

| Ruby | Bedeutung (XP-Gegenstück) |
|---|---|
| `$game_switches[id]`, `$game_switches[id] = true` | Game_Switches (5000 Stück, `size` liefert die Anzahl) |
| `$game_variables[id]`, `$game_variables[id] = 5` | Game_Variables (5000 Stück) |
| `$game_self_switches[[map, ev, "A"]] = true` | Game_SelfSwitches – Schlüssel = [Karten-ID, Event-ID, „A"–„D"] |
| `$game_party.gold`, `.gain_gold(n)`, `.lose_gold(n)` | Game_Party – Gold (Bedingung „Gold oder mehr" reagiert sofort) |
| `$game_party.gain_item(id, n)`, `.item_count(id)` | Gegenstände (analog `.gain_weapon/.weapon_count`, `.gain_armor/.armor_count`) |
| `$game_party.add_actor(id)`, `.remove_actor(id)`, `.has_actor(id)` | Gruppenmitglieder (Bedingung „Akteur in der Gruppe") |
| `$game_party.members` | Array aus Hashes `{:id, :name, :level, :hp, :mp, :exp}` |
| `$game_player.x`, `.y`, `.z`, `.move_to(x, y, z)` | Game_Player – Position/Teleport |
| `$game_player.locked?`, `$game_player.locked = true`, `.moving?` | Spielerbewegung sperren/abfragen |
| `$game_map.id`, `.setup(id)`, `.width`, `.height`, `.visible?` | Game_Map – aktive Karte |

### HUD & Oberfläche aus Ruby

Der Skript-Editor steuert auch die Anzeige – **inklusive HUD** (HP/MP/Gold/
Kartenname im Spiel-Fenster):

| Ruby | Wirkung |
|---|---|
| `UI.show_message("Text")` | XP-Nachrichtenfenster |
| `UI.show_screen_text(text, x, y, r, g, b, a)` | freier Bildschirmtext (HUD-Ebene) |
| `UI.show_world_text(...)`, `UI.clear_texts` | Texte in der 3D-Welt |
| `UI.show_picture / .move_picture / .tween_picture / .remove_picture / .clear_pictures` | Bilder wie XP „Bild anzeigen/bewegen" |
| `UI.hud_visible = false`, `UI.hud_visible?` | **HUD ein-/ausblenden** (z. B. für Zwischensequenzen) |
| `UI.open_menu` | XP-Spielmenü öffnen (Gegenstände/Speichern/Beenden) |
| `UI.open_save_screen(true)` | XP-Speicherbildschirm (4 Slots; `false` = Laden-Ansicht) |
| `UI.open_load_screen` | XP-Ladebildschirm (Alias für `UI.open_save_screen(false)`) |
| `UI.gold`, `UI.add_gold(n)` | Gold-Anzeige lesen/ändern |

### Weitere Module

- **Game** (Komfort-API): `Game.save(slot)` / `Game.load(slot)` – Datei
  `<Projekt>/saves/save<N>.json`; `Game.start_battle(trupp_id)`,
  `Game.in_battle?`, `Game.map_id`, `Game.setup_map(id)`,
  `Game.new_game`, `Game.start_game` (NewGame + Spielmodus – für Custom-Titel).
- **Audio**: `Audio.bgm_play("name.ogg")` sowie BGS/ME/SE und Fades.
- **Input**: `Input.key_down?(:return)` (gehalten) und
  `Input.key_pressed?(:return)` (genau einmal beim Drücken – für eigene
  Menü-Navigation).
- **Engine / Map / Camera / Actor**: Fenster, Kartenmaße, Kamera, Akteurswerte.

### Regeln für eigene Skripte

1. **Ladereihenfolge über Dateinamen steuern** (`00_` lädt zuerst) – spätere
   Skripte dürfen Klassen/Methoden früherer erweitern (XP-Skriptliste).
2. **Setter benutzen, nichts doppeln**: `$game_switches[1] = true` löst
   bereits den Seiten-Refresh aus – kein extra Interpreter-Aufruf nötig.
3. **Audio-Dateien** gehören nach `Audio/BGM|BGS|ME|SE` (XP-Struktur);
   Karten-BGM läuft automatisch über die Karteneigenschaften.
4. **Fehlersuche**: erst „Alle .rb jetzt prüfen" im Spiel-Tab (Syntax), dann
   Playtest – Laufzeitfehler zeigen Datei/Zeile + Aufrufkette im Konsole-Log.

## Alles custom (eigene Titel, Menüs, Kämpfe, Skins)

Wie in RPG Maker XP ist **jede eingebaute Oberfläche ersetzbar**. Drei Hebel:

### 1. `Game.ini` (Projektordner) – eingebaute Oberflächen abschalten

Beim Anlegen eines Projekts legt der Editor eine kommentierte `Game.ini` an:

```ini
[RPG Maker 3D]
NativeTitle=1        ; 0 = kein eingebauter Titel -> Ruby-Hook Game.custom_title
NativeHud=1          ; 0 = HUD beim Start aus (UI.hud_visible= steuert)
NativeGameMenu=1     ; 0 = Esc öffnet NICHT das eingebaute Spielmenü
NativeBattleMenu=1   ; 0 = kein eingebautes Kampfmenü (Battle-API nutzen)
NativeBattleStatus=1 ; 0 = keine Gegner-/Gruppenzeile oben im Kampf
NativeMessage=1      ; 0 = Standard-Dialoge (Text/Auswahl/Zahl/Name) per
                         Ruby-Hooks Game.on_ui_* (Skript-System, PAKET 42)
```

Wird bei jedem Spielstart/Playtest neu gelesen (ändern ohne Engine-Neustart).
Alle sechs Schalter gibt es auch als Ruby-Setter, z. B.
`UI.native_battle_menu = false` (plus `...?`-Getter).

### 2. Ruby-APIs für eigene Oberflächen

- **Eigene Menüs in einer Zeile** –
  `UI.open_list_menu("Titel", ["A", ["B", false], "C"]) { |i| ... }`:
  Listenmenü mit Block; der Block bekommt den Index (`-1` bei Esc);
  `[text, false]` sperrt einen Eintrag.
- **Standard-Dialoge im Skript ersetzen** (PAKET 42 – das „System" ist
  Spiel-Code wie in XP): `NativeMessage=0` (oder `UI.native_message=false`)
  routet Text (101), Auswahl (102), Zahl (103) und Name (303) an die
  Ruby-Hooks `Game.on_ui_message(text, sprecher, position, face)` /
  `Game.on_ui_choices(text, optionen, abbruch_erlaubt)` /
  `Game.on_ui_number(titel, stellen, start)` /
  `Game.on_ui_name(titel, start, max_zeichen)`. Das Skript liefert das
  Ergebnis per `UI.deliver_message_done / deliver_choice(i|-1) /
  deliver_number(n) / deliver_name(s)` zurück; die Warte-Semantik des
  Interpreters bleibt identisch (IsBusy hält per Script-Hold). Ohne
  definierte Hooks fällt alles automatisch auf das eingebaute Fenster.
  **Referenz-Implementierung**: `scripts/18_System_Message.rb` (Opt-in über
  `00_Config.rb: SCRIPT_MESSAGE_SYSTEM = true`) – gedacht als editierbarer
  Startpunkt. Achtung: die Hooks bauen ihre Fenster aus `Rui`-Primitiven
  und rufen **nicht** `UI.show_message` (Endlosschleife!).
- **Custom-Titelbildschirm**: `NativeTitle=0` → die Engine ruft
  `Game.custom_title` auf (dort eigene Menüs/Szenen aufbauen; mit
  `Game.start_game` startet „Neues Spiel"). Ohne Hook startet das Spiel direkt.
- **Custom-Kampfszene** (`Battle`-Modul – der nativer Kampfkern bleibt):
  `Battle.setup([1,1,2], flucht, niederlage_moeglich)` (freie Gegnerliste!),
  `Battle.needs_input?`, `Battle.input_actor_index` / `input_actor_id`,
  `Battle.actors` / `Battle.enemies` (Arrays von Hashes mit den Schlüsseln
  `id, index, name, hp, max_hp, mp, max_mp, atk, def, agi, dead`),
  `Battle.set_action(Battle::ATTACK | GUARD | SKILL | ITEM | ESCAPE,
  ziel_index, skill_id, item_id, ziel_ist_akteur)`,
  `Battle.can_escape?`, `Battle.abort`, `Battle.last_outcome / last_exp /
  last_gold`, `Battle.damage_actor / damage_enemy(i, n)`,
  `Battle.heal_actor / heal_enemy(i, hp, mp)`, `Battle.message("text")`.
  Reihenfolge, Schaden, Sieg/Niederlage, EXP und Level-Ups macht weiter der
  C++-Kern – die Ruby-Szene ersetzt nur Auswahl & Darstellung.
- **Pro Frame laufen** weiterhin `SceneManager.update` und `$game.update(dt)` –
  dort eigene Szenen aktualisieren (mit `Input.key_pressed?` navigieren).

### 3. Eingebaute Oberflächen – eigenes UI-Framework (RUI, PAKET 31–39)

Die gesamte eingebaute Spielanzeige (Nachrichten mit Sprecher, Menüs,
Zahlen-/Namenseingabe, HUD, Bilder, Bildschirmtexte, Kampf-Statusfenster,
Farbton, Wetter) läuft über das **eigene RUI-Framework** mit eigener
GL-Renderer-Schicht – **RmlUi ist vollständig entfernt**, Dear ImGui wird
zur Laufzeit nicht mehr benötigt (nur noch optionaler Fallback). Eigene
Oberflächen baut man wie in XP üblich als **Ruby-Szenen/Fenster** (Punkt
4, RGSS; bzw. RUI-Skriptfenster, `docs/SCRIPT-RUI.md`) oder schaltet die
eingebauten per `Game.ini`-Flags (`NativeTitle/Hud/GameMenu/Message/…`) ab.
**F9** blendet das HUD ein/aus.

### 4. RGSS (Ruby Game Scripting System) – komplette XP-Skriptschicht

Die Engine enthält eine **vollständige RGSS-Schicht nach dem RGSS-Referenz-
Handbuch des RPG Maker XP** – ein eigenes 2D-System, das komplett aus Ruby
gesteuert wird. Alles wird als GL-Overlay auf der **obersten Ebene** (über
allen GameUI-Overlays) im logischen **640×480-Raum** gezeichnet.

**Abgedeckte RGSS-Bibliothek (Spezifikation: RMXP-Hilfe):**

| Bereich | Umfang |
|---|---|
| `Graphics` | `update`, `freeze`, `transition(dauer, datei, vague)` (Crossfade oder Masken-Grafik), `frame_reset`, `frame_rate` (10–120, Standard 40), `frame_count`, `width`/`height` (640×480) |
| `Input` | `update`, `press?`, `trigger?`, `repeat?`, `dir4`, `dir8` + Konstanten `DOWN/LEFT/RIGHT/UP/A/B/C/X/Y/Z/L/R/SHIFT/CTRL/ALT/F5…F9` (XP-Belegung: C = Enter/Leertaste/C, B = Esc/X/Num0, A = Shift/Z …) |
| `Audio` | `bgm/bgs/me/se_play(datei[, vol[, pitch]])` (0–100 / 50–150, auch `RPG::AudioFile`-Objekte), `bgm/bgs/me/se_stop`, `bgm/bgs/me_fade(ms)` – XP-Pfadauflösung (`Audio/BGM|BGS|ME|SE/<Name>.ogg|wav|mp3`) |
| `Bitmap` | `new(datei)` / `new(b, h)`, `dispose`, `disposed?`, `width`, `height`, `rect`, `blt`, `stretch_blt`, `fill_rect`, `gradient_fill_rect`, `clear`, `clear_rect`, `get_pixel`, `set_pixel`, `hue_change`, `blur`, `radial_blur`, `draw_text` (beide Formen, mit automatischem 60 %-Stauchen), `text_size`, `font`, `clone`/`dup` |
| `Font` | `name`, `size`, `bold`, `italic`, `color` + Klassenwerte `Font.default_name/size/bold/italic/color` und `Font.exist?` |
| `Color`/`Tone`/`Rect` | `red/green/blue/alpha`, `gray`, `set`, `==` – **echte Referenz-Semantik** (`sprite.color.set(...)` und `font.color.red = ...` wirken sofort) |
| `Sprite` | `bitmap`, `src_rect`, `x/y/z/ox/oy`, `zoom_x/y`, `angle`, `mirror`, `opacity`, `blend_type` (0/1/2), `bush_depth`, `color`, `tone`, `flash(farbe|nil, dauer)`, `viewport`, `dispose`/`disposed?`/`update` |
| `Plane` | `bitmap`, `visible`, `z`, `ox/oy`, `zoom_x/y`, `opacity`, `blend_type`, `color`, `tone` (kachelt + scrollt, z. B. Panorama/Fog) |
| `Viewport` | `new(x,y,b,h)` oder `new(rect)`, `rect`, `visible`, `z`, `ox/oy`, `color`, `tone`, `flash`, `dispose` – clippt Kinder sauber auf den Ausschnitt |
| `Tilemap` | `tileset`, `autotiles[i]` (7 Slots), `map_data` (Table x×y×3), `flash_data`, `priorities`, `visible`, `ox/oy`, `update` – **echte XP-Autotile-Mustertabelle** (48 Muster), Autotile-Animation, Prioritäten-Z-Ordnung, pulsierende flash_data |
| `Window` | `new([viewport])` (oder `new(x,y,b,h)`), `windowskin` (**Bitmap**, XP!), `contents` (automatische Bitmap), `stretch`, `cursor_rect`, `active` (Cursor-Blinken), `pause` (4-Frame-Pausenanimation), `x/y/z/width/height/ox/oy`, `openness` (0–255), `opacity`, `back_opacity`, `contents_opacity`, `visible`, `viewport`, `dispose`/`disposed?`/`update` |
| `Table` | `new(x[, y[, z]])`, `[]`, `[]=`, `xsize/ysize/zsize`, `resize` (int16-Matrix mit Wrap-around) |
| `RPG`-Modul | **alle Datenklassen** (`AudioFile`, `Map`, `MapInfo`, `Event`(+`Page`,`Condition`,`Graphic`), `EventCommand`, `MoveRoute`, `MoveCommand`, `Actor`, `Class`(+`Learning`), `Skill`, `Item`, `Weapon`, `Armor`, `Enemy`(+`Action`), `Troop`(+`Member`,`Page`,`Condition`), `State`, `Animation`(+`Frame`,`Timing`), `Tileset`, `CommonEvent`, `System`(+`Words`,`TestBattler`)) mit XP-Standardwerten |
| `RPG::Cache` | `animation/autotile/battleback/battler/character/fog/gameover/icon/panorama/picture/tileset/title/transition/windowskin`, `tile`, `load_bitmap`, `clear` – plus Hue-Varianten pro Pfad |
| `RPG::Sprite` | `whiten/appear/disappear/escape/collapse/damage/blink_on/off/animation/loop_animation/update` (als `::Sprite`-Subklasse) |
| `RPG::Weather` | `type` (0 aus/1 Regen/2 Sturm/3 Schnee), `max`, `ox/oy`, `update`, `dispose` |
| Sonstiges | `RGSSError`, `Reset`, `rgss_main`/`rgss_stop` |

**Windowskin:** XP-Layout 192×128 (Hintergrund (0,0,128,128) gestreckt
oder gekachelt via `stretch`, Rahmen (128,0,64,64), Cursor (128,64,32,32),
Pause-Frames (160,64,32,32)). Gesucht wird in `Graphics/Windowskins/`,
`Graphics/System/`, `Graphics/` bzw. dem Projektordner; ohne Zuweisung gilt
der Name aus dem Datenbank-**System-Tab** (Standard `001-Blue01`).

**Beispiel:**
```ruby
@win = Window.new(80, 120, 480, 200)
@win.windowskin = RPG::Cache.windowskin("001-Blue01")
@win.contents.font.color.set(255, 255, 0)
@win.contents.draw_text(4, 4, 440, 32, "Hallo RGSS!")
```

**Bewusste Näherungen (funktional, visuell leicht anders als das Original):**
`Graphics.update`/`Input.update`/`Sprites#update` ticken framebasiert (die
Engine rendert im Hauptloop statt in Skriptschleifen); Transitions laufen
nicht-blockierend nebenbei; Viewport-Farbe/Ton wird als Overlay angenähert;
Text nutzt den eingebauten 8×8-Bitmap-Font (Public Domain, Daniel Hepper)
inkl. deutscher Umlaute statt System-TTFs.

**Bekannte Grenzen:** `load_data`/`save_data` und `Marshal` (rxdata) werden
nicht unterstützt – Datenbank = Engine-JSON, Laufzeitdaten über die
`$game_*`-Objekte, Spielstände über `Game.save(slot)`/`Game.load(slot)`.

Der Skript-Editor enthält fertige Vorlagen: **„Eigenes Menue (custom)"**,
**„Eigene Kampfszene (custom)"**, **„Kampf mit eigener Gegnerliste"**,
**„Eigener Titel (custom)"**, **„RGSS-Fenster (XP-Stil)"**, **„RGSS: Bitmap &
Sprite"** und **„RGSS: Viewport & Tilemap"**.

## Datenbank (XP-Dialog)

Ribbon **Werkzeuge → Datenbank** (oder der Button im Datenbank-Dock) öffnet den
**XP-artigen Datenbank-Dialog** mit den 13 Tabs **Akteure, Klassen,
Fertigkeiten, Gegenstände, Waffen, Rüstungen, Gegner, Trupps, Status,
Animationen, Tilesets, Gemeinsame Events und System** — links die nummerierte
Liste („001: …") mit **Maximum ändern …**, rechts die Eigenschaften, unten
**OK / Abbrechen / Anwenden**. Der System-Tab enthält wie in XP Anfangsgruppe,
Elemente (mit Maximum), Systemgrafiken, BGM/ME, 12 Soundeffekte und alle
Begriffe (HP, SP, STR … Ausrüsten). Die Anfangsgruppe steuert direkt die
Start-Party im Spiel. Alles wird im Projekt unter `database/*.json`
gespeichert.

## Sound-Test

Ribbon **Werkzeuge → Sound-Test** öffnet das XP-artige **Sound-Test-Fenster**:
Tabs **BGM / BGS / ME / SE**, Dateiliste mit „(Kein)", **Wiedergabe / Stopp**,
**Lautstärke-Regler** (0–100 %) und **Pitch-Regler** (50–150 %), beide greifen
live in die laufende Wiedergabe. Doppelklick auf einen Titel spielt ihn sofort
ab; beim Schließen wird automatisch gestoppt. Gescannt werden die Ordner
`Audio/BGM`, `Audio/BGS`, `Audio/ME`, `Audio/SE` (XP-Konvention) bzw.
`assets/audio/<typ>`; neue Projekte legen die XP-Ordner automatisch an.
Abspielbar: WAV, MP3, OGG, FLAC (MIDI nicht).

## Umlaute (ä ö ü Ä Ö Ü ß)

UI und Spiel sind UTF-8-durchgängig (MSVC baut mit `/utf-8`); Nachrichten,
Namen und Datenbanktexte dürfen Umlaute enthalten. In der Namenseingabe im
Spiel lassen sich Umlaute per **Alt+A / Alt+O / Alt+U** tippen.

## Lizenz

MIT – für kommerzielle und nicht-kommerzielle Projekte frei verwendbar.
