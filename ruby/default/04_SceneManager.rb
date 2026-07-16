# SceneManager - Scene-Stack (wird von C++ RubyVM::Update pro Frame getrieben)
module SceneManager
  @stack = []
  @next_scene = nil
  @current_scene = nil
  @started = false

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

  def self.started?
    @started
  end

  # Einmalig beim ersten Playtest / Script-Load
  def self.run(initial_scene)
    return if @started
    @started = true
    @current_scene = initial_scene.new
    @current_scene.start
    Engine.log("SceneManager.run #{initial_scene}")
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
end
