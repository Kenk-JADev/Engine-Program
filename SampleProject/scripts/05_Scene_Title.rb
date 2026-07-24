# Scene_Title - Titelbildschirm (Ruby-Texte via UI -> GameUI-Overlay/
# RgssUI-Canvas; RmlUi ist seit PAKET 10 entfernt).
# Hinweis: Der Player zeigt zusaetzlich den nativen XP-Titel
# (Datenbank -> System -> Titel-Optionen, PAKET 11/14).
class Scene_Title < Scene_Base
  def start
    super
    @done = false
    UI.clear_texts
    UI.show_screen_text("RPG Maker 3D", 0.5, 0.28, 1.0, 0.85, 0.25, 0.0)
    UI.show_screen_text("Enter / E  -  New Game", 0.5, 0.48, 0.95, 0.95, 0.95, 0.0)
    Engine.log("Title: Enter fuer New Game")
  end

  def update
    return if @done
    if Input.key_down?(:return) || Input.key_down?(:enter) || Input.key_down?(:e)
      @done = true
      UI.clear_texts
      SceneManager.goto(Scene_Map)
    end
  end

  def terminate
    UI.clear_texts
    super
  end
end
