#pragma once
// ============================================================================
// RGSS-Prelude (wird beim VM-Start nach den nativen Bindings geladen)
// ----------------------------------------------------------------------------
// Reine-Ruby-Teile der RGSS, aufbauend auf den nativen Klassen:
//   * Fehlerklassen RGSSError / Reset, rgss_main / rgss_stop
//   * Font-Klassen-API (default_*, exist?) delegiert auf native Defaults
//   * Vergleichsoperatoren fuer Rect/Color/Tone (== / eql?)
//   * module RPG mit ALLEN Datenklassen des RPG Maker XP (RPGXP Data
//     Structures laut RGSS-Referenz) als reine Datencontainer mit
//     XP-Standardwerten
//   * RPG::Cache (Original-Referenzimplementierung, angepasst: ohne GC.start)
//   * RPG::Sprite  (Effekt-Sprites: whiten/appear/disappear/escape/collapse/
//                   damage/blink/animation - approximiert ueber Sprite-API)
//   * RPG::Weather (Regen/Sturm/Schnee-Partikel ueber native Sprites)
// ============================================================================

namespace rpg {

inline const char* kRgssPrelude = R"RUBY(

# ============================================================================
# Fehlerklassen + Basisfunktionen
# ============================================================================
class RGSSError < StandardError; end
class Reset < Exception; end

def rgss_main
  begin
    yield
  rescue Reset
    retry
  end
end

def rgss_stop(*args)
  # Engine laeuft framebasiert; Stop ist hier ein no-op (Kompatibilitaet).
end

def load_data(filename)
  raise RGSSError, "load_data(\"#{filename}\"): .rxdata/Marshal wird nicht " \
    "unterstuetzt - die Datenbank liegt im Engine-Datenbank-Dialog " \
    "(JSON), Laufzeitzugriff ueber die $game_*-Objekte."
end

def save_data(obj, filename)
  raise RGSSError, "save_data: Marshal wird nicht unterstuetzt " \
    "(Spielstaende: Game.save(slot))."
end

# ============================================================================
# Wertklassen: Vergleiche (nativ sind sie es schon; == hier rubysch)
# ============================================================================
class Rect
  def ==(other)
    other.is_a?(Rect) && x == other.x && y == other.y &&
      width == other.width && height == other.height
  end
  def eql?(other); self == other; end
end

class Color
  def ==(other)
    other.is_a?(Color) && red == other.red && green == other.green &&
      blue == other.blue && alpha == other.alpha
  end
  def eql?(other); self == other; end
end

class Tone
  def ==(other)
    other.is_a?(Tone) && red == other.red && green == other.green &&
      blue == other.blue && gray == other.gray
  end
  def eql?(other); self == other; end
end

# ============================================================================
# Font: Klassen-API (Instanz-Methoden sind nativ)
# ============================================================================
class Font
  class << self
    def default_name; FontDefaults.name; end
    def default_name=(v); FontDefaults.name = v; end
    def default_size; FontDefaults.size; end
    def default_size=(v); FontDefaults.size = v; end
    def default_bold; FontDefaults.bold; end
    def default_bold=(v); FontDefaults.bold = v; end
    def default_italic; FontDefaults.italic; end
    def default_italic=(v); FontDefaults.italic = v; end
    def default_color; FontDefaults.color; end
    def default_color=(v); FontDefaults.color = v; end
    # Der eingebaute Bitmap-Font kann jeden Namen darstellen
    def exist?(name); true; end
  end
end

# ============================================================================
# RPG-Modul: saemtliche RPGXP-Datenklassen (reine Container, XP-Defaults)
# ============================================================================
module RPG
  class AudioFile
    attr_accessor :name, :volume, :pitch
    def initialize(name = "", volume = 100, pitch = 100)
      @name = name; @volume = volume; @pitch = pitch
    end
  end

  class Map
    attr_accessor :tileset_id, :width, :height, :autotile_names,
      :panorama_name, :panorama_hue, :fog_name, :fog_hue, :fog_zoom,
      :fog_opacity, :fog_blend_type, :fog_sx, :fog_sy, :battleback_name,
      :bgm, :bgs, :encounter_list, :encounter_step, :data, :events
    def initialize(width = 20, height = 15)
      @tileset_id = 1
      @width = width; @height = height
      @autotile_names = [""] * 7
      @panorama_name = ""; @panorama_hue = 0
      @fog_name = ""; @fog_hue = 0
      @fog_zoom = 100; @fog_opacity = 64; @fog_blend_type = 0
      @fog_sx = 0; @fog_sy = 0
      @battleback_name = ""
      @bgm = AudioFile.new
      @bgs = AudioFile.new("", 80)
      @encounter_list = []
      @encounter_step = 30
      @data = Table.new(width, height, 3)
      @events = {}
    end
  end

  class MapInfo
    attr_accessor :name, :parent_id, :order, :expanded, :scroll_x, :scroll_y
    def initialize
      @name = ""; @parent_id = 0; @order = 0
      @expanded = false; @scroll_x = 0; @scroll_y = 0
    end
  end

  class Event
    attr_accessor :id, :name, :x, :y, :pages
    def initialize(x = 0, y = 0)
      @id = 0; @name = ""; @x = x; @y = y; @pages = [Page.new]
    end
    class Page
      attr_accessor :condition, :graphic, :move_type, :move_speed,
        :move_frequency, :move_route, :walk_anime, :step_anime,
        :direction_fix, :through, :always_on_top, :trigger, :list
      def initialize
        @condition = Condition.new
        @graphic = Graphic.new
        @move_type = 0; @move_speed = 3; @move_frequency = 3
        @move_route = RPG::MoveRoute.new
        @walk_anime = true; @step_anime = false
        @direction_fix = false; @through = false; @always_on_top = false
        @trigger = 0
        @list = [RPG::EventCommand.new]
      end
      class Condition
        attr_accessor :switch1_valid, :switch2_valid, :variable_valid,
          :self_switch_valid, :switch1_id, :switch2_id, :variable_id,
          :variable_value, :self_switch_ch
        def initialize
          @switch1_valid = false; @switch2_valid = false
          @variable_valid = false; @self_switch_valid = false
          @switch1_id = 1; @switch2_id = 1
          @variable_id = 1; @variable_value = 0
          @self_switch_ch = "A"
        end
      end
      class Graphic
        attr_accessor :tile_id, :character_name, :character_hue,
          :direction, :pattern, :opacity, :blend_type
        def initialize
          @tile_id = 0; @character_name = ""; @character_hue = 0
          @direction = 2; @pattern = 0
          @opacity = 255; @blend_type = 0
        end
      end
    end
  end

  class EventCommand
    attr_accessor :code, :indent, :parameters
    def initialize(code = 0, indent = 0, parameters = [])
      @code = code; @indent = indent; @parameters = parameters
    end
  end

  class MoveRoute
    attr_accessor :repeat, :skippable, :list
    def initialize
      @repeat = true; @skippable = false
      @list = [MoveCommand.new]
    end
  end

  class MoveCommand
    attr_accessor :code, :parameters
    def initialize(code = 0, parameters = [])
      @code = code; @parameters = parameters
    end
  end

  class Actor
    attr_accessor :id, :name, :class_id, :initial_level, :final_level,
      :exp_basis, :exp_inflation, :character_name, :character_hue,
      :battler_name, :battler_hue, :parameters, :weapon_id, :armor1_id,
      :armor2_id, :armor3_id, :armor4_id, :weapon_fix, :armor1_fix,
      :armor2_fix, :armor3_fix, :armor4_fix
    def initialize
      @id = 0; @name = ""; @class_id = 1
      @initial_level = 1; @final_level = 99
      @exp_basis = 30; @exp_inflation = 30
      @character_name = ""; @character_hue = 0
      @battler_name = ""; @battler_hue = 0
      @parameters = Table.new(6, 100)
      for i in 1..99
        @parameters[0, i] = 500 + i * 50   # maxhp
        @parameters[1, i] = 500 + i * 50   # maxsp
        @parameters[2, i] = 50 + i * 5     # str
        @parameters[3, i] = 50 + i * 5     # dex
        @parameters[4, i] = 50 + i * 5     # agi
        @parameters[5, i] = 50 + i * 5     # int
      end
      @weapon_id = 0; @armor1_id = 0; @armor2_id = 0
      @armor3_id = 0; @armor4_id = 0
      @weapon_fix = false; @armor1_fix = false; @armor2_fix = false
      @armor3_fix = false; @armor4_fix = false
    end
  end

  class Class
    attr_accessor :id, :name, :position, :weapon_set, :armor_set,
      :element_ranks, :state_ranks, :learnings
    def initialize
      @id = 0; @name = ""; @position = 0
      @weapon_set = []; @armor_set = []
      @element_ranks = Table.new(1)
      @state_ranks = Table.new(1)
      @learnings = []
    end
    class Learning
      attr_accessor :level, :skill_id
      def initialize
        @level = 1; @skill_id = 1
      end
    end
  end

  class Skill
    attr_accessor :id, :name, :icon_name, :description, :scope, :occasion,
      :animation1_id, :animation2_id, :menu_se, :common_event_id, :sp_cost,
      :power, :atk_f, :eva_f, :str_f, :dex_f, :agi_f, :int_f, :hit,
      :pdef_f, :mdef_f, :variance, :element_set, :plus_state_set,
      :minus_state_set
    def initialize
      @id = 0; @name = ""; @icon_name = ""; @description = ""
      @scope = 0; @occasion = 1
      @animation1_id = 0; @animation2_id = 0
      @menu_se = AudioFile.new("", 80)
      @common_event_id = 0; @sp_cost = 0; @power = 0
      @atk_f = 0; @eva_f = 0; @str_f = 0; @dex_f = 0; @agi_f = 0; @int_f = 100
      @hit = 100; @pdef_f = 0; @mdef_f = 0; @variance = 15
      @element_set = []; @plus_state_set = []; @minus_state_set = []
    end
  end

  class Item
    attr_accessor :id, :name, :icon_name, :description, :scope, :occasion,
      :animation1_id, :animation2_id, :menu_se, :common_event_id, :price,
      :consumable, :parameter_type, :parameter_points, :recover_hp_rate,
      :recover_hp, :recover_sp_rate, :recover_sp, :hit, :pdef_f, :mdef_f,
      :variance, :element_set, :plus_state_set, :minus_state_set
    def initialize
      @id = 0; @name = ""; @icon_name = ""; @description = ""
      @scope = 0; @occasion = 0
      @animation1_id = 0; @animation2_id = 0
      @menu_se = AudioFile.new("", 80)
      @common_event_id = 0; @price = 0; @consumable = true
      @parameter_type = 0; @parameter_points = 0
      @recover_hp_rate = 0; @recover_hp = 0
      @recover_sp_rate = 0; @recover_sp = 0
      @hit = 100; @pdef_f = 0; @mdef_f = 0; @variance = 0
      @element_set = []; @plus_state_set = []; @minus_state_set = []
    end
  end

  class Weapon
    attr_accessor :id, :name, :icon_name, :description, :animation1_id,
      :animation2_id, :price, :atk, :pdef, :mdef, :str_plus, :dex_plus,
      :agi_plus, :int_plus, :element_set, :plus_state_set, :minus_state_set
    def initialize
      @id = 0; @name = ""; @icon_name = ""; @description = ""
      @animation1_id = 0; @animation2_id = 0
      @price = 0; @atk = 0; @pdef = 0; @mdef = 0
      @str_plus = 0; @dex_plus = 0; @agi_plus = 0; @int_plus = 0
      @element_set = []; @plus_state_set = []; @minus_state_set = []
    end
  end

  class Armor
    attr_accessor :id, :name, :icon_name, :description, :kind,
      :auto_state_id, :price, :pdef, :mdef, :eva, :str_plus, :dex_plus,
      :agi_plus, :int_plus, :guard_element_set, :guard_state_set
    def initialize
      @id = 0; @name = ""; @icon_name = ""; @description = ""
      @kind = 0; @auto_state_id = 0; @price = 0
      @pdef = 0; @mdef = 0; @eva = 0
      @str_plus = 0; @dex_plus = 0; @agi_plus = 0; @int_plus = 0
      @guard_element_set = []; @guard_state_set = []
    end
  end

  class Enemy
    attr_accessor :id, :name, :battler_name, :battler_hue, :maxhp, :maxsp,
      :str, :dex, :agi, :int, :atk, :pdef, :mdef, :eva, :animation1_id,
      :animation2_id, :element_ranks, :state_ranks, :actions, :exp, :gold,
      :item_id, :weapon_id, :armor_id, :treasure_prob
    def initialize
      @id = 0; @name = ""; @battler_name = ""; @battler_hue = 0
      @maxhp = 500; @maxsp = 500
      @str = 50; @dex = 50; @agi = 50; @int = 50
      @atk = 100; @pdef = 100; @mdef = 100; @eva = 0
      @animation1_id = 0; @animation2_id = 0
      @element_ranks = Table.new(1)
      @state_ranks = Table.new(1)
      @actions = [Action.new]
      @exp = 0; @gold = 0
      @item_id = 0; @weapon_id = 0; @armor_id = 0
      @treasure_prob = 100
    end
    class Action
      attr_accessor :kind, :basic, :skill_id, :condition_turn_a,
        :condition_turn_b, :condition_hp, :condition_level,
        :condition_switch_id, :rating
      def initialize
        @kind = 0; @basic = 0; @skill_id = 1
        @condition_turn_a = 0; @condition_turn_b = 1
        @condition_hp = 100; @condition_level = 1
        @condition_switch_id = 0; @rating = 5
      end
    end
  end

  class Troop
    attr_accessor :id, :name, :members, :pages
    def initialize
      @id = 0; @name = ""; @members = []; @pages = [Page.new]
    end
    class Member
      attr_accessor :enemy_id, :x, :y, :hidden, :immortal
      def initialize
        @enemy_id = 1; @x = 0; @y = 0
        @hidden = false; @immortal = false
      end
    end
    class Page
      attr_accessor :condition, :span, :list
      def initialize
        @condition = Condition.new; @span = 0
        @list = [RPG::EventCommand.new]
      end
      class Condition
        attr_accessor :turn_valid, :enemy_valid, :actor_valid,
          :switch_valid, :turn_a, :turn_b, :enemy_index, :enemy_hp,
          :actor_id, :actor_hp, :switch_id
        def initialize
          @turn_valid = false; @enemy_valid = false
          @actor_valid = false; @switch_valid = false
          @turn_a = 0; @turn_b = 0
          @enemy_index = 0; @enemy_hp = 50
          @actor_id = 1; @actor_hp = 50; @switch_id = 1
        end
      end
    end
  end

  class State
    attr_accessor :id, :name, :animation_id, :restriction, :nonresistance,
      :zero_hp, :cant_get_exp, :cant_evade, :slip_damage, :rating,
      :hit_rate, :maxhp_rate, :maxsp_rate, :str_rate, :dex_rate,
      :agi_rate, :int_rate, :atk_rate, :pdef_rate, :mdef_rate, :eva,
      :battle_only, :hold_turn, :auto_release_prob, :shock_release_prob,
      :guard_element_set, :plus_state_set, :minus_state_set
    def initialize
      @id = 0; @name = ""; @animation_id = 0; @restriction = 0
      @nonresistance = false; @zero_hp = false
      @cant_get_exp = false; @cant_evade = false; @slip_damage = false
      @rating = 5; @hit_rate = 100
      @maxhp_rate = 100; @maxsp_rate = 100
      @str_rate = 100; @dex_rate = 100; @agi_rate = 100; @int_rate = 100
      @atk_rate = 100; @pdef_rate = 100; @mdef_rate = 100; @eva = 0
      @battle_only = true; @hold_turn = 0
      @auto_release_prob = 0; @shock_release_prob = 0
      @guard_element_set = []; @plus_state_set = []; @minus_state_set = []
    end
  end

  class Animation
    attr_accessor :id, :name, :animation_name, :animation_hue, :frame_max,
      :frames, :timings, :position
    def initialize
      @id = 0; @name = ""; @animation_name = ""; @animation_hue = 0
      @frame_max = 16
      @frames = [RPG::Animation::Frame.new] * 16
      @timings = []; @position = 2
    end
    class Frame
      attr_accessor :cell_max, :cell_data
      def initialize
        @cell_max = 0; @cell_data = Table.new(0, 0)
      end
    end
    class Timing
      attr_accessor :frame, :se, :flash_scope, :flash_color,
        :flash_duration, :condition
      def initialize
        @frame = 0; @se = AudioFile.new("", 80)
        @flash_scope = 0; @flash_color = Color.new(255, 255, 255, 255)
        @flash_duration = 5; @condition = 0
      end
    end
  end

  class Tileset
    attr_accessor :id, :name, :tileset_name, :autotile_names,
      :panorama_name, :panorama_hue, :fog_name, :fog_hue, :fog_zoom,
      :fog_opacity, :fog_blend_type, :fog_sx, :fog_sy, :battleback_name,
      :passages, :priorities, :terrain_tags
    def initialize
      @id = 0; @name = ""; @tileset_name = ""
      @autotile_names = [""] * 7
      @panorama_name = ""; @panorama_hue = 0
      @fog_name = ""; @fog_hue = 0
      @fog_zoom = 100; @fog_opacity = 64; @fog_blend_type = 0
      @fog_sx = 0; @fog_sy = 0; @battleback_name = ""
      @passages = Table.new(384)
      @priorities = Table.new(384)
      @terrain_tags = Table.new(384)
    end
  end

  class CommonEvent
    attr_accessor :id, :name, :trigger, :switch_id, :list
    def initialize
      @id = 0; @name = ""; @trigger = 0; @switch_id = 1
      @list = [RPG::EventCommand.new]
    end
  end

  class System
    attr_accessor :magic_number, :party_members, :elements, :switches,
      :variables, :windowskin_name, :title_name, :gameover_name,
      :battle_transition, :title_bgm, :battle_bgm, :battle_end_me,
      :gameover_me, :cursor_se, :decision_se, :cancel_se, :buzzer_se,
      :equip_se, :shop_se, :save_se, :load_se, :battle_start_se,
      :escape_se, :actor_collapse_se, :enemy_collapse_se, :words,
      :test_battlers, :test_troop_id, :start_map_id, :start_x, :start_y,
      :battleback_name, :battler_name, :battler_hue, :edit_map_id
    def initialize
      @magic_number = 0; @party_members = [1]
      @elements = [nil, ""]
      @switches = [nil]; @variables = [nil]
      @windowskin_name = ""; @title_name = ""; @gameover_name = ""
      @battle_transition = ""
      @title_bgm = AudioFile.new; @battle_bgm = AudioFile.new
      @battle_end_me = AudioFile.new; @gameover_me = AudioFile.new
      @cursor_se = AudioFile.new("Cursor")
      @decision_se = AudioFile.new("Decision")
      @cancel_se = AudioFile.new("Cancel")
      @buzzer_se = AudioFile.new("Buzzer")
      @equip_se = AudioFile.new("Equip")
      @shop_se = AudioFile.new("Shop")
      @save_se = AudioFile.new("Save")
      @load_se = AudioFile.new("Load")
      @battle_start_se = AudioFile.new("BattleStart")
      @escape_se = AudioFile.new("Escape")
      @actor_collapse_se = AudioFile.new("ActorCollapse")
      @enemy_collapse_se = AudioFile.new("EnemyCollapse")
      @words = Words.new
      @test_battlers = []; @test_troop_id = 1
      @start_map_id = 1; @start_x = 0; @start_y = 0
      @battleback_name = ""; @battler_name = ""; @battler_hue = 0
      @edit_map_id = 1
    end
    class Words
      attr_accessor :gold, :hp, :sp, :str, :dex, :agi, :int, :atk,
        :pdef, :mdef, :skill, :item, :weapon, :armor1, :armor2,
        :armor3, :armor4, :attack, :guard
      def initialize
        @gold = ""; @hp = ""; @sp = ""
        @str = ""; @dex = ""; @agi = ""; @int = ""
        @atk = ""; @pdef = ""; @mdef = ""
        @skill = ""; @item = ""; @weapon = ""
        @armor1 = ""; @armor2 = ""; @armor3 = ""; @armor4 = ""
        @attack = ""; @guard = ""
      end
    end
    class TestBattler
      attr_accessor :actor_id, :level, :weapon_id, :armor1_id,
        :armor2_id, :armor3_id, :armor4_id
      def initialize
        @actor_id = 1; @level = 1
        @weapon_id = 0; @armor1_id = 0; @armor2_id = 0
        @armor3_id = 0; @armor4_id = 0
      end
    end
  end

  # ==========================================================================
  # RPG::Cache (Referenzimplementierung aus dem RGSS-Handbuch, adaptiert)
  # ==========================================================================
  module Cache
    @cache = {}
    def self.load_bitmap(folder_name, filename, hue = 0)
      path = folder_name + filename.to_s
      if (not @cache.include?(path)) or @cache[path].disposed?
        if filename != "" and filename != nil
          @cache[path] = Bitmap.new(path)
        else
          @cache[path] = Bitmap.new(32, 32)
        end
      end
      if hue == 0
        @cache[path]
      else
        key = [path, hue]
        if (not @cache.include?(key)) or @cache[key].disposed?
          @cache[key] = @cache[path].clone
          @cache[key].hue_change(hue)
        end
        @cache[key]
      end
    end
    def self.animation(filename, hue);  load_bitmap("Graphics/Animations/", filename, hue); end
    def self.autotile(filename);        load_bitmap("Graphics/Autotiles/", filename); end
    def self.battleback(filename);      load_bitmap("Graphics/Battlebacks/", filename); end
    def self.battler(filename, hue);    load_bitmap("Graphics/Battlers/", filename, hue); end
    def self.character(filename, hue);  load_bitmap("Graphics/Characters/", filename, hue); end
    def self.fog(filename, hue);        load_bitmap("Graphics/Fogs/", filename, hue); end
    def self.gameover(filename);        load_bitmap("Graphics/Gameovers/", filename); end
    def self.icon(filename);            load_bitmap("Graphics/Icons/", filename); end
    def self.panorama(filename, hue);   load_bitmap("Graphics/Panoramas/", filename, hue); end
    def self.picture(filename);         load_bitmap("Graphics/Pictures/", filename); end
    def self.tileset(filename);         load_bitmap("Graphics/Tilesets/", filename); end
    def self.title(filename);           load_bitmap("Graphics/Titles/", filename); end
    def self.transition(filename);      load_bitmap("Graphics/Transitions/", filename); end
    def self.windowskin(filename);      load_bitmap("Graphics/Windowskins/", filename); end
    def self.tile(filename, tile_id, hue)
      key = [filename, tile_id, hue]
      if (not @cache.include?(key)) or @cache[key].disposed?
        @cache[key] = Bitmap.new(32, 32)
        x = (tile_id - 384) % 8 * 32
        y = (tile_id - 384) / 8 * 32
        rect = Rect.new(x, y, 32, 32)
        @cache[key].blt(0, 0, tileset(filename), rect)
        @cache[key].hue_change(hue)
      end
      @cache[key]
    end
    def self.clear
      @cache = {}
    end
  end

  # ==========================================================================
  # RPG::Sprite (Effekt-Sprite, XP-Befehle als Naeherung ueber Sprite-API)
  # ==========================================================================
  class Sprite < ::Sprite
    def initialize(viewport = nil)
      super(viewport)
      @_effect_type = nil
      @_effect_start = 0
      @_effect_duration = 0
      @_blink = false
      @_damage = nil
      @_animation = nil
      @_loop_animation = nil
    end

    def update
      super
      now = Graphics.frame_count
      # Blinken (8-Frame-Raster)
      if @_blink
        self.visible = (now % 32 < 4) ? false : true
      end
      # Laufende Effekte (jeweils 20 Frames)
      if @_effect_type
        t = now - @_effect_start
        p = @_effect_duration > 0 ? t.to_f / @_effect_duration : 1.0
        p = 1.0 if p > 1.0
        case @_effect_type
        when :whiten
          # kurzes Aufblitzen: Farbe weiss auslaufend
          a = (255 * (1.0 - p)).to_i
          self.color = Color.new(255, 255, 255, a)
          @_effect_type = nil if p >= 1.0
        when :appear
          self.opacity = (255 * p).to_i
          @_effect_type = nil if p >= 1.0
        when :disappear
          self.opacity = (255 * (1.0 - p)).to_i
          @_effect_type = nil if p >= 1.0
        when :escape
          self.x += (self.zoom_x rescue 1.0) * 2
          self.opacity = (255 * (1.0 - p)).to_i
          @_effect_type = nil if p >= 1.0
        when :collapse
          self.zoom_y = 1.0 - p
          self.opacity = (255 * (1.0 - p)).to_i
          @_effect_type = nil if p >= 1.0
        end
      end
    end

    def whiten
      @_effect_type = :whiten
      @_effect_start = Graphics.frame_count
      @_effect_duration = 16
      self.color = Color.new(255, 255, 255, 128)
    end

    def appear
      @_effect_type = :appear
      @_effect_start = Graphics.frame_count
      @_effect_duration = 20
      self.opacity = 0
    end

    def disappear
      @_effect_type = :disappear
      @_effect_start = Graphics.frame_count
      @_effect_duration = 20
    end

    def escape
      @_effect_type = :escape
      @_effect_start = Graphics.frame_count
      @_effect_duration = 30
    end

    def collapse
      @_effect_type = :collapse
      @_effect_start = Graphics.frame_count
      @_effect_duration = 48
    end

    def damage(value, critical)
      # Zahlen-Pop-Up: Naeherung ueber kurzen weissen/roten Blitz
      @_damage = [value, critical]
      if value.is_a?(Integer) && value < 0
        self.flash(Color.new(255, 255, 255, 255), 4)
      elsif critical
        self.flash(Color.new(255, 64, 64, 255), 6)
      else
        self.flash(Color.new(255, 255, 255, 192), 4)
      end
    end
    def damage?(junk = nil); false; end

    def animation(animation, hit)
      @_animation = animation
      self.flash(Color.new(255, 255, 255, 160), 5) if hit
    end
    def animation?(junk = nil); false; end

    def loop_animation(animation)
      @_loop_animation = animation
      self.opacity = 255
    end
    def loop_animation?(junk = nil); false; end

    def blink_on; @_blink = true; end
    def blink_off
      @_blink = false
      self.visible = true
    end
    def blink?; @_blink; end

    def effect?(junk = nil)
      !@_effect_type.nil?
    end
    def effect_type(junk = nil)
      @_effect_type || :none
    end
  end

  # ==========================================================================
  # RPG::Weather (Regen/Sturm/Schnee-Matrix ueber native Sprites)
  # ==========================================================================
  class Weather
    attr_accessor :ox, :oy
    attr_reader :type, :max

    def initialize(viewport = nil)
      @viewport = viewport || Viewport.new(0, 0, 640, 480)
      @type = 0
      @max = 0
      @ox = 0.0
      @oy = 0.0
      @sprites = []
      @rain_bitmap = Bitmap.new(8, 32)
      (0...24).each do |i|
        @rain_bitmap.set_pixel(3 - (i * 2 / 24), i + 4, Color.new(180, 200, 235, 140))
        @rain_bitmap.set_pixel(4 - (i * 2 / 24), i + 4, Color.new(180, 200, 235, 110))
      end
      @snow_bitmap = Bitmap.new(6, 6)
      @snow_bitmap.fill_rect(1, 1, 4, 4, Color.new(255, 255, 255, 220))
      @storm_bitmap = Bitmap.new(8, 48)
      (0...40).each do |i|
        @storm_bitmap.set_pixel(3 - (i * 3 / 40), i + 4, Color.new(160, 180, 215, 200))
        @storm_bitmap.set_pixel(4 - (i * 3 / 40), i + 4, Color.new(160, 180, 215, 170))
      end
    end

    def type=(t)
      return if t == @type
      @type = [[t.to_i, 0].max, 3].min
      rebuild_sprites
    end

    def max=(m)
      m = [[m.to_i, 0].max, 40].min
      return if m == @max
      @max = m
      rebuild_sprites
    end

    def update
      @sprites.each_with_index do |sp, i|
        case @type
        when 1 # Regen
          sp.y += 26
          sp.x -= 4
          if sp.y > 480
            sp.y = @sprites[i].y - 480 - 32
            sp.x = pseudo(i * 7 + Graphics.frame_count) % 640
            sp.y = -32
          end
        when 2 # Sturm
          sp.y += 48
          sp.x -= 10
          if sp.y > 480
            sp.x = pseudo(i * 11 + Graphics.frame_count) % 640
            sp.y = -48
          end
        when 3 # Schnee
          ph = (Graphics.frame_count + i * 13).to_f * 0.03
          sp.x += Math.sin(ph) + 0.3
          sp.y += 3 + (i % 3)
          if sp.y > 480
            sp.x = pseudo(i * 5 + Graphics.frame_count) % 640
            sp.y = -6
          end
          sp.angle += 2
        end
      end
    end

    def dispose
      @sprites.each { |sp| sp.dispose }
      @sprites = []
      @rain_bitmap.dispose
      @snow_bitmap.dispose
      @storm_bitmap.dispose
    end
    def disposed?; @sprites.empty? && @rain_bitmap.disposed?; end

    private
    def pseudo(n)
      (n * 1103515245 + 12345) % 65536
    end

    def rebuild_sprites
      @sprites.each { |sp| sp.dispose }
      @sprites = []
      return if @type == 0 || @max == 0
      bmp = @type == 3 ? @snow_bitmap : (@type == 2 ? @storm_bitmap : @rain_bitmap)
      count = @max * 12 # Partikel-Dichte
      count.times do |i|
        sp = ::Sprite.new(@viewport)
        sp.bitmap = bmp
        sp.src_rect = Rect.new(0, 0, bmp.width, bmp.height)
        sp.x = pseudo(i * 31) % 640
        sp.y = pseudo(i * 57) % 480
        sp.opacity = @type == 3 ? 200 : 160
        sp.z = 999
        @sprites << sp
      end
    end
  end
end

)RUBY";

} // namespace rpg
