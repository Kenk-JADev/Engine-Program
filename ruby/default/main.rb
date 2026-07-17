# RPG Maker 3D – Hauptspielskript (zuletzt geladen)
# WICHTIG: Spiellogik laeuft in Ruby. C++ liefert Engine/Rendering/Input.
# UI.* schreibt in GameUI; RmlUi zeigt es im Game-Fenster an (kein Ruby-in-RmlUi).

class Game
  def initialize
    @time = 0.0
    Engine.log("Game ($game) bereit – SceneManager steuert Scenes")
  end

  def update(delta_time)
    @time += delta_time
    # Party-Menue (Esc) hat Vorrang
    if defined?(PartyMenu)
      PartyMenu.update
      return if PartyMenu.open?
    end
    # F1/F2/F3 – siehe 14_Menu_Save.rb
    GameMenu.update if defined?(GameMenu)
  end

  def on_gold_gained(amount)
    UI.show_world_text("Gold +#{amount}!", 0, 1.5, 0, 1.0, 0.85, 0.2, 2.5)
    UI.show_screen_text("Du hast #{amount} Gold erhalten!", 0.5, 0.1, 1.0, 0.9, 0.3, 3.0)
  end

  def on_damage_dealt(x, y, z, damage)
    UI.show_world_text("-#{damage} HP", x, y + 1.0, z, 1.0, 0.2, 0.2, 1.5)
  end
end

$game = Game.new

# Hilfsfunktionen fuer Events / Custom Codes
def show_tutorial(text)
  UI.show_screen_text("[TUTORIAL] #{text}", 0.5, 0.85, 0.4, 1.0, 0.6, 5.0)
end

def show_quest(text)
  UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.2, 4.0)
end

def show_floating_text(text, x, y, z, r = 1.0, g = 1.0, b = 0.0)
  UI.show_world_text(text, x, y, z, r, g, b, 2.5)
end

# Scene-Start: Playtest springt auf Map; sonst Title
begin
  if RPGMaker3D::Config::SKIP_TITLE_IN_PLAYTEST
    SceneManager.run(Scene_Map)
  else
    SceneManager.run(Scene_Title)
  end
rescue => e
  Engine.log("Scene start fallback: #{e}")
  SceneManager.run(Scene_Map) if defined?(Scene_Map)
end
