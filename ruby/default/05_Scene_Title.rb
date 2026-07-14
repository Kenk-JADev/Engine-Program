# Scene_Title
class Scene_Title < Scene_Base
  def start
    super
    UI.show_screen_text("RPG Maker 3D", 0.5, 0.3, 1.0, 0.8, 0.2, 0.0)
    UI.show_screen_text("Press Enter - New Game", 0.5, 0.5, 1.0, 1.0, 1.0, 0.0)
  end
  def update
    if Input.key_down?(:return)
      UI.clear_texts
      SceneManager.goto(Scene_Map)
    end
  end
  def terminate
    UI.clear_texts
    super
  end
end
