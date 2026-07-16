# Dialog-Komfort (Ruby)
# Message.say("Name", "Text", :bottom|:mid|:top)
# C++ speichert Speaker/Position wenn GameUI.ShowMessage(text, speaker, pos) genutzt wird.
# Aktuell: Name wird in den Text praefixiert (kompatibel ohne neue Ruby-Bindings).
module Message
  def self.say(name, text, pos = :bottom)
    prefix = case pos
             when :top then "[TOP] "
             when :mid, :middle then "[MID] "
             else ""
             end
    body = text.to_s
    body = "#{name}: #{body}" if name && !name.to_s.empty?
    UI.show_message(prefix + body)
  end

  def self.choices(prompt, *options)
    UI.show_message(([prompt] + options).join("|"))
  end
end
