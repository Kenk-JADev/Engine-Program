# RUI — Eigenes UI-Framework (PAKET 31)

Entscheidung (Roadmap): **Eigenes UI-Framework mit Script-Windows.**
PAKET 31 legt das Fundament: die Retained-Window-Schicht in C++, die erste
umgestellte Bildschirm-Gruppe (Message + Auswahl) und natives Maus-Input.

## 1. Schichten

```
Spielzustand (GameUI-Klassen, bleiben die Fuehrungsgroesse)
        |  per Frame "Sync" (Widget-Baum wird aufgebaut/gefuettert)
        v
RUI-Widgets  (retained: Window/Panel/Label/ListView/Gauge)
        |  DrawTarget (reine Zeichen-Schnittstelle, kein IO!)
        v
Adapter: ImGui-DrawList   [spaeter: eigener GL-Batcher / RGSS-Canvas]
```

- **Retained**: Fenster existieren dauerhaft (`rui::Manager`), behalten
  Rect, Skin, Openness (XP-Oeffnen: vertikale Aufroll-Animation), Fokus.
- **Input nativ**: Tastatur (unveraendert `UpdateModalInput` etc.) UND
  **Maus** — Hover waehlt Zeilen, Click bestaetigt, Flaechen-Click spult
  Text vor. Damit ist der im Audit gefundene Deckel (ImGui bekam nie
  Maus/Tasten -> Klicks tot) behoben.
- **Theme**: `rui::Theme` (Farben, Padding, Zeilenhoehe, Blink-Rate);
  spaeter aus `Graphics/System/windowskin.png` / Projekt-Config.

## 2. API-Ueberblick (`include/rpgmaker3d/Rui.h`)

| Klasse | Zweck |
|---|---|
| `rui::Theme` | XP-anmutende Farben/Metriken (Standard-Instanz) |
| `rui::DrawTarget` | FillRect/StrokeRect/Text/Measure/Clip (Adapter-API) |
| `rui::Widget` | Basis: rect, visible, enabled, Draw, Maus-Hooks |
| `rui::Label` | Text (Farbe, Skalierung, Align, Wrap an Leerzeichen) |
| `rui::Gauge` | Balken (HP/MP), fract-gefuellt |
| `rui::Panel` | Container + optional Skin (Schatten/Face/Rahmen) + `onClick` |
| `rui::ListView` | Auswahl-Liste: Cursor mit Blinken, Scroll-Offset, `onPick`, `onHoverItem`, Maus |
| `rui::DigitRow` | PAKET 36: Ziffernzeile (Event 103) — Zellen, Cursor-Blinken, Click setzt Stelle |
| `rui::CharPad` | PAKET 36: Zeichentafel (Event 303) — Zeilen mit gleich breiten Zellen, Hover, Click |
| `rui::Banner` | PAKET 37: Text mit Anker+Pivot (0.5 = zentriert am Punkt) + optionalem Kasten |
| `rui::Custom` | PAKET 37: freie Draw-Callbacks (Wetter-Partikel, Farbschleier) |
| `rui::Window` | Panel + Oeffnen/Schliessen-Animation (`openness` 0..255) |
| `rui::Manager` | Fenster-Liste (Z-Ordnung), Update(dt, Maus x/y/pressed), Draw |

## 3. Umstellung (laufend)

| Bildschirm | Status |
|---|---|
| Message + Auswahl (`rui.msgbox`) | **RUI** (PAKET 31, Maus) |
| Menue/Titel/Speicher/Laden (`rui.menu`) | **RUI** (PAKET 34, Maus: Hover/Click) |
| XP-Kampfstatus (`rui.battlestatus`) | **RUI** (PAKET 34, Gauges + Faces) |
| Zahleneingabe 103 (`rui.numberinput`) | **RUI** (PAKET 36, DigitRow, Maus) |
| Namenseingabe 303 (`rui.nameinput`) | **RUI** (PAKET 36, CharPad, Maus) |
| Play-HUD (`rui.playhud`) | **RUI** (PAKET 37, eigener FPS-Schnitt) |
| Pictures (`rui.pic.*`) | **RUI** (PAKET 37, Rotation/Flash/Blinken) |
| Screen-Texts (`rui.stext.*`) | **RUI** (PAKET 37, Banner Anker+Pivot) |
| Farbton/Blitz (`rui.screenfx`) | **RUI** (PAKET 37, Vollbild-Schleier z=0) |
| Wetter (`rui.weather`) | **RUI** (PAKET 37, Line/FillCircle-Partikel) |

**PAKET 37 — UI.cpp ist ImGui-frei.** Alle Fenster und Overlays laufen
ueber RUI; ImGui ist nur noch ein OPTIONALER Zeichen-Adapter hinter
`rui::DrawTarget`. Dazu gehoeren:

- **Z-Ordnung** (`Window::z`, stabile Sortierung im Manager): Farbton 0,
  Wetter 10, Message 20, Kampfstatus 25, Pictures 30, Screen-Texts 40,
  Skript-Fenster 50 (Default), modale Fenster 60, Play-HUD 70 — unabhaengig
  von der Fenster-Anlegereihenfolge; Maus-Clicks gehen von oben nach unten.
- **Neue DrawTarget-Primitive**: `Line`, `FillCircle`, `Image` mit
  `rotationDeg` (Drehung um die Quad-Mitte); `imgW/imgH <= 0` = Vollbild-UV.
- **Neue Widgets**: `Banner` (Text mit Anker+Pivot und optionalem Kasten),
  `Custom` (freie Draw-Callbacks fuer Partikel/Schleier).
- **Eigene Uhren**: `rui::GetTime()` (Spieluhr, Partikel) statt
  `ImGui::GetTime()`; FPS als gleitender Schnitt statt `io.Framerate`.
- **DisplaySize ohne ImGui**: `GameUI::SetDisplaySize` aus `Engine::Render`
  (mWindow) — die ohne-ImGui-Build baut die Fensterlogik jetzt identisch
  auf (Zeichnung folgt mit dem GL-DrawTarget, PAKET 39).
- **Lebenszyklus**: Screen-Texts/Pictures/HUD/Schleier/Wetter entfernen
  ihre Fenster ueber ID-Buchfuehrung bzw. Frueh-Return + RemoveWindow
  (PAKET-36-Muster); `SetPlaying(false)` raeumt RUI beim Playtest-Stopp.

PAKET 36 (Fenster-Lebenszyklus-Fixes):
- `DrawModalWindows` ruft jetzt IMMER alle Sync-Funktionen — jede verwaltet
  ihr retained Fenster selbst (RemoveWindow sobald inaktiv). Der fruehere
  Frueh-Return liess z.B. `rui.menu` nach dem Schliessen als Geisterfenster
  stehen.
- `GameUI::Draw` ruft `MessageWindow::Draw` jetzt immer auf (nicht nur bei
  Sichtbarkeit), damit die Schliess-Animation der Box wirklich laeuft.
- `rui.msgbox`: Flaechen-`onClick` einer frueheren Nachricht ueberlebte
  `children.clear()` und haette bei aktiven Choices die Auswahl wegwerfen
  koennen — wird jetzt mit Choices explizit zurueckgesetzt.

PAKET 33: Windowskin-PNG (Nine-Patch, XP-96x96-Rahmenflaeche) im Skin-Slot
von `Theme`; Auto-Suche `Graphics/System/windowskin.*`, Script-Override
via `Rui.windowskin = "name"`. Flaechen-/Flat-Skin bleibt Fallback.

Widget-Sync-Konvention: Der Spielzustand bleibt in den GameUI-Klassen;
die Draw-Funktion baut den Widget-Baum pro Frame neu auf (Container wie
`rui.msgbox` bleiben retained, inkl. Openness/Z-Ordnung).

## 4. Script-Windows (PAKET 32 — umgesetzt)

Ruby-Zugriff liegt direkt auf den C++-Widgets (`Rui::Window/Label/Gauge/
ListView`), inkl. Bloecke fuer Pick/Cancel/Hover und Tastatur-Fokus
(`Rui.set_focus_list` — die Engine sperrt dann Spiel-Eingaben):

```ruby
win = Rui.window("hud.hp", 10.0, 10.0, 240.0, 90.0)
win.openness = 0.0
win.open            # XP-Aufroll-Animation
win.add_label("t", "HP", 8.0, 4.0)
@hp = win.add_gauge("hp", 8.0, 26.0, 180.0, 10.0, 85, 100, "hp")
```

Vollstaendige API + Beispiele: `docs/SCRIPT-RUI.md`;
Demo: `SampleProject/scripts/17_Rui_Demo.rb`.
