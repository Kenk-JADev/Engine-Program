# RPG Maker 3D – Hauptspielskript
# Hier kann der Nutzer seine eigene Logik definieren.

class Game
  def initialize
    @player = Actor.new("Hero")
    @player.move_to(0, 0, 0)
  end

  def update(delta_time)
    # Beispiel: Bewegung über Tastatur
    @player.move(0, 0, delta_time) if Input.key_down?(:w)
    @player.move(0, 0, -delta_time) if Input.key_down?(:s)
  end
end

$game = Game.new
