# Picture Move mit Tween Animation - Vorinstalliertes Default Script
# Zeigt wie Screen Sprites smooth animiert werden können

module PictureTween
  # Easing Konstanten wie im C++ System
  LINEAR = 0
  EASE_IN_QUAD = 1
  EASE_OUT_QUAD = 2
  EASE_IN_OUT_QUAD = 3
  BOUNCE = 4
  SINE = 5
  ELASTIC = 6

  def self.move(id_or_name, x, y, duration=0.8, easing=EASE_OUT_QUAD)
    UI.move_picture(id_or_name, x, y, duration, easing) rescue nil
  end

  def self.tween(id, x, y, scale=1.0, opacity=1.0, rotation=0.0, duration=1.0, easing=EASE_OUT_QUAD)
    UI.tween_picture(id, x, y, scale, opacity, rotation, duration, easing) rescue UI.move_picture(id, x, y, duration, easing)
  end

  def self.show_popup(picture, text=nil)
    id = UI.show_picture(picture, picture, 0.5, 1.2, 0.0, 0.0)
    move(id, 0.5, 0.5, 0.6, BOUNCE)
    UI.show_screen_text(text, 0.5, 0.6, 1.0, 1.0, 1.0, 2.0) if text
    id
  end

  def self.hide_with_fade(id_or_name, duration=0.5)
    # Fade out + shrink
    UI.tween_picture(id_or_name, 0.5, 0.5, 0.0, 0.0, 0.0, duration, EASE_IN_QUAD) rescue UI.remove_picture(id_or_name)
  end
end

# Beispiel Custom System das Tween nutzt
class PictureMenuSystem
  def initialize
    @open = false
  end

  def open_menu
    return if @open
    @open = true
    # Menü von oben rein sliden
    id = UI.show_picture("icon.png", "main_menu", 0.5, -0.5, 1.0, 0.0)
    PictureTween.move(id, 0.5, 0.5, 0.6, PictureTween::EASE_OUT_QUAD)
  end

  def close_menu
    return unless @open
    @open = false
    PictureTween.move("main_menu", 0.5, -0.5, 0.5, PictureTween::EASE_IN_QUAD)
  end
end

$picture_menu = PictureMenuSystem.new
