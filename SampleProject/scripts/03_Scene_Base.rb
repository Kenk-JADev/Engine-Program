# Scene_Base - Basis fuer alle Scenes (wie RPG Maker VX Ace / MV)
class Scene_Base
  def initialize
    @running = false
  end

  def start
    Engine.log("Scene #{self.class.name} start")
  end

  def update
    # pro Frame – Input/UI hier
  end

  def terminate
    Engine.log("Scene #{self.class.name} terminate")
  end

  def goto_scene(scene_class)
    SceneManager.goto(scene_class)
  end
end
