# Sprite / Picture System - Screen Sprites wie im Original
class Game_Picture
  attr_accessor :name, :x, :y, :scale, :opacity, :visible
  def initialize(id)
    @id = id
    @name = ""
    @x = 0.5
    @y = 0.5
    @scale = 1.0
    @opacity = 1.0
    @visible = false
  end
  def show(filename, x=0.5, y=0.5, scale=1.0, opacity=0.9)
    @name = filename
    @x = x
    @y = y
    @scale = scale
    @opacity = opacity
    @visible = true
    UI.show_picture(filename, filename, x, y, scale, opacity)
  end
  def move(x, y, duration=1.0)
    @x = x
    @y = y
    UI.move_picture(@name, x, y)
  end
  def erase
    UI.remove_picture(@name)
    @visible = false
  end
end

class Game_Pictures
  def initialize
    @pictures = {}
  end
  def [](id)
    @pictures[id] ||= Game_Picture.new(id)
  end
  def clear
    UI.clear_pictures
    @pictures.clear
  end
end

$game_pictures = Game_Pictures.new

class ScreenSprite
  def self.show(filename, x=0.5, y=0.5, scale=1.0)
    $game_pictures[filename].show(filename, x, y, scale)
  end
  def self.hide(filename)
    $game_pictures[filename].erase
  end
end
