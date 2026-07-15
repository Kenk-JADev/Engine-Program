# Scene_Map
class Scene_Map < Scene_Base
  def start
    super
    UI.show_screen_text("Gold: #{UI.gold}", 0.85, 0.05, 1.0, 0.9, 0.2, 0.0)
    UI.show_screen_text("WASD bewegen, E sprechen", 0.15, 0.95, 0.6, 0.8, 1.0, 0.0)
  end
  def update
  end
  def terminate
    UI.clear_texts
    super
  end
end
