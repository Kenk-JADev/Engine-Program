# Picture Move mit Tween Animation - Beispiele
# Zeigt wie Screen Sprites smooth animiert bewegt werden können

# === Easing Typen ===
# 0 = linear
# 1 = easeInQuad (langsam start, schnell ende)
# 2 = easeOutQuad (schnell start, langsam ende) - DEFAULT, am natürlichsten
# 3 = easeInOutQuad (langsam start + ende)
# 4 = easeOutBounce (bounce effekt)
# 5 = easeInOutSine (sehr smooth)
# 6 = easeOutElastic (überzieht leicht, elastisch)

# === Beispiel 1: Einfaches Move mit Tween ===
class PictureTweenExample
  def initialize
    @pics = []
  end

  def show_entrance
    # Bild erscheint von außerhalb mit Bounce
    id = UI.show_picture("icon.png", "entrance", -0.2, 0.5, 0.0, 0.0) # start außerhalb links, unsichtbar klein
    UI.show_world_text("Entrance Animation!", 0, 2, 0, 1.0, 0.8, 0.2, 2.0)
    # Tween to center with bounce: pos 0.5,0.5 scale 1.0 opacity 1.0 in 1.0 sec mit Bounce easing (4)
    UI.tween_picture(id, 0.5, 0.5, 1.0, 1.0, 0.0, 1.0, 4) rescue UI.move_picture(id, 0.5, 0.5, 1.0, 4)
  end

  def slide_menu
    # Simuliere Menü das von oben rein slided
    id = UI.show_picture("icon.png", "menu", 0.5, -0.3, 0.8, 0.0)
    # Move to 0.5,0.2 in 0.6 sec mit easeOutQuad (2) - smooth stop
    UI.move_picture(id, 0.5, 0.2, 0.6, 2)
  end

  def fade_out_and_move
    # Bild ausblenden und dabei nach rechts bewegen
    id = UI.show_picture("icon.png", "fade", 0.5, 0.5, 1.0, 1.0)
    # Nach 1 sec: bewege nach rechts und fade out gleichzeitig
    # UI.tween_picture(id, x, y, scale, opacity, rotation, duration, easing)
    UI.tween_picture(id, 0.9, 0.5, 0.5, 0.0, 0.0, 1.2, 1) # nach rechts, kleiner, unsichtbar
  end

  def achievement_popup
    # Achievement unten rein bouncen, dann wieder raus
    id = UI.show_picture("icon.png", "ach", 0.5, 1.2, 0.0, 0.0)
    UI.move_picture(id, 0.5, 0.8, 0.5, 4) # Bounce rein
    # Nach 3 sec wieder raus - in echtem Code via Timer
  end

  def gold_collected_animation
    # Gold Icon: erst groß, dann klein + nach oben zum Gold Counter
    id = UI.show_picture("icon.png", "gold_anim", 0.5, 0.5, 2.0, 1.0)
    UI.tween_picture(id, 0.85, 0.05, 0.3, 0.0, 0.0, 0.8, 2)
  end
end

$pic_tween = PictureTweenExample.new

# === Beispiel Nutzung in Events ===

# Event: Wenn Spieler Truhe öffnet
def on_chest_opened
  UI.show_world_text("Truhe geöffnet!", 0, 1.5, 0, 1.0, 0.9, 0.2, 2.0)
  # Gold Icon Animation
  id = UI.show_picture("icon.png", "gold_icon", 0.5, 0.5, 0.0, 0.0)
  UI.tween_picture(id, 0.5, 0.5, 1.2, 1.0, 0.0, 0.3, 4) # pop in mit bounce
  # Kurz warten, dann zum Gold Counter moven
  # (In Event mit Wait Befehl)
  # MovePicture: id, x, y, duration, easing
  # Easing 2 = easeOutQuad ist am besten für UI
end

# === Beispiel als Custom System im Script Editor ===
# Vorinstalliertes Default Script kann so aussehen:
class PictureSystem
  def show_quest_banner(text)
    # Banner von oben rein
    UI.show_screen_text(text, 0.5, -0.1, 1.0, 0.9, 0.3, 0.0) # infinite, wir steuern per tween
    id = UI.show_picture("banner.png", "quest_banner", 0.5, -0.2, 1.0, 0.0)
    UI.move_picture(id, 0.5, 0.15, 0.6, 2) # easeOut
    UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.3, 4.0)
  end

  def hide_quest_banner
    UI.move_picture("quest_banner", 0.5, -0.2, 0.5, 1) # easeIn raus
  end
end

$pic_system = PictureSystem.new

Engine.log("Picture Tween System geladen - nutze UI.move_picture(id, x, y, duration, easing)")

# === Liste Easing für Nutzer ===
# UI.move_picture(id, 0.8, 0.5, 1.0, 2)
# id = picture id oder name, x=0..1, y=0..1, duration=sekunden, easing=0..6
# Easing: 0 linear, 1 easeInQuad, 2 easeOutQuad (default), 3 easeInOutQuad, 4 bounce, 5 sine, 6 elastic
