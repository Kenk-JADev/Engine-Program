# Scene_Battle - Anzeige + Hinweis auf Tasten (C++ BattleSystem rechnet)
class Scene_Battle < Scene_Base
  def start
    super
    UI.clear_texts
    UI.show_screen_text("=== KAMPF ===", 0.5, 0.08, 1.0, 0.3, 0.3, 0.0)
    UI.show_screen_text("1/A Angriff  2/S Skill  3/I Item  4 Flucht", 0.5, 0.16, 0.95, 0.95, 0.7, 0.0)
    Engine.log("Scene_Battle – warte auf Input 1-4")
  end

  def update
    if Game.respond_to?(:in_battle?) && !Game.in_battle?
      UI.clear_texts
      SceneManager.goto(Scene_Map) if Object.const_defined?(:Scene_Map)
    end
  end

  def terminate
    UI.clear_texts
    super
  end
end
