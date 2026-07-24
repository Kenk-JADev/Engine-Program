# RUI aus Ruby — Script-Windows API (PAKET 32)

Eigene Fenster direkt aus Spielskripten bauen. Die Objekte leben in der
C++-Window-Schicht (`rui::Manager`); Ruby-Handles adressieren sie ueber
IDs — Fenster ueberleben Szenenwechsel nicht automatisch (Scenes raeumen
in `terminate` auf, siehe `SampleProject/scripts/17_Rui_Demo.rb`).

> Laedt nur, wenn die Engine mit mruby gebaut ist. Code defensiv halten:
> `if defined?(Rui)`.

## Modul `Rui`

| Aufruf | Bedeutung |
|---|---|
| `Rui.window(id, x, y, w, h)` | Fenster anlegen/austauschen → `Rui::Window` |
| `Rui.find(id)` | vorhandenes Fenster-Handle oder `nil` |
| `Rui.clear` | alle Fenster entfernen (+ Fokus loesen) |
| `Rui.set_focus_list(win_id, list_id)` | Liste bekommt Tastatur-Fokus (Pfeil hoch/runter waehlen, Enter/E bestatigen) |
| `Rui.clear_focus` | Fokus aufloesen |
| `Rui.has_focus?` | true solange fokussiert (die Engine sperrt dann Interagieren/Menues) |
| `Rui.windowskin = "name"` | Windowskin aus `Graphics/System/` (oder Flat mit `""`/`nil`) (PAKET 33) |
| `Rui.windowskin` | aktueller Skin-Quellpfad |
| `Rui.theme_color("accent")` | Farbe lesen → `[r, g, b, a]` (0..255) (PAKET 38) |
| `Rui.set_theme_color("text", r, g, b[, a])` | Farbe setzen (0..255) — wirkt ab dem naechsten Frame (PAKET 38) |
| `Rui.theme_metric("row_height")` | Metrik lesen (Float, px) (PAKET 38) |
| `Rui.set_theme_metric("padding", 12.0)` | Metrik setzen — wirkt ab dem naechsten Frame (PAKET 38) |

### Theme-Namen (PAKET 38)

Farben: `face`, `face_shadow`, `border`, `text`, `text_disabled`,
`accent`, `cursor_bg`, `gauge_hp`, `gauge_mp`.
Metriken: `padding`, `border_width`, `rounding`, `shadow`, `row_height`,
`blink_hz`. Unbekannte Namen geben `nil` zurueck (kein Raise).
Ein Skript kann so eigene Themes bauen, z. B. ein helleres Arena-Theme
fuer Boss-kaempfe und blasse Nacht-Farben bei Aussenkarten.

## `Rui::Window`

- Geometrie: `x`, `y`, `width`, `height` (Getter+Setter), `move(x, y, w, h)`
- Oeffnen wie XP: `openness` (0..255), `open`, `close` (aufrollende
  Zu-Animation, Fenster raeumt sich danach selbst aus), `closing?`,
  `fully_open?`
- Sichtbarkeit: `visible`, `visible=`
- Inhalt (Koordinaten relativ zum Fenster, Innenrand `padding` wird
  automatisch addiert):
  - `add_label(id, text, x=0, y=0, align=0, scale=1.0)` → `Rui::Label`
  - `add_gauge(id, x, y, w, h, current=0, maximum=100, kind="hp")` → `Rui::Gauge`
    (`kind: "hp"` gruen / `"mp"` blau, danach frei einfaerbbar)
  - `add_list(id, items, x=0, y=0, w=100, h=100)` → `Rui::ListView`
    (`items`: Array von `"Text"` oder `["Text", enabled_bool]`)
  - `remove_widget(id)`
- `destroy` — sofort entfernen (ohne Animation)

## `Rui::Label`
`text`, `text=`, `set_color(r, g=1, b=1, a=1)`

## `Rui::Gauge`
`current`, `current=`, `maximum`, `maximum=`, `set_color(r, g, b, a)`

## `Rui::ListView`
- `selected`, `selected=`, `items=` (Array-Format wie `add_list`)
- `on_pick { |idx| ... }` — Bestatigung (Maus-Click ODER Tastatur-Enter bei Fokus)
- `on_cancel { ... }` — Abbruch (Esc bei Fokus)
- `on_hover { |idx| ... }` — Maus-Hover

Deaktivierte Eintraege (`["Name", false]`) sind nicht waehlbar und werden
bei der Fokus-Navigation uebersprungen.

## Beispiel

```ruby
win = Rui.window("hud", 10.0, 10.0, 240.0, 90.0)
@gold = win.add_label("gold", "Gold: 0", 8.0, 4.0)
@hp = win.add_gauge("hp", 8.0, 26.0, 180.0, 10.0, 85, 100, "hp")

def show_menu
  w = Rui.window("menu", 170.0, 150.0, 260.0, 130.0)
  w.openness = 0.0
  w.open # Aufroll-Animation
  list = w.add_list("m", ["Hallo", "Gold +10", "Zurueck"], 8.0, 8.0, 220.0, 96.0)
  list.on_pick do |idx|
    UI.show_message("Auswahl #{idx}")
    w.close
    Rui.clear_focus
  end
  list.on_cancel do
    w.close
    Rui.clear_focus
  end
  Rui.set_focus_list("menu", "m")
end
```
