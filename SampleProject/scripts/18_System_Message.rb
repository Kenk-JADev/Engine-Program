# ============================================================================
# 18_System_Message.rb - Standard-Dialoge als Ruby-System (PAKET 42)
# ============================================================================
# XP-Philosophie: WIE Text (101), Auswahl (102), Zahl (103) und Name (303)
# aussehen und sich anfuehlen, ist SPIEL-CODE und gehoert in den Script-
# Editor - nicht in die Engine. Dieses Skript ist die Engine-Referenz:
# es ersetzt die eingebauten Fenster mit derselben Warte-Semantik fuer den
# Event-Interpreter und ist bewusst schlicht gehalten, damit es als
# Startpunkt fuer eigene Systeme dient (Typewriter-Tempo, Skins, Fenster-
# layout, Gesichter, Sounds - alles hier aenderbar).
#
# AKTIVIEREN: in 00_Config.rb SCRIPT_MESSAGE_SYSTEM = true setzen
# (setzt UI.native_message = false; die Engine ruft dann Game.on_ui_*.
#  Fehlen die Hooks, bleibt automatisch alles nativ.)
#
# WICHTIG: in den Hooks NICHT UI.show_message() aufrufen - das wuerde
# wieder bei on_ui_message landen (Endlosschleife). Die Fenster werden
# direkt aus Rui-Primitiven gebaut.
#
# Ruecklieferung an die Engine (Pflicht, sonst wartet das Event ewig):
#   UI.deliver_message_done       101 gelesen/geschlossen
#   UI.deliver_choice(index|-1)   102 Auswahl (-1 = Abbruch, Engine mappt
#                                 Abbruchverhalten wie beim nativen Fenster)
#   UI.deliver_number(wert)       103 Zahl
#   UI.deliver_name(string)       303 Name
# ============================================================================

if defined?(Rui) && defined?(UI)

module SystemMessage
  WIN_MSG    = "sys.msg"
  WIN_CHOICE = "sys.choice"
  WIN_NUMBER = "sys.number"
  WIN_NAME   = "sys.name"

  @kind    = nil      # nil | :message | :choices | :number | :name
  # :message (Typewriter)
  @full    = ""
  @shown   = 0        # bereits sichtbare Bytes (Codepoint-sicher)
  @textY   = 0.0      # Textzeile (unter dem Sprecher, wenn vorhanden)
  # :number
  @ndigits = 4
  @nvalue  = 0
  @ncursor = 0        # Zehnerpotenz von RECHTS (0 = Einer, wie XP/Engine)
  # :name
  @ntext   = ""
  @nmax    = 8
  @ninitial = ""

  # --- kleine Helfer --------------------------------------------------------
  SW = (defined?(RPGMaker3D::Config::SCREEN_WIDTH) ?
        RPGMaker3D::Config::SCREEN_WIDTH : 1280).to_f
  SH = (defined?(RPGMaker3D::Config::SCREEN_HEIGHT) ?
        RPGMaker3D::Config::SCREEN_HEIGHT : 720).to_f

  # Byte-Laenge des UTF-8-Codepoints an Position i (Typewriter darf keine
  # Umlaute zerreissen - sonst Glitches mitten in der Typ-Animation).
  def self.utf8_step(s, i)
    c = s.getbyte(i)
    return 1 if c < 0x80
    return 2 if (c & 0xE0) == 0xC0
    return 3 if (c & 0xF0) == 0xE0
    return 4 if (c & 0xF8) == 0xF0
    1
  end

  # UTF-8-sicher ein Zeichen am Ende wegnehmen (Backspace-Regel der Engine)
  def self.pop_char(s)
    return s if s.length == 0
    begin
      b = s.getbyte(s.length - 1)
      s = s[0, s.length - 1]
      keep = ((b & 0xC0) == 0x80) # Fortsetzungsbyte -> weiter loeschen
    end while keep && s.length > 0
    s
  end

  def self.close_win(id)
    w = Rui.find(id)
    w.close if w
  end

  def self.reset
    [WIN_MSG, WIN_CHOICE, WIN_NUMBER, WIN_NAME].each { |id| close_win(id) }
    Rui.clear_focus
    @kind = nil
  end

  # --- 101: Text ------------------------------------------------------------
  # position: 0 = unten / 1 = Mitte / 2 = oben (wie der native Befehl).
  # face ist hier bewusst ungenutzt - Gesichter sind ein guter eigener
  # Ausbau-Punkt (z. B. mit UI.show_picture aus Graphics/Faces/).
  def self.on_message(text, speaker, position, face)
    reset
    @kind  = :message
    @full  = text.to_s
    @shown = 0
    @textY = speaker.to_s.empty? ? 2.0 : 26.0
    w  = SW * 0.72
    h  = 150.0
    x  = (SW - w) * 0.5
    y  = (position == 2) ? 24.0 : (position == 1) ? (SH - h) * 0.5 : SH - h - 28.0
    win = Rui.window(WIN_MSG, x, y, w, h)
    win.openness = 0.0
    win.open # XP-Aufrollen
    unless speaker.to_s.empty?
      win.add_label("speaker", speaker, 2.0, 0.0, 0, 1.0)
    end
    win.add_label("text", "", 2.0, @textY, 0, 1.0)
    win.add_label("hint", "E / Enter / Space", w - 180.0, h - 58.0, 2, 1.0)
    refresh_message
  end

  def self.refresh_message
    win = Rui.find(WIN_MSG)
    return unless win
    # Text aktualisieren: Widget derselben Id ersetzen (retained)
    win.remove_widget("text") rescue nil
    visible = @full[0, @shown]
    win.add_label("text", visible, 2.0, @textY, 0, 1.0)
  end

  TYPESPEED = 2 # Codepoints pro Frame-Takt (60/s => ~120 Zeichen/s)

  def self.update_message
    # Typewriter (Tempo = TYPESPEED, eigener Custom-Punkt)
    if @shown < @full.length
      TYPESPEED.times do
        break if @shown >= @full.length
        @shown += utf8_step(@full, @shown)
      end
      refresh_message
      return
    end
    if Input.key_pressed?(:e) || Input.key_pressed?(:return) ||
       Input.key_pressed?(:space)
      # fertig -> Fenster zu, Interpreter weiterlaufen lassen
      close_win(WIN_MSG)
      @kind = nil
      UI.deliver_message_done
    end
  end

  # --- 102: Auswahl ---------------------------------------------------------
  def self.on_choices(text, options, cancel_allowed)
    reset
    @kind = :choices
    rows  = [options.size, 6].min
    w  = SW * 0.40
    h  = 2 * 14.0 + (text.to_s.empty? ? 0.0 : 26.0) + rows * 22.0 + 26.0
    x  = (SW - w) * 0.5
    y  = SH - h - 28.0
    win = Rui.window(WIN_CHOICE, x, y, w, h)
    win.add_label("q", text.to_s, 2.0, 0.0, 0, 1.0) unless text.to_s.empty?
    list = win.add_list("opts", options, 2.0, text.to_s.empty? ? 4.0 : 26.0,
                        w - 40.0, rows * 22.0)
    win.add_label("hint", cancel_allowed ?
      "W/S waehlen | E/Enter/Click | Esc Abbruch" :
      "W/S waehlen | E/Enter/Click", 2.0, h - 46.0, 0, 1.0)
    list.on_pick do |idx|
      close_win(WIN_CHOICE)
      Rui.clear_focus
      @kind = nil
      UI.deliver_choice(idx)
    end
    list.on_cancel do
      if cancel_allowed
        close_win(WIN_CHOICE)
        Rui.clear_focus
        @kind = nil
        UI.deliver_choice(-1)
      end
    end
    Rui.set_focus_list(WIN_CHOICE, "opts")
  end

  # --- 103: Zahl ------------------------------------------------------------
  def self.on_number(prompt, digits, initial)
    reset
    @kind    = :number
    @ndigits = (digits < 1 || digits > 8) ? 4 : digits
    max = 1
    @ndigits.times { max *= 10 }
    @nvalue  = initial % max
    @nvalue  = 0 if @nvalue < 0
    @ncursor = 0 # XP: Cursor startet auf der Einer-Stelle (rechts)
    w = SW * 0.30
    h = 120.0
    x = (SW - w) * 0.5
    y = SH * 0.30
    win = Rui.window(WIN_NUMBER, x, y, w, h)
    win.add_label("cap", prompt.to_s.empty? ? "Zahleneingabe" : prompt,
                  2.0, 0.0, 0, 1.0)
    win.add_label("hint", "A/D Stelle | W/S Ziffer | Enter ok", 2.0,
                  h - 44.0, 0, 1.0)
    refresh_number
  end

  def self.refresh_number
    win = Rui.find(WIN_NUMBER)
    return unless win
    # Zahl ohne sprintf: Ziffern-String von links (hoechste Stelle zuerst)
    s = @nvalue.to_s
    while s.length < @ndigits
      s = "0" + s
    end
    win.remove_widget("digits") rescue nil
    win.remove_widget("caret") rescue nil
    # Monospace (RUI-Font): Ziffern mit Leerzeichen, Caret darunter.
    # Sichtbare Spalte von links = Stellen-1-cursor (links = hoechste
    # Stelle). WICHTIG: Caret im GLEICHEN Masstab wie die Ziffern (1.5),
    # sonst laufen die Zeichenbreiten auseinander.
    spaced = ""
    s.length.times { |i| spaced += (i > 0 ? " " : "") + s[i, 1] }
    col = @ndigits - 1 - @ncursor
    caret = ""
    (col * 2).times { caret += " " }
    caret += "^"
    win.add_label("digits", spaced, 6.0, 26.0, 0, 1.5)
    win.add_label("caret", caret, 6.0, 44.0, 0, 1.5)
  end

  def self.digit_at(pos) # pos = Zehnerpotenz von rechts
    div = 1
    pos.times { div *= 10 }
    (@nvalue / div) % 10
  end

  def self.set_digit(pos, d)
    div = 1
    pos.times { div *= 10 }
    @nvalue += (d - digit_at(pos)) * div
  end

  def self.update_number
    # Wie die native Eingabe/XP: cursor zaehlt die Zehnerpotenz von RECHTS.
    # Links = hoehere Stelle (cursor+1), Rechts = niedrigere (cursor-1).
    if Input.key_pressed?(:a) || Input.key_pressed?(:left)
      @ncursor = (@ncursor + 1) % @ndigits
      refresh_number
    elsif Input.key_pressed?(:d) || Input.key_pressed?(:right)
      @ncursor = (@ncursor + @ndigits - 1) % @ndigits
      refresh_number
    elsif Input.key_pressed?(:w) || Input.key_pressed?(:up)
      set_digit(@ncursor, (digit_at(@ncursor) + 1) % 10)
      refresh_number
    elsif Input.key_pressed?(:s) || Input.key_pressed?(:down)
      set_digit(@ncursor, (digit_at(@ncursor) + 9) % 10)
      refresh_number
    elsif Input.key_pressed?(:e) || Input.key_pressed?(:return) ||
          Input.key_pressed?(:space)
      close_win(WIN_NUMBER)
      @kind = nil
      UI.deliver_number(@nvalue)
    end
  end

  # --- 303: Name ------------------------------------------------------------
  NAME_CELLS = (("A".."Z").to_a + ("a".."z").to_a + ("0".."9").to_a +
    ["\xC3\x84", "\xC3\x96", "\xC3\x9C", "\xC3\xA4", "\xC3\xB6",
     "\xC3\xBC", "\xC3\x9F", " ", "-"]).freeze
  CMD_DELETE   = "(Loeschen)"
  CMD_DONE     = "(Fertig)"

  def self.on_name(prompt, initial, max_chars)
    reset
    @kind    = :name
    @nmax    = (max_chars < 1 || max_chars > 16) ? 8 : max_chars
    @ninitial = initial.to_s
    @ntext   = initial.to_s
    w = SW * 0.36
    h = 240.0
    x = (SW - w) * 0.5
    y = SH * 0.16
    win = Rui.window(WIN_NAME, x, y, w, h)
    win.add_label("cap", prompt.to_s.empty? ? "Namenseingabe" : prompt,
                  2.0, 0.0, 0, 1.0)
    list = win.add_list("cells", NAME_CELLS + [CMD_DELETE, CMD_DONE],
                        2.0, 60.0, w - 40.0, h - 120.0)
    win.add_label("hint", "W/S waehlen | E/Enter Buchstabe | Esc behalten",
                  2.0, h - 34.0, 0, 1.0)
    list.on_pick do |idx|
      SystemMessage.name_pick(idx)
    end
    list.on_cancel do
      close_win(WIN_NAME)
      Rui.clear_focus
      @kind = nil
      UI.deliver_name(@ninitial) # Abbruch = bisheriger Name bleibt
    end
    Rui.set_focus_list(WIN_NAME, "cells")
    refresh_name
  end

  def self.refresh_name
    win = Rui.find(WIN_NAME)
    return unless win
    win.remove_widget("cur") rescue nil
    show = @ntext.empty? ? "_" : @ntext
    win.add_label("cur", show + "_", 6.0, 28.0, 0, 1.25)
  end

  def self.name_pick(idx)
    cells = NAME_CELLS + [CMD_DELETE, CMD_DONE]
    cell = cells[idx]
    return unless cell
    if cell == CMD_DELETE
      @ntext = pop_char(@ntext)
    elsif cell == CMD_DONE
      close_win(WIN_NAME)
      Rui.clear_focus
      @kind = nil
      UI.deliver_name(@ntext.empty? ? @ninitial : @ntext)
      return
    else
      # Byte-Budget wie die native Eingabe (Umlaute = 2 Bytes)
      @ntext += cell if @ntext.length + cell.length <= @nmax
    end
    refresh_name
  end

  # --- Frame-Pumpe (aus main.rb gerufen) ------------------------------------
  def self.update
    case @kind
    when :message then update_message
    when :number  then update_number
    end
    # :choices/:name laufen ueber die Rui-Fokusliste (Enter/Esc/Maus)
  end
end

# Engine-Hooks (werden von C++ gerufen, wenn UI.native_message == false)
class Game
  def self.on_ui_message(text, speaker, position, face)
    SystemMessage.on_message(text, speaker, position, face)
  end
  def self.on_ui_choices(text, options, cancel_allowed)
    SystemMessage.on_choices(text, options, cancel_allowed)
  end
  def self.on_ui_number(prompt, digits, initial)
    SystemMessage.on_number(prompt, digits, initial)
  end
  def self.on_ui_name(prompt, initial, max_chars)
    SystemMessage.on_name(prompt, initial, max_chars)
  end
end

# Opt-in aus 00_Config.rb (Standard: natives System bleibt aktiv)
if defined?(RPGMaker3D::Config::SCRIPT_MESSAGE_SYSTEM) &&
   RPGMaker3D::Config::SCRIPT_MESSAGE_SYSTEM
  UI.native_message = false
  Engine.log("System-Dialoge per Ruby aktiv (18_System_Message.rb)")
end

end # defined?(Rui) && defined?(UI)
