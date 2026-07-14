# UI HUD
module UI_HUD
  def self.show_gold_popup(amount)
    UI.show_world_text("Gold +#{amount}", 0, 1.5, 0, 1.0, 0.9, 0.2, 2.5)
  end
  def self.show_damage(x, y, z, damage, critical=false)
    r = critical ? 1.0 : 1.0
    g = critical ? 0.8 : 0.2
    b = critical ? 0.0 : 0.2
    UI.show_world_text(critical ? "CRITICAL #{damage}!" : "#{damage}", x, y + 1.0, z, r, g, b, 1.5)
  end
  def self.show_quest(text)
    UI.show_screen_text(text, 0.5, 0.15, 1.0, 0.9, 0.3, 4.0)
  end
  def self.show_tutorial(text)
    UI.show_screen_text("[T] #{text}", 0.5, 0.85, 0.4, 0.8, 1.0, 5.0)
  end
  def self.clear
    UI.clear_texts
    UI.clear_pictures
  end
end
