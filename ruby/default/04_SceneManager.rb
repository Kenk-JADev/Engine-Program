# SceneManager
module SceneManager
  @stack = []
  @next_scene = nil
  def self.goto(scene_class)
    @next_scene = scene_class
  end
  def self.push(scene_class)
    @stack.push(@current_scene) if @current_scene
    @next_scene = scene_class
  end
  def self.pop
    @next_scene = @stack.pop
  end
  def self.current_scene
    @current_scene
  end
  def self.update
    if @next_scene
      @current_scene.terminate if @current_scene
      @current_scene = @next_scene.new
      @current_scene.start
      @next_scene = nil
    end
    @current_scene.update if @current_scene
  end
  def self.run(initial_scene)
    @current_scene = initial_scene.new
    @current_scene.start
  end
end
