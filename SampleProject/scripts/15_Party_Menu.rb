# Esc-Party-Menue (RPG-Maker-Style) - rein in Ruby.
# PAKET 26: Standardmaessig INAKTIV, solange das native Engine-Menue
# laeuft (00_Config.rb: USE_NATIVE_MENU = true) - sonst doppeltes Menue.
# Zum Umstellen: USE_NATIVE_MENU = false setzen.
module PartyMenu
  @open = false
  @index = 0
  @mode = :main # :main :items
  @lock = 0
  OPTIONS = ["Items", "Status", "Speichern", "Schliessen"].freeze

  def self.open?
    @open
  end

  def self.open
    # Natives Engine-Menue aktiv? Dann dieses Ruby-Menue nicht oeffnen
    # (PAKET 26 - doppelte Esc-Menues verhindern)
    if defined?(RPGMaker3D::Config::USE_NATIVE_MENU) &&
       RPGMaker3D::Config::USE_NATIVE_MENU
      @open = false
      return
    end
    @open = true
    @index = 0
    @mode = :main
    @lock = 15
    redraw
  end

  def self.close
    @open = false
    UI.clear_texts
  end

  def self.redraw
    UI.clear_texts
    return unless @open
    UI.show_screen_text("=== MENUE ===", 0.5, 0.12, 1.0, 0.85, 0.3, 0.0)
    if @mode == :main
      OPTIONS.each_with_index do |opt, i|
        mark = (i == @index) ? "> " : "  "
        col = (i == @index) ? [1.0, 1.0, 0.4] : [0.85, 0.85, 0.85]
        UI.show_screen_text("#{mark}#{opt}", 0.5, 0.28 + i * 0.08, col[0], col[1], col[2], 0.0)
      end
      g = (UI.gold rescue 0)
      UI.show_screen_text("Gold: #{g}", 0.5, 0.72, 1.0, 0.9, 0.3, 0.0)
      UI.show_screen_text("W/S waehlen  E/Enter OK  Esc zu", 0.5, 0.88, 0.6, 0.7, 0.8, 0.0)
    elsif @mode == :items
      UI.show_screen_text("Items (E benutzt Potion falls vorhanden)", 0.5, 0.3, 0.9, 0.9, 0.9, 0.0)
      UI.show_screen_text("Esc = zurueck", 0.5, 0.45, 0.7, 0.7, 0.7, 0.0)
    elsif @mode == :status
      UI.show_screen_text("Party-Status", 0.5, 0.28, 1.0, 0.9, 0.5, 0.0)
      UI.show_screen_text("Hero – siehe Database Actors", 0.5, 0.4, 0.85, 0.85, 0.85, 0.0)
      UI.show_screen_text("Esc = zurueck", 0.5, 0.55, 0.7, 0.7, 0.7, 0.0)
    end
  end

  def self.update
    @lock -= 1 if @lock && @lock > 0
    # Toggle Esc
    if Input.key_down?(:escape) && (!@lock || @lock <= 0)
      if @open && @mode != :main
        @mode = :main
        @lock = 12
        redraw
        return
      end
      if @open
        close
      else
        open
      end
      @lock = 12
      return
    end
    return unless @open
    return if @lock && @lock > 0

    if @mode == :main
      if Input.key_down?(:w) || Input.key_down?(:up)
        @index = (@index - 1) % OPTIONS.size
        @lock = 10
        redraw
      elsif Input.key_down?(:s) || Input.key_down?(:down)
        @index = (@index + 1) % OPTIONS.size
        @lock = 10
        redraw
      elsif Input.key_down?(:e) || Input.key_down?(:return) || Input.key_down?(:enter)
        case @index
        when 0
          @mode = :items
          redraw
        when 1
          @mode = :status
          redraw
        when 2
          ok = (Game.save(1) rescue false)
          UI.show_message(ok ? "Spiel gespeichert (Slot 1)." : "Speichern fehlgeschlagen.")
        when 3
          close
        end
        @lock = 12
      end
    elsif @mode == :items
      if Input.key_down?(:e) || Input.key_down?(:return)
        # Potion id=1
        UI.show_message("Item benutzt (Potion) – HP via Event/Recover.")
        # optional: Game script hook
        begin
          # Recover actor 1 via event-like call
          UI.show_screen_text("Potion!", 0.5, 0.2, 0.3, 1.0, 0.4, 2.0)
        rescue
        end
        @lock = 15
      end
    end
  end
end
