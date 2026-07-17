# Event-System (XP-Befehlssatz)

Die Engine besitzt einen **vollständigen RPG-Maker-XP-Event-Interpreter**
(siehe `include/rpgmaker3d/EventSystem.h`, `src/EventSystem.cpp`; Referenz:
`XP_Scripts/Interpreter 1-7.rb`). Der Qt-Editor (`qt_editor/QtEventEditorDialog`)
baut Events in exakt derselben Kodierung, daher sind Editor und Laufzeit immer
synchron.

## Aufbau eines Events

```
MapEvent
 ├─ id, name, x/y/z
 └─ pages[] (EventPage)
     ├─ condition      (Schalter 1/2, Variable ≥ Wert, Selbstschalter A–D)
     ├─ graphicName / graphicIndex
     ├─ moveType (0 Fest, 1 Zufällig, 2 Annähern, 3 Benutzerdefiniert)
     ├─ moveSpeed/moveFrequency (1..6)
     ├─ customRoute ("R W40 L W40"), routeRepeat, routeSkippable
     ├─ walkAnime, stepAnime, directionFix, through, alwaysOnTop
     ├─ trigger (1 Aktionstaste, 2 Spieler berührt, 3 Event berührt,
     │           4 Automatisch, 5 Parallel)
     └─ list[] (EventCommand: code, indent, param1..3, text, parameters[])
```

**Seitenauswahl:** XP-Regel – die *letzte* Seite, deren Bedingungen alle
zutreffen, ist aktiv. `RefreshAllPages()` läuft nach jeder
Schalter/Variablen-Änderung.

## Befehls-Liste (Codes)

### Seite 1 – Nachricht / Ablauf / Spielstatus
| Code | Befehl | Kodierung |
|---|---|---|
| 101 | Text zeigen | `text` = erste Zeile; Folgezeilen = 401 |
| 102 | Auswahl zeigen | `text` = `"Frage\|Opt1\|Opt2…"`, `param1` = Anzahl, `param2` = Abbruch (0 verboten, 1–4 Index, 5 Abbruch-Zweig); `parameters[0..3]` = Optionen, `[4]` = Fragetext |
| 103 | Zahlen eingeben | `param1` = Variable, `param2` = Stellen |
| 104 | Text-Optionen | `param1` = Position, `param2` = Fenster |
| 105 | Tastenabfrage | `param1` = Zielvariable (XP-Tastencode) |
| 106 | Warten | `param1` = Frames (40 fps) |
| 108 | Kommentar | `text`, Folgezeilen = 408 |
| 111 | Bedingung | `param1` = Typ 0–12 (s. unten) |
| 112 / 413 | Schleife / von oben | Marker |
| 113 | Schleife verlassen | – |
| 115 | Event-Verarbeitung beenden | – |
| 116 | Event löschen | bis Karten-Reload |
| 117 | Gemeinsames Event | `param1` = CE-Id (Child-Interpreter, Tiefe ≤ 100) |
| 118 / 119 | Marke / Springe | `text` = Name |
| 121 | Schalter steuern | `param1/param2` = von..bis, `param3` = 1 AN / 0 AUS |
| 122 | Variablen steuern | `param1/param2` = von..bis, `param3` = Op (0=,1+,2−,3*,4/,5%), `parameters[0]` = Wert-Art (0 Konst, 1 Variable, 2 Zufall), `[1]` = Wert/VarId/Min, `[2]` = Max |
| 123 | Selbstschalter | `text` = A–D, `param3` = 1 AN / 0 AUS |
| 124 | Timer | `param1` = 0 Start/1 Stopp, `param2` = Sekunden |
| 125 | Geld | `param1` = ±Betrag |
| 126/127/128 | Gegenstand/Waffe/Rüstung | `param1` = Id, `param2` = ±Anzahl |
| 129 | Gruppenmitglied | `param1` = Akteur, `param3` = 0 hinzu / 1 entfernen |
| 131–133 | Windowskin/Kampf-BGM/Sieg-ME | `text` = Datei |
| 134/135/136 | Speichern/Menü/Zufallskämpfe | `param1` = 1 erlaubt / 0 gesperrt |

### Seite 2 – Bewegung / Bildschirm / Bilder / Audio
| Code | Befehl | Kodierung |
|---|---|---|
| 201 | Spieler transferieren | `param1` = X, `param2` = Z, `param3` = Map (0 = aktuell) |
| 202 | Event-Position setzen | `param1` = Event-Id, `param2` = X, `param3` = Z |
| 208 | Spieler-Transparenz | `param1` = 0 normal / 1 transparent |
| 209 | Bewegungsroute | `param1` = Ziel (0 dieses, −1 Spieler), `param2` = Flags (1 wiederholen, 2 überspringbar, 4 warten), `text` = Route |
| 210 | Auf Bewegung warten | – |
| 223 | Bildschirm-Farbton | `param1..3` = -255..255, `parameters[0]` = Grau, `[1]` = Sekunden |
| 224 | Bildschirm-Blitz | `param1..3` = R/G/B, `parameters[0]` = Stärke, `[1]` = Sekunden |
| 225 | Bildschirm-Beben | `param1` = Stärke, `param2` = Tempo, `param3` = warten, `parameters[0]` = Sekunden |
| 231–235 | Bilder | Position/Farbton/Löschen |
| 236 | Wetter | `param1` = Typ, `param2` = Stärke |
| 241–251 | BGM/BGS/ME/SE | `text` = Datei, `param1` = Sekunden (Fade) |

### Seite 3 – Kampf / Akteure / System
| Code | Befehl | Kodierung |
|---|---|---|
| 301 | Kampf verarbeiten | `param1` = Truppe, `param2` = Flags (1 Flucht, 2 Niederlage) |
| 601/602/603 | Wenn Sieg/Flucht/Niederlage | gleicher `indent` wie 301 |
| 302 | Laden | `text` = `"1,2,3"` Item-Ids |
| 303 | Namenseingabe | `param1` = Akteur, `param2` = max. Zeichen |
| 311–322 | Akteur ändern | `param1` = Akteur (0 = Gruppe), je Befehl `param2/param3/text` |
| 331–340 | Gegner / Kampfablauf | im Kampfzustand |
| 351/352 | Menü/Speichern öffnen | – |
| 353/354 | Game Over / Titel | – |
| 355 | Skript (Ruby) | `text`, Folgezeilen = 655 |

### Strukturelle Codes (vom Editor automatisch verwaltet)
`401` Textzeile, `402` Wenn [x], `403` Wenn Abbruch, `404` Ende der Auswahl,
`408` Kommentarzeile, `411` Sonst, `412` Ende der Bedingung, `413` von oben,
`655` Skriptzeile.

> **Wichtig (XP-Konvention):** „Wenn“-Köpfe (402/403/411) haben **denselben
> Einzug** wie ihr Kopfbefehl (102/111). Nur der Zweig-Body ist tiefer
> eingerückt. Der Interpreter vergleicht `mBranch[indent]`.

### Bedingungstypen (111, `param1`)
| Typ | Bedingung | param2 | param3 | parameters |
|---|---|---|---|---|
| 0 | Schalter | Id | 0 = ist AN, 1 = ist AUS | – |
| 1 | Variable | Id | Vergleich 0==,1>=,2<=,3>,4<,5!= | [0] Art, [1] Wert/VarId/Min, [2] Max |
| 2 | Selbstschalter | – | 0 = AN, 1 = AUS | `text` = A–D |
| 3 | Timer | Sekunden | 0 = ≥, 1 = ≤ | – |
| 4 | Akteur | Id | 0 Gruppe, 1 Name, 2 Fertigkeit, 3 Waffe, 4 Rüstung, 5 Status | [0] Wert |
| 5 | Gegner | Index | 0 erschienen | – |
| 6 | Event | Id (0 = dieses) | Richtung 2/4/6/8 | – |
| 7 | Geld | Betrag | 0 = ≥, 1 = ≤ | – |
| 8/9/10 | Gegenst./Waffe/Rüst. | Id | – | – |
| 11 | Taste | XP-Code | – | – |
| 12 | Skript | – | – | `text` = Ruby-Ausdruck |

**XP-Tastencodes:** `2/4/6/8` = unten/links/rechts/oben, `11` = A (Umschalt),
`12` = B (Esc), `13` = C (Eingabe/E/Leertaste), `15` = L (Q), `16` = R (Tab).

### Engine-Erweiterungen (3D)
| Code | Befehl |
|---|---|
| 181 | HUD-Text zeigen (früher 104 – Alt-Dateien werden beim Laden gemappt) |
| 182 | Welt-Text zeigen |
| 183 | HUD-Texte löschen |
| 500–507 | Objekt spawnen/bewegen/drehen, Animation, Schaden, Partikel, Tageszeit, Wetter |

Alt-Projekte (`formatVersion < 2` in `events_map<N>.json`) werden automatisch
konvertiert (104→181, 105→182, 106→183, 205→209, 230→106).

## Interpreter-Hinweise
- Max. 100 Befehle pro Frame (XP-Frame-Cap), Endlosschleifen sind sicher.
- Action-/Autorun-Events **sperren** die Spielerbewegung, Parallel-Events nicht.
- `Warten` zählt Frames bei 40 fps (XP), Warte-States blockieren den Interpreter.
- `Skript` (355) läuft über den Script-Runner (mruby, falls aktiviert).

## Playtest
Editor: **F5** = Projekt speichern + `RPGMaker3D_Player[.exe] --project <Pfad>`
(startet exakt wie bei Spielern). **Umschalt+F5** = schneller eingebetteter Test.
Player direkt: `RPGMaker3D_Player --project SampleProject` oder
`RPGMaker3D_Player SampleProject`.
