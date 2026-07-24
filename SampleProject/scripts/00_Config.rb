# RPG Maker 3D - Config
# Wird zuerst geladen (Load-Order: Dateiname).
# Skriptpaket-Stand: PAKET 26 (2026-07-24)
module RPGMaker3D
  VERSION = "0.2.0" # folgt EngineConfig::VERSION (include/rpgmaker3d/Config.h)
  ENGINE = "RPG Maker 3D"
  module Config
    SCREEN_WIDTH = 1280
    SCREEN_HEIGHT = 720
    START_GOLD = 500
    START_MAP_ID = 1
    # true = Playtest startet direkt auf der Map (C++ startet Game bereits)
    # false = Title-Scene zuerst (Enter = New Game)
    SKIP_TITLE_IN_PLAYTEST = true
    # true  = natives XP-Menue der Engine (Esc), Ruby-PartyMenue aus
    # false = Ruby-PartyMenue (15_Party_Menu.rb) stattdessen
    USE_NATIVE_MENU = true
  end
end
