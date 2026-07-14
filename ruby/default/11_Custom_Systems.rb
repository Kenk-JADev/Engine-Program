# Custom Systeme
class QuestSystem
  def initialize
    @quests = {}
  end
  def add_quest(id, name, desc)
    @quests[id] = {name: name, desc: desc, completed: false}
    UI_HUD.show_quest("Neue Quest: #{name}")
  end
  def complete_quest(id)
    if @quests[id]
      @quests[id][:completed] = true
      UI_HUD.show_quest("Quest abgeschlossen: #{@quests[id][:name]}")
      UI.show_world_text("Quest Complete!", 0, 2, 0, 0.2, 1.0, 0.2, 3.0)
    end
  end
end

$quest_system = QuestSystem.new

class AchievementSystem
  def initialize
    @achievements = {}
  end
  def unlock(name)
    return if @achievements[name]
    @achievements[name] = true
    UI.show_screen_text("Achievement: #{name}!", 0.5, 0.2, 1.0, 0.8, 0.2, 4.0)
  end
end

$achievements = AchievementSystem.new
