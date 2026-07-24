# PAKET 32: RUI Script-Windows — Demo (eigenes UI-Framework aus Ruby)
# Werden nur geladen, wenn die Engine mit mruby gebaut ist UND Rui
# gebunden ist (definiert sonst nichts, laeuft also ueberall mit).
#
# Belegt: Status-Fenster (Gold + zwei animierte Gauges) + modal-listiges
# Beispielmenue auf Taste I mit Maus- UND Tastatursteuerung.
if defined?(Rui)
  module RuiDemo
    GAUGE_COLS = [[1.0, 0.85, 0.45]].freeze # Akzent

    def start
      super
      @rui_t = 0.0
      # Linkes Status-Fenster (sofort offen, retained)
      win = Rui.window("rui.demo", 12.0, 12.0, 250.0, 104.0)
      @rui_gold = win.add_label("gold", "Gold: 0", 8.0, 4.0)
      @rui_gold.set_color(*GAUGE_COLS.first, 1.0)
      win.add_label("t", "Ladung", 8.0, 26.0)
      @rui_charge = win.add_gauge("charge", 8.0, 44.0, 190.0, 10.0, 0, 100, "hp")
      win.add_label("m", "Mana", 8.0, 58.0)
      @rui_mana = win.add_gauge("mana", 8.0, 76.0, 190.0, 10.0, 0, 100, "mp")
      win.add_label("hint", "I = RUI-Menue (Maus/Tasten)", 8.0, 88.0)
      @rui_menu = nil
      @rui_list = nil
    end

    def demo_menu_open
      return if Rui.find("rui.demo_menu")
      win = Rui.window("rui.demo_menu", 170.0, 150.0, 260.0, 132.0)
      win.openness = 0.0
      win.open # XP-Aufroll-Animation
      win.add_label("cap", "RUI-Menue (Esc = zurueck)", 8.0, 2.0)
      @rui_list = win.add_list("m", [
        "Hallo sagen",
        ["Gold +10", true],
        ["Gesperrter Eintrag", false],
        "Menue schliessen"
      ], 8.0, 24.0, 220.0, 96.0)
      @rui_list.on_pick do |idx|
        case idx
        when 0
          UI.show_message("Hallo aus einem RUI-Script-Fenster! (PAKET 32)")
          demo_menu_close
        when 1
          UI.add_gold(10)
          demo_menu_close
        when 3
          demo_menu_close
        end
      end
      @rui_list.on_cancel { demo_menu_close }
      Rui.set_focus_list("rui.demo_menu", "m")
      @rui_menu = win
    end

    def demo_menu_close
      if (m = Rui.find("rui.demo_menu"))
        m.close # Aufroll nach unten; Fenster entfernt sich selbst
      end
      Rui.clear_focus
      @rui_menu = nil
      @rui_list = nil
    end

    def update
      super
      return unless Rui.find("rui.demo")
      @rui_t += 0.016
      # Gauges animieren (live-Zuweisung aus dem Skript)
      @rui_charge.current = (50.0 + 45.0 * Math.sin(@rui_t)).round
      @rui_mana.current = (50.0 + 45.0 * Math.sin(@rui_t * 0.63 + 1.2)).round
      # Gold-Anzeige von Zeit zu Zeit aktualisieren
      @rui_t2 = (@rui_t2 || 0) + 1
      if @rui_t2 >= 60
        @rui_t2 = 0
        begin
          g = UI.gold
        rescue
          g = 0
        end
        @rui_gold.text = "Gold: #{g}"
      end
      if Input.key_pressed?(:i)
        if Rui.find("rui.demo_menu")
          demo_menu_close
        else
          demo_menu_open
        end
      end
    end

    def terminate
      Rui.destroy("rui.demo") if Rui.find("rui.demo")
      if Rui.find("rui.demo_menu")
        Rui.destroy("rui.demo_menu")
        Rui.clear_focus
      end
      super
    end
  end

  Scene_Map.prepend(RuiDemo) if defined?(Scene_Map)
  Engine.log("RuiDemo aktiv (I = RUI-Menue)") rescue nil
end
