# RPG Maker 3D – Screen Text Beispiele
# Zeigt wie man Texte auf dem Bildschirm anzeigt (HUD, Floating Damage, etc.)
# Diese Funktionen sind sowohl per Default als auch per Custom Code im Script Editor nutzbar

# === Beispiel 1: HUD Texte ===
# Zeigt Gold, HP, Position etc. als Screen Texte
class HUDSystem
  def initialize
    @gold_text_id = nil
    @pos_text_id = nil
  end

  def update
    # Gold Anzeige oben rechts (0.85, 0.05 = 85% von links, 5% von oben)
    # Lösche alten Text und zeige neuen an (für dynamische Werte)
    # In richtigem Spiel würde man hier nur updaten wenn sich Wert ändert
    
    # Beispiel: Gold wird live angezeigt
    # UI.clear_texts löscht alle, besser wäre nur IDs tracken
  end

  def show_gold_popup(amount)
    # Gold Gain als Floating Text über Spieler
    UI.show_world_text("Gold +#{amount}", 0, 1, 0, 1.0, 0.9, 0.2, 2.0)
    Engine.log("Gold Popup: +#{amount}")
  end

  def show_damage(target_x, target_y, target_z, damage)
    # Schaden als roter Floating Text
    UI.show_world_text("-#{damage}", target_x, target_y + 1.0, target_z, 1.0, 0.2, 0.2, 1.5)
  end
end

# === Beispiel 2: Quest Texte ===
class QuestNotifier
  def self.show_quest(text)
    # Quest Text mittig oben, 4 Sekunden
    UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.2, 4.0)
  end

  def self.show_tutorial(text)
    # Tutorial unten mittig, mit Hintergrund, länger sichtbar
    UI.show_screen_text("[TUTORIAL] #{text}", 0.5, 0.85, 0.4, 1.0, 0.4, 6.0)
  end
end

# === Beispiel 3: Input Feedback ===
# Zeigt Texte bei Tastendruck
class InputFeedback
  def update
    if Input.key_down?(:e)
      UI.show_screen_text("Interagieren!", 0.5, 0.5, 0.2, 1.0, 0.5, 1.0)
    end

    if Input.key_down?(:q)
      UI.show_screen_text("Q gedrückt", 0.1, 0.5, 0.8, 0.8, 1.0, 1.0)
    end
  end
end

# === Beispiel 4: Map-spezifische Texte ===
# Wenn Spieler in bestimmte Zone läuft, zeige Text
class ZoneTrigger
  def initialize
    @triggered = {}
  end

  def check_zone(player_pos, zone_name, x_min, x_max, z_min, z_max)
    key = zone_name
    return if @triggered[key]

    px, py, pz = player_pos[0], player_pos[1], player_pos[2] if player_pos.is_a?(Array)
    # Vereinfacht: wenn Spieler in Zone
    if x_min <= px && px <= x_max && z_min <= pz && pz <= z_max
      QuestNotifier.show_quest("Du hast #{zone_name} betreten!")
      @triggered[key] = true
    end
  end
end

# === Default Instanzen für schnellen Zugriff ===
$hud = HUDSystem.new
$quest = QuestNotifier
$input_feedback = InputFeedback.new
$zones = ZoneTrigger.new

# === Beispiel Nutzung im Game Update ===
# Diese update Methode wird vom Engine jede Frame aufgerufen wenn mruby aktiv
class Game
  def update(delta_time)
    # Screen Texte können hier permanent gemanaged werden
    $input_feedback.update if $input_feedback

    # Beispiel: Zeige Position alle 5 Sekunden (vereinfacht)
    # In richtigem Code würde man Timer nutzen
  end
end

Engine.log("Screen Text System geladen - nutze UI.show_screen_text / UI.show_world_text")
