# RPG Maker 3D - Config
# Wird zuerst geladen (Load-Order: Dateiname).
# Skriptpaket-Stand: PAKET 45 (2026-07-29)
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
    # PAKET 42: Standard-Dialoge (Text 101, Auswahl 102, Zahl 103,
    # Name 303) durch das Ruby-System 18_System_Message.rb ersetzen?
    # false = eingebaute Fenster (Standard), true = Ruby-System aktiv.
    # (Das Skript setzt dann UI.native_message = false; alternativ geht
    # das auch projektweit ueber Game.ini: NativeMessage=0)
    SCRIPT_MESSAGE_SYSTEM = false
    # PAKET 45: Skript-Optionsmenue (20_System_Options.rb) mit F6 aktivieren?
    # false = aus (Standard), true = F6 oeffnet Lautstaerke/Vollbild (nutzt
    # die PAKET-43-Bindings Audio.*_volume + Graphics.fullscreen=).
    SYSTEM_OPTIONS = false
  end
end
