# Scene_Base
class Scene_Base
  def initialize
    @running = false
  end
  def start
    Engine.log("Scene #{self.class.name} start")
  end
  def update
  end
  def terminate
    Engine.log("Scene #{self.class.name} terminate")
  end
  def goto_scene(scene_class)
    SceneManager.goto(scene_class)
  end
end
