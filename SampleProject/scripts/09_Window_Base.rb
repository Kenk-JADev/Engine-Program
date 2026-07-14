# Window_Base
class Window_Base
  attr_accessor :x, :y, :width, :height, :visible, :text
  def initialize(x, y, w, h)
    @x = x
    @y = y
    @width = w
    @height = h
    @visible = true
    @text = ""
  end
  def show_text(t, x=0.5, y=0.1, duration=3.0)
    UI.show_screen_text(t, x, y, 1.0, 1.0, 1.0, duration)
  end
end
