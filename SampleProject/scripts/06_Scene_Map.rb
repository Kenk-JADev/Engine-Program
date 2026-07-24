# Scene_Map - Hauptspiel (Map + HUD). Logik laeuft in Ruby; C++ macht 3D/Player.
class Scene_Map < Scene_Base
  def start
    super
    UI.clear_texts
    # Dauerhafte HUD-Zeilen (duration 0 = infinite) im GameUI-Overlay
    # (RgssUI-Canvas fuer Ruby-Fenster; RmlUi ist seit PAKET 10 entfernt)
    refresh_hud
    UI.show_screen_text("WASD laufen | Shift sprinten | E reden/oeffnen | Esc Menue | H Hilfe", 0.5, 0.94, 0.55, 0.75, 1.0, 0.0)
    Engine.log("Scene_Map aktiv – Scripts steuern HUD/Dialoge")
  end

  def refresh_hud
    begin
      g = UI.gold
    rescue
      g = 0
    end
    UI.show_screen_text("Gold: #{g}", 0.88, 0.05, 1.0, 0.9, 0.25, 0.0)
  end

  def update
    # Hilfe
    if Input.key_down?(:h)
      UI.show_screen_text("Esc Menue (Speichern/Laden) | F1/F2 Schnell-Slot 1 | F3 Testkampf | E reden", 0.5, 0.12, 0.4, 0.9, 1.0, 3.0)
    end
    # Leichte HUD-Aktualisierung (nicht jedes Frame clearen – teuer)
    @hud_t = (@hud_t || 0) + 1
    if @hud_t % 60 == 0
      refresh_hud
    end
  end

  def terminate
    UI.clear_texts
    super
  end
end
