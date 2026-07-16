# RPG Maker 3D - Config
# Wird zuerst geladen (Load-Order: Dateiname).
module RPGMaker3D
  VERSION = "0.3.0"
  ENGINE = "RPG Maker 3D"
  module Config
    SCREEN_WIDTH = 1280
    SCREEN_HEIGHT = 720
    START_GOLD = 500
    START_MAP_ID = 1
    # true = Playtest startet direkt auf der Map (C++ startet Game bereits)
    # false = Title-Scene zuerst (Enter = New Game)
    SKIP_TITLE_IN_PLAYTEST = true
  end
end
