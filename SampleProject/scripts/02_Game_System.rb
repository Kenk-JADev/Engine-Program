# Game_System
class Game_System
  attr_accessor :playtime, :save_count, :bgm, :bgs
  def initialize
    @playtime = 0
    @save_count = 0
  end
  def update(delta)
    @playtime += delta
  end
end
$game_system = Game_System.new
