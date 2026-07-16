# Scene_Battle - Ruby-Seite des Kampfs (Anzeige). C++ BattleSystem rechnet.
class Scene_Battle < Scene_Base
  def start
    super
    UI.clear_texts
    UI.show_screen_text("=== KAMPF ===", 0.5, 0.12, 1.0, 0.35, 0.35, 0.0)
    UI.show_screen_text("C++ BattleSystem laeuft – Sieg/Niederlage als Message", 0.5, 0.22, 0.9, 0.9, 0.9, 4.0)
    Engine.log("Scene_Battle start")
  end

  def update
    # Zurueck zur Map wenn Kampf vorbei
    if defined?(Game) && Game.respond_to?(:in_battle?)
      unless Game.in_battle?
        SceneManager.goto(Scene_Map) if defined?(Scene_Map)
      end
    end
  end

  def terminate
    UI.clear_texts
    super
  end
end
