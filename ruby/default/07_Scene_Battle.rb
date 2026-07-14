# Scene_Battle
class Scene_Battle < Scene_Base
  def start
    super
    UI.show_screen_text("Kampf gestartet!", 0.5, 0.3, 1.0, 0.3, 0.3, 3.0)
  end
end
