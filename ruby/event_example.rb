# Beispiel für ein Map-Event

class NPCEvent < Event
  def initialize
    super
    @dialog = ["Willkommen in der Welt!", "Viel Spaß beim Erkunden."]
  end

  def on_interact(player)
    UI.show_message(@dialog.sample)
  end
end
