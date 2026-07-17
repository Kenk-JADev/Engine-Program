# Hotkeys: F1 Save | F2 Load | F3 Testkampf (Ruby steuert das Spiel)
module GameMenu
  @lock = 0
  def self.update
    @lock -= 1 if @lock && @lock > 0
    return if @lock && @lock > 0
    if Input.key_down?(:f1)
      ok = (Game.save(1) rescue false)
      UI.show_message(ok ? "Gespeichert (Slot 1)." : "Speichern fehlgeschlagen.")
      @lock = 30
    elsif Input.key_down?(:f2)
      ok = (Game.load(1) rescue false)
      UI.show_message(ok ? "Geladen (Slot 1)." : "Kein Save in Slot 1.")
      @lock = 30
    elsif Input.key_down?(:f3)
      Game.start_battle(1) rescue nil
      SceneManager.goto(Scene_Battle) if Object.const_defined?(:Scene_Battle)
      @lock = 30
    end
  end
end
