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
| `rui::Window` | Panel + Oeffnen/Schliessen-Animation (`openness` 0..255) |
| `rui::Manager` | Fenster-Liste (Z-Ordnung), Update(dt, Maus x/y/pressed), Draw |

## 3. Umstellung (laufend)

| Bildschirm | Status |
|---|---|
| Message + Auswahl (`rui.msgbox`) | **RUI** (PAKET 31) |
| Menue/Speicher/Shop/Name/Zahl/Kampf | ImGui-Overlay (Folgepakete nacheinander) |
| HUD, Pictures, Wetter, Farbton | ImGui-Overlay (unveraendert) |

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
