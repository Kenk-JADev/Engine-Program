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
- `third_party/` – glad, stb_image, miniaudio, RmlUi, …
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

Alles ist jetzt **im Player sichtbar und spielbar** (RmlUi-Anzeige,
Pfeiltasten/W-S wählen, E/Enter bestätigen, Esc zurück):

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
  Max-HP/MP werden aus den Datenbank-Startwerten hochgerechnet
  (+5 %/Level, bis ein Kurven-Editor existiert). Respektiert die
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
  Preis, Käufe ziehen das Gold sofort ab und legen den Gegenstand ins
  Inventar. Bei zu wenig Gold ertönt der Buzzer-SE.
- **Sichtbar gemacht**: Auch **Auswahl (102)**, **Zahleneingabe (103)** und
  **Namenseingabe (303)** zeigen jetzt ein eigenes XP-Fenster im Player
  (vorher nur Tastatursteuerung ohne Anzeige). **Bilder** (231,
  `UI.show_picture`) werden ebenfalls im Player dargestellt; gesucht wird in
  `<Projekt>/Graphics/Pictures|Titles/` (XP-Struktur), `assets/pictures/`
  u. a. (png/jpg/jpeg/bmp/tga).
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
| `UI.gold`, `UI.add_gold(n)` | Gold-Anzeige lesen/ändern |

### Weitere Module

- **Game** (Komfort-API): `Game.save(slot)` / `Game.load(slot)` – Datei
  `<Projekt>/saves/save<N>.json`; `Game.start_battle(trupp_id)`,
  `Game.in_battle?`, `Game.map_id`, `Game.setup_map(id)`.
- **Audio**: `Audio.bgm_play("name.ogg")` sowie BGS/ME/SE und Fades.
- **Input**: `Input.key_down?(:return)` & Co.
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
