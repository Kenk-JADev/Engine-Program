# Game_Temp
class Game_Temp
  attr_accessor :common_event_id, :fade_type
  def initialize
    @common_event_id = 0
    @fade_type = 0
  end
end
$game_temp = Game_Temp.new
