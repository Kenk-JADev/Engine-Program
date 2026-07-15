# RPG Maker 3D – Hauptspielskript
# Hier kann der Nutzer seine eigene Logik definieren.
# Neu: ScreenText System für HUD, Floating Texte, Quests

class Game
  def initialize
    @player = Actor.new("Hero")
    @player.move_to(0, 0, 0)
    @time = 0.0
    Engine.log("Game gestartet - ScreenText System bereit!")

    # Willkommens-Text
    UI.show_screen_text("Willkommen in RPG Maker 3D!", 0.5, 0.2, 1.0, 1.0, 0.5, 4.0)
  end

  def update(delta_time)
    @time += delta_time

    # Beispiel: Bewegung über Tastatur (Legacy)
    @player.move(0, 0, delta_time) if Input.key_down?(:w)
    @player.move(0, 0, -delta_time) if Input.key_down?(:s)

    # === ScreenText Beispiele (Custom Codes) ===
    # Gold Anzeige alle 3 Sekunden als Floating Text über Spieler
    if (@time % 5.0) < delta_time
      # UI.show_world_text("Gold: #{UI.gold}", 0, 1, 0, 1.0, 0.9, 0.2, 2.0)
    end

    # Tutorial Text bei bestimmten Tasten
    if Input.key_down?(:h)
      UI.show_screen_text("Hilfe: WASD bewegen, E sprechen, Q/R/T Gizmo, F Fokus", 0.5, 0.85, 0.3, 0.8, 1.0, 3.0)
    end
  end

  # Beispiel Methode für Events: wird von EventSystem aufgerufen
  def on_gold_gained(amount)
    UI.show_world_text("Gold +#{amount}!", 0, 1.5, 0, 1.0, 0.85, 0.2, 2.5)
    UI.show_screen_text("Du hast #{amount} Gold erhalten!", 0.5, 0.1, 1.0, 0.9, 0.3, 3.0)
  end

  def on_damage_dealt(target, damage)
    x, y, z = target.position
    UI.show_world_text("-#{damage} HP", x, y + 1.0, z, 1.0, 0.2, 0.2, 1.5)
  end
end

$game = Game.new

# Globale Hilfsfunktionen für Events und Custom Codes
def show_tutorial(text)
  UI.show_screen_text("[TUTORIAL] #{text}", 0.5, 0.85, 0.4, 1.0, 0.6, 5.0)
end

def show_quest(text)
  UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.2, 4.0)
end

def show_floating_text(text, x, y, z, r=1.0, g=1.0, b=0.0)
  UI.show_world_text(text, x, y, z, r, g, b, 2.5)
end

