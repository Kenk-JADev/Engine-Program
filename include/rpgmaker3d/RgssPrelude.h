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

# ---------------------------------------------------------------------------
# XP load_data-Bruecke (PAKET 6, XP_Scripts schrittweise): Die Original-
# Skripte laden $data_* per load_data("Data/X.rxdata"). Unsere Datenbank ist
# JSON im Datenbank-Dialog — die Bruecke mappt die bekannten rxdata-Namen
# auf __engine_db_fetch (nativ, liefert generische Hashes) und baut daraus
# XP-konforme RPG::*-Objekte ([nil] + Eintraege, Index = ID wie in XP).
# Nicht verdrahtete Dateien (u.a. Map%03d.rxdata, CommonEvents.rxdata,
# BT_*.rxdata) werfen weiterhin den erklaerenden Fehler — schrittweise.
# ---------------------------------------------------------------------------
def load_data(filename)
  base = filename.to_s
  slash = base.rindex("/")
  base = base[(slash + 1)..-1] if slash
  # Dynamische Kartendateien (XP Game_Map.setup: "Data/Map%03d.rxdata"):
  # Zahl extrahieren — ohne Regexp (mruby-Kern hat keine).
  if base.length >= 12 and base[0..2] == "Map" and base[-7..-1] == ".rxdata"
    num = base[3..-8].to_i
    if num > 0
      rows = __engine_db_fetch("map", num)
      if rows.nil?
        raise RGSSError, "load_data(\"#{filename}\"): Karte nicht gefunden " \
          "(maps/map#{num}.map im Projekt)."
      end
      return __engine_db_build("map", rows)
    end
  end
  kind = case base
    when "Actors.rxdata"      then "actors"
    when "Classes.rxdata"     then "classes"
    when "Skills.rxdata"      then "skills"
    when "Items.rxdata"       then "items"
    when "Weapons.rxdata"     then "weapons"
    when "Armors.rxdata"      then "armors"
    when "Enemies.rxdata"     then "enemies"
    when "Troops.rxdata"      then "troops"
    when "States.rxdata"      then "states"
    when "Animations.rxdata"  then "animations"
    when "Tilesets.rxdata"    then "tilesets"
    when "System.rxdata"      then "system"
    when "MapInfos.rxdata"    then "mapinfos"
    when "CommonEvents.rxdata" then "common_events"
    else nil
  end
  # Kampf-Testdateien (BT_*.rxdata): XP-Trennung Test/Produktiv fuehren wir
  # nicht — ehrlicher Fallback: Test-Schiene liest die Produktivdaten
  # (BT_Tilesets.rxdata -> tilesets usw.). System.rxdata hat kein BT_-Pendant
  # (battler/test_battlers bleiben Standard).
  if kind.nil? and base.length >= 12 and base[0..2] == "BT_" and base[-7..-1] == ".rxdata"
    sub = base[3..-1]
    kind = case sub
      when "Actors.rxdata"     then "actors"
      when "Classes.rxdata"    then "classes"
      when "Skills.rxdata"     then "skills"
      when "Items.rxdata"      then "items"
      when "Weapons.rxdata"    then "weapons"
      when "Armors.rxdata"     then "armors"
      when "Enemies.rxdata"    then "enemies"
      when "Troops.rxdata"     then "troops"
      when "States.rxdata"     then "states"
      when "Animations.rxdata" then "animations"
      when "Tilesets.rxdata"   then "tilesets"
      when "CommonEvents.rxdata" then "common_events"
      else nil
    end
  end
  if kind.nil?
    raise RGSSError, "load_data(\"#{filename}\"): .rxdata/Marshal wird nicht " \
      "unterstuetzt und fuer diese Datei gibt es noch keine JSON-Bruecke " \
      "(verdrahtet: Actors, Classes, Skills, Items, Weapons, Armors, " \
      "Enemies, Troops, States, Animations, Tilesets, System, MapInfos, " \
      "CommonEvents + Map%03d.rxdata; BT_*.rxdata mit Fallback auf die " \
      "Produktivdaten; offen nur: Marshal-Savefiles - TODO_XP_PARITY.md)."
  end
  rows = __engine_db_fetch(kind)
  if rows.nil?
    raise RGSSError, "load_data(\"#{filename}\"): Engine-Datenbank leer."
  end
  __engine_db_build(kind, rows)
end

# Baut aus den generischen DB-Hashes (nativ) die RPG::*-Objekte.
def __engine_db_build(kind, rows)
  case kind
  when "map"
    # Stufe 2a: volle Geometrie; Events leer (NPCs laufen nativ ueber das
    # EventSystem — XP-Skripte iterieren $game_map.events gefahrlos).
    m = RPG::Map.new(rows[:width], rows[:height])
    m.tileset_id = rows[:tileset_id]
    m.encounter_step = rows[:encounter_step]
    m.encounter_list = rows[:encounter_list]
    w = rows[:width]; h = rows[:height]
    t = Table.new(w, h, 3)
    rows[:layers].each_with_index do |lay, li|
      break if li >= 3
      for y in 0...h
        for x in 0...w
          v = lay[y * w + x]
          if v.nil? or v < 0
            t[x, y, li] = 0
          else
            # native Tile-ID -> RGSS-ID (0..383 sind Autotiles bei XP)
            t[x, y, li] = v + 384
          end
        end
      end
    end
    m.data = t
    m.events = {}
    return m
  when "actors"
    out = [nil]
    rows.each do |r|
      a = RPG::Actor.new
      a.id = r[:id]; a.name = r[:name]; a.class_id = r[:class_id]
      a.initial_level = r[:initial_level]; a.final_level = r[:final_level]
      a.character_name = r[:character_name]; a.battler_name = r[:battler_name]
      # Parameterkurve: linear initial -> final ueber 1..99
      # (XP: maxhp/maxsp/str/dex/agi/int; unsere Stats: mhp/mmp/atk/def/mat/agi)
      t = Table.new(6, 100)
      for lv in 1..99
        f = (lv - 1) / 98.0
        t[0, lv] = (r[:init_mhp] + (r[:fin_mhp] - r[:init_mhp]) * f).to_i
        t[1, lv] = (r[:init_mmp] + (r[:fin_mmp] - r[:init_mmp]) * f).to_i
        t[2, lv] = (r[:init_atk] + (r[:fin_atk] - r[:init_atk]) * f).to_i
        t[3, lv] = (r[:init_def] + (r[:fin_def] - r[:init_def]) * f).to_i
        t[4, lv] = (r[:init_agi] + (r[:fin_agi] - r[:init_agi]) * f).to_i
        t[5, lv] = (r[:init_mat] + (r[:fin_mat] - r[:init_mat]) * f).to_i
      end
      a.parameters = t
      out.push(a)
    end
    return out
  when "classes"
    out = [nil]
    rows.each do |r|
      c = RPG::Class.new
      c.id = r[:id]; c.name = r[:name]
      ls = []
      r[:learnings].each do |pair|
        l = RPG::Class::Learning.new
        l.level = pair[0]; l.skill_id = pair[1]
        ls.push(l)
      end
      c.learnings = ls
      out.push(c)
    end
    return out
  when "skills"
    out = [nil]
    rows.each do |r|
      s = RPG::Skill.new
      s.id = r[:id]; s.name = r[:name]; s.description = r[:description]
      s.scope = r[:scope]; s.sp_cost = r[:sp_cost]; s.power = r[:power]
      s.animation1_id = r[:animation1_id]
      out.push(s)
    end
    return out
  when "items"
    out = [nil]
    rows.each do |r|
      i = RPG::Item.new
      i.id = r[:id]; i.name = r[:name]; i.description = r[:description]
      i.price = r[:price]; i.consumable = r[:consumable]; i.scope = r[:scope]
      i.recover_hp = r[:recover_hp]; i.recover_sp = r[:recover_sp]
      i.animation1_id = r[:animation1_id]
      out.push(i)
    end
    return out
  when "weapons"
    out = [nil]
    rows.each do |r|
      w = RPG::Weapon.new
      w.id = r[:id]; w.name = r[:name]; w.description = r[:description]
      w.price = r[:price]; w.atk = r[:atk]; w.animation1_id = r[:animation1_id]
      out.push(w)
    end
    return out
  when "armors"
    out = [nil]
    rows.each do |r|
      ar = RPG::Armor.new
      ar.id = r[:id]; ar.name = r[:name]; ar.description = r[:description]
      ar.price = r[:price]; ar.pdef = r[:pdef]; ar.mdef = r[:mdef]
      ar.kind = r[:kind]
      out.push(ar)
    end
    return out
  when "enemies"
    out = [nil]
    rows.each do |r|
      e = RPG::Enemy.new
      e.id = r[:id]; e.name = r[:name]
      e.battler_name = r[:battler_name]; e.battler_hue = r[:battler_hue]
      e.maxhp = r[:maxhp]; e.maxsp = r[:maxsp]
      e.str = r[:str]; e.dex = r[:dex]; e.agi = r[:agi]; e.int = r[:int]
      e.atk = r[:atk]; e.pdef = r[:pdef]; e.mdef = r[:mdef]
      e.exp = r[:exp]; e.gold = r[:gold]
      e.item_id = r[:item_id] if r[:item_id] > 0
      out.push(e)
    end
    return out
  when "troops"
    out = [nil]
    rows.each do |r|
      t = RPG::Troop.new
      t.id = r[:id]; t.name = r[:name]
      ms = []
      r[:members].each do |eid|
        m = RPG::Troop::Member.new
        m.enemy_id = eid
        # XP-Member-Koordinaten speichert unsere DB nicht -> in einer Reihe
        # auffaechern, damit Spriteset_Battle sie nicht stapelt (Naeherung).
        m.x = 120 + ms.length * 100; m.y = 250
        ms.push(m)
      end
      t.members = ms
      ps = []
      r[:pages].each do |ph|
        p = RPG::Troop::Page.new
        p.span = ph[:span]
        c = p.condition
        c.turn_valid = ph[:turn_valid]; c.turn_a = ph[:turn_a]; c.turn_b = ph[:turn_b]
        c.enemy_valid = ph[:enemy_valid]; c.enemy_index = ph[:enemy_index]
        c.enemy_hp = ph[:enemy_hp]
        c.actor_valid = ph[:actor_valid]; c.actor_id = ph[:actor_id]
        c.actor_hp = ph[:actor_hp]
        c.switch_valid = ph[:switch_valid]; c.switch_id = ph[:switch_id]
        if ph[:common_event_id] > 0
          ec = RPG::EventCommand.new
          ec.code = 117; ec.parameters = [ph[:common_event_id]]
          p.list = [ec]
        end
        ps.push(p)
      end
      t.pages = ps
      out.push(t)
    end
    return out
  when "states"
    out = [nil]
    rows.each do |r|
      s = RPG::State.new
      s.id = r[:id]; s.name = r[:name]
      s.restriction = r[:restriction]; s.rating = r[:rating]
      s.slip_damage = r[:slip_damage]; s.battle_only = r[:battle_only]
      s.hold_turn = r[:hold_turn]; s.auto_release_prob = r[:auto_release_prob]
      out.push(s)
    end
    return out
  when "animations"
    out = [nil]
    rows.each do |r|
      an = RPG::Animation.new
      an.id = r[:id]; an.name = r[:name]
      an.animation_name = r[:animation_name]; an.position = r[:position]
      an.frame_max = r[:frame_max]
      fr = []
      ti = []
      r[:frames].each_with_index do |fh, fi|
        f = RPG::Animation::Frame.new
        cells = fh[:cells]
        f.cell_max = cells.length
        if f.cell_max > 0
          t = Table.new(f.cell_max, 8)
          cells.each_with_index do |c, j|
            t[j, 0] = c[0]; t[j, 1] = c[1]; t[j, 2] = c[2]
            t[j, 3] = c[3]; t[j, 4] = c[4]; t[j, 5] = 0 # flip
            t[j, 6] = c[5]; t[j, 7] = 0 # blend
          end
          f.cell_data = t
        end
        fr.push(f)
        if fh[:se_name] != "" or fh[:flash_scope] > 0
          tm = RPG::Animation::Timing.new
          tm.frame = fi
          tm.se = RPG::AudioFile.new(fh[:se_name], fh[:se_volume], fh[:se_pitch])
          tm.flash_scope = fh[:flash_scope]
          tm.flash_color = Color.new(fh[:flash_r], fh[:flash_g], fh[:flash_b], 255)
          tm.flash_duration = fh[:flash_duration]
          tm.condition = 0
          ti.push(tm)
        end
      end
      an.frames = fr
      an.timings = ti
      out.push(an)
    end
    return out
  when "tilesets"
    out = [nil]
    rows.each do |r|
      ts = RPG::Tileset.new
      ts.id = r[:id]; ts.name = r[:name]; ts.tileset_name = r[:tileset_name]
      ts.autotile_names = r[:autotile_names]
      ts.panorama_name = r[:panorama_name]; ts.fog_name = r[:fog_name]
      ts.battleback_name = r[:battleback_name]
      # Unsere Flag-Tabellen ohne RGSS-Autotile-Offset -> +384 verschieben
      n = r[:passages].length
      pas = Table.new(384 + n)
      pri = Table.new(384 + n)
      ter = Table.new(384 + n)
      for i in 0...n
        pas[384 + i] = r[:passages][i]
        pv = r[:priorities][i];  pri[384 + i] = pv || 0
        tg = r[:terrain_tags][i]; ter[384 + i] = tg || 0
      end
      ts.passages = pas; ts.priorities = pri; ts.terrain_tags = ter
      out.push(ts)
    end
    return out
  when "system"
    r = rows
    s = RPG::System.new
    s.party_members = r[:party_members].length > 0 ? r[:party_members] : [1]
    elems = [nil]; r[:elements].each { |e| elems.push(e) }
    s.elements = elems
    sw = [nil]; r[:switches].each { |nm| sw.push(nm) }
    s.switches = sw
    va = [nil]; r[:variables].each { |nm| va.push(nm) }
    s.variables = va
    s.windowskin_name = r[:windowskin_name]
    s.title_name = r[:title_name]; s.gameover_name = r[:gameover_name]
    s.battle_transition = r[:battle_transition]
    s.title_bgm = RPG::AudioFile.new(r[:title_bgm])
    s.battle_bgm = RPG::AudioFile.new(r[:battle_bgm])
    s.battle_end_me = RPG::AudioFile.new(r[:battle_end_me])
    s.gameover_me = RPG::AudioFile.new(r[:gameover_me])
    s.cursor_se = RPG::AudioFile.new(r[:cursor_se])
    s.decision_se = RPG::AudioFile.new(r[:decision_se])
    s.cancel_se = RPG::AudioFile.new(r[:cancel_se])
    s.buzzer_se = RPG::AudioFile.new(r[:buzzer_se])
    s.equip_se = RPG::AudioFile.new(r[:equip_se])
    s.shop_se = RPG::AudioFile.new(r[:shop_se])
    s.save_se = RPG::AudioFile.new(r[:save_se])
    s.load_se = RPG::AudioFile.new(r[:load_se])
    s.battle_start_se = RPG::AudioFile.new(r[:battle_start_se])
    s.escape_se = RPG::AudioFile.new(r[:escape_se])
    s.actor_collapse_se = RPG::AudioFile.new(r[:actor_collapse_se])
    s.enemy_collapse_se = RPG::AudioFile.new(r[:enemy_collapse_se])
    w = RPG::System::Words.new
    w.gold = r[:word_gold]; w.hp = r[:word_hp]; w.sp = r[:word_sp]
    w.str = r[:word_str]; w.dex = r[:word_dex]; w.agi = r[:word_agi]
    w.int = r[:word_int]; w.atk = r[:word_atk]; w.pdef = r[:word_pdef]
    w.mdef = r[:word_mdef]; w.skill = r[:word_skill]; w.item = r[:word_item]
    w.weapon = r[:word_weapon]; w.armor1 = r[:word_armor1]
    w.armor2 = r[:word_armor2]; w.armor3 = r[:word_armor3]
    w.armor4 = r[:word_armor4]; w.attack = r[:word_attack]; w.guard = r[:word_guard]
    s.words = w
    s.start_map_id = r[:start_map_id]; s.start_x = r[:start_x]; s.start_y = r[:start_y]
    return s
  when "mapinfos"
    out = {}
    rows.each do |r|
      mi = RPG::MapInfo.new
      mi.name = r[:name]; mi.parent_id = r[:parent_id]; mi.order = r[:order]
      mi.expanded = r[:expanded]; mi.scroll_x = r[:scroll_x]; mi.scroll_y = r[:scroll_y]
      out[r[:id]] = mi
    end
    return out
  when "common_events"
    out = [nil]
    rows.each do |r|
      ce = RPG::CommonEvent.new
      ce.id = r[:id]; ce.name = r[:name]
      ce.trigger = r[:trigger]; ce.switch_id = r[:switch_id]
      ls = []
      r[:list].each do |c|
        ec = RPG::EventCommand.new
        ec.code = c[:code]; ec.indent = c[:indent]
        # Roh-Parameter (p1..p3, text, params[]) zusammenfuehren und
        # Trailing-Defaults kuerzen (XP-Daten enden nicht auf 0/"") —
        # unsere Codes sind exakt XP-kodiert (101..355).
        pars = [c[:p1], c[:p2], c[:p3]]
        pars.push(c[:text]) if c[:text] != ""
        c[:params].each { |p| pars.push(p) }
        while pars.length > 0 and (pars[-1] == 0 or pars[-1] == "")
          pars.pop
        end
        ec.parameters = pars
        ls.push(ec)
      end
      ce.list = ls
      out.push(ce)
    end
    return out
  end
  nil
end

# ---------------------------------------------------------------------------
# XP $game_actors (Stufe 4g): Die Klasse Game_Actor ist nativ an die
# Party-Laufzeit (GameActor-Struct) gekoppelt; diese Sammlung liefert die
# XP-Identitaet (pro Akteur-ID immer dieselbe Instanz, Cache).
# ---------------------------------------------------------------------------
class Game_Actors
  def initialize
    @cache = {}
  end
  def [](actor_id)
    @cache[actor_id] ||= Game_Actor.new(actor_id)
  end
end
$game_actors = Game_Actors.new

class Game_Actor
  # XP-Bequemlichkeit, nativ nicht abgedeckt: Intelligenz aus der Akteur-
  # Parameter-Tabelle der load_data-Bruecke (lineare init->fin-Kurve).
  def int
    all = ($__engine_db_actors ||= load_data("Data/Actors.rxdata"))
    row = all[id]
    row ? row.parameters[5, level] : 0
  end
  # Ausruestung als Objekte (XP liest z.B. actor.weapon.atk)
  def weapon
    ($__engine_db_weapons ||= load_data("Data/Weapons.rxdata"))[weapon_id]
  end
  def armor1
    ($__engine_db_armors ||= load_data("Data/Armors.rxdata"))[armor1_id]
  end
  def armor2
    ($__engine_db_armors ||= load_data("Data/Armors.rxdata"))[armor2_id]
  end
  def armor3
    ($__engine_db_armors ||= load_data("Data/Armors.rxdata"))[armor3_id]
  end
  def armor4
    ($__engine_db_armors ||= load_data("Data/Armors.rxdata"))[armor4_id]
  end
end

# ---------------------------------------------------------------------------
# XP Game_Party-Komfort (Stufe 4g): actors liefert Game_Actor-Objekte aus
# $game_actors (gleiche Instanzen = XP-Identitaet). items/weapons/armors
# liefern — abweichend zu XPs Hash — die anteiligen ID-Listen (Naeherung,
# Anzahlfragen laufen praezise ueber item_number/weapon_number/armor_number).
# ---------------------------------------------------------------------------
class Game_Party
  def actors
    a = []
    __actor_ids.each { |aid| a.push($game_actors[aid]) }
    a
  end
  def actor(actor_id)
    $game_actors[actor_id]
  end
  def items
    __item_ids
  end
  def weapons
    __weapon_ids
  end
  def armors
    __armor_ids
  end
  def max_level
    lv = 0
    actors.each { |a| lv = a.level if a.level > lv }
    lv
  end
  def average_level
    a = actors
    return 0 if a.length == 0
    sum = 0
    a.each { |x| sum += x.level }
    sum / a.length
  end
  def item_can_use?(item_id)
    return false if item_number(item_id) <= 0
    it = ($__engine_db_items ||= load_data("Data/Items.rxdata"))[item_id]
    it ? it.consumable : false
  end
  def movable?
    not all_dead?
  end
end

# ---------------------------------------------------------------------------
# XP Game_Actor#equip (Stufe 4g Teil 3): Inventar-Tausch mit der Party
# (altes Stueck zurueck ins Lager, neues herausgenommen). Der nativ gebundene
# change_equip setzt rein datengetrieben — exakt die XP-Aufteilung.
# ---------------------------------------------------------------------------
class Game_Actor
  def equip(equip_type, id)
    if equip_type == 0
      if id == 0 or $game_party.weapon_number(id) > 0
        old = weapon_id
        $game_party.gain_weapon(old, 1) if old > 0
        change_equip(0, id)
        $game_party.gain_weapon(id, -1) if id > 0
      end
    else
      if equip_type >= 1 and equip_type <= 4
        old = [armor1_id, armor2_id, armor3_id, armor4_id][equip_type - 1]
        if id == 0 or $game_party.armor_number(id) > 0
          $game_party.gain_armor(old, 1) if old and old > 0
          change_equip(equip_type, id)
          $game_party.gain_armor(id, -1) if id > 0
        end
      end
    end
  end
end

# ---------------------------------------------------------------------------
# XP Game_Enemy-Komfort (Stufe 4g Teil 3): Die Klasse ist nativ an die
# Kampf-Battler gekoppelt; Zustaende haelt die Bruecke objektbezogen als
# Ruby-Ivar (unser nativer Kampf kennt Stand heute keine Gegner-Zustaende —
# dokumentierte Naeherung; add/remove_state/states wie in RPGXP).
# ---------------------------------------------------------------------------
class Game_Enemy
  attr_accessor :letter
  def states
    if @states == nil
      @states = []
    end
    @states
  end
  def add_state(state_id, force = false)
    s = states
    found = false
    i = 0
    while i < s.length
      if s[i] == state_id
        found = true
      end
      i += 1
    end
    unless found
      s.push(state_id)
    end
    s
  end
  def remove_state(state_id, force = false)
    s = states
    out = []
    i = 0
    while i < s.length
      if s[i] != state_id
        out.push(s[i])
      end
      i += 1
    end
    @states = out
  end
end

# ---------------------------------------------------------------------------
# XP $game_troop (Stufe 4g Teil 3): setup(troop_id) baut die members aus
# __enemy_ids (nativ: Live-Kampf-Battler bevorzugt, sonst TroopData) und
# bindet jedes Game_Enemy per __attach an seinen Kampf-Slot. Der Hook
# Game.onBattleStarted ruft setup bei JEDEM Kampfstart automatisch auf
# (Skript- wie Event-Weg), so wie es XPs Scene_Battle per Hand macht.
# ---------------------------------------------------------------------------
class Game_Troop
  attr_reader :troop_id, :members
  def initialize
    @troop_id = 0
    @members = []
  end
  def setup(troop_id)
    @troop_id = troop_id
    @members = []
    ids = __enemy_ids(troop_id)
    i = 0
    while i < ids.length
      e = Game_Enemy.new(ids[i])
      e.__attach(i)
      @members.push(e)
      i += 1
    end
    @members
  end
end
$game_troop = Game_Troop.new

# ---------------------------------------------------------------------------
# XP Game_Picture (Stufe 4g Teil 3): adressiert die native Bildschicht
# (GameUI) ueber die bei show gemerkte Laufzeit-ID. Naeherungen (alle hier
# dokumentiert): nativ wird zentriert (origin-1-Verhalten) und mit EINER
# Skalierung gezeichnet (zoom_x/zoom_y gemittelt); rotate(speed) ist bei XP
# kontinuierlich — hier Tween-Schritt je Aufruf; start_tone_change wird nur
# als Zustand gespeichert (kein nativer Picture-Ton-Renderer).
# ---------------------------------------------------------------------------
class Game_Picture
  attr_reader :number, :name, :origin, :x, :y, :zoom_x, :zoom_y, :opacity, :blend_type, :angle, :tone
  def initialize(number)
    @number = number
    @name = ""
    @origin = 0
    @x = 0.0
    @y = 0.0
    @zoom_x = 100.0
    @zoom_y = 100.0
    @opacity = 255.0
    @blend_type = 1
    @angle = 0.0
    @tone = Tone.new(0, 0, 0, 0)
    @native_id = nil
  end
  def show(name, origin, x, y, zoom_x, zoom_y, opacity, blend_type)
    @name = name
    @origin = origin
    @x = x.to_f
    @y = y.to_f
    @zoom_x = zoom_x.to_f
    @zoom_y = zoom_y.to_f
    @opacity = opacity.to_f
    @blend_type = blend_type
    @native_id = Game.show_picture(name, "xp_pic_" + number.to_s,
      x / 640.0, y / 480.0, (zoom_x + zoom_y) / 200.0, opacity / 255.0)
  end
  def move(duration, origin, x, y, zoom_x, zoom_y, opacity, blend_type)
    @origin = origin
    @x = x.to_f
    @y = y.to_f
    @zoom_x = zoom_x.to_f
    @zoom_y = zoom_y.to_f
    @opacity = opacity.to_f
    @blend_type = blend_type
    if @native_id != nil
      UI.tween_picture(@native_id, @x / 640.0, @y / 480.0,
        (@zoom_x + @zoom_y) / 200.0, @opacity / 255.0, @angle,
        duration / 40.0 + 0.01)
    end
  end
  def rotate(speed)
    @angle = (@angle + speed * 20.0) % 360
    if @native_id != nil
      UI.tween_picture(@native_id, @x / 640.0, @y / 480.0,
        (@zoom_x + @zoom_y) / 200.0, @opacity / 255.0, @angle, 1.0)
    end
  end
  def start_tone_change(tone, duration)
    @tone = tone
    @tone_duration = duration
  end
  def fade(duration)
    @opacity = 0.0
    if @native_id != nil
      UI.tween_picture(@native_id, @x / 640.0, @y / 480.0,
        (@zoom_x + @zoom_y) / 200.0, 0.0, @angle, duration / 40.0 + 0.01)
    end
  end
  def erase
    if @native_id != nil
      UI.remove_picture(@native_id)
      @native_id = nil
    end
    @name = ""
  end
  def update
    # Tweens laufen nativ — Kompat-No-op
  end
end

# ---------------------------------------------------------------------------
# XP $game_screen (Stufe 4g Teil 3): Klasse nativ an ScreenEffects gekoppelt
# (start_flash/start_tone_change/start_shake mit XP-Frames 40/s). Hier in
# Ruby dazu: pictures (51 Game_Picture, XP 1..50 adressiert) und weather als
# reiner Zustand (type 0=kein/1=Regen/2=Sturm/3=Schnee, max = Staerke*10;
# Rendern via RPG::Weather in Ruby-Szenen, kein nativer Hook — s. TODO).
# ---------------------------------------------------------------------------
class Game_Screen
  attr_reader :weather_type, :weather_max
  def weather(type, power, duration)
    @weather_type = type
    @weather_max = power.to_f * 10.0
    @weather_duration = duration
  end
  def pictures
    if @pictures == nil
      @pictures = []
      i = 0
      while i <= 50
        @pictures.push(Game_Picture.new(i))
        i += 1
      end
    end
    @pictures
  end
  def update
    # Flash/Tone/Shake laufen als C++-Timer — Kompat-No-op
  end
end
$game_screen = Game_Screen.new

# ---------------------------------------------------------------------------
# XP Szenen-Framework (PAKET 6/h, Opt-in): Aktivierung mit
#   UI.xp_scene_mode = true      (oder Game.ini: XpSceneMode=1)
# Dann tickt die ENGINE pro Frame $scene.__engine_frame: start genau einmal,
# update pro Frame, terminate genau einmal sobald $scene auf eine andere
# Szene zeigt. Szenenwechsel wie in RPGXP per Zuweisung:
#   $scene = Scene_Karte.new
# WICHTIG: Die XP-Main.rb-Schleife `while $scene != nil; $scene.main; end`
# darf im XP-Modus NICHT verwendet werden — die Engine ownet den Frame-Loop
# (main ist hier bewusst nur die Start-Fassade; dokumentiert in TODO).
# ---------------------------------------------------------------------------
class Scene_Base
  def initialize
    @__started = false
    @__done = false
  end
  def start
  end
  def update
  end
  def terminate
  end
  # Engine-Tick (intern): start einmalig -> update pro Frame -> terminate
  # einmalig beim Szenenwechsel (XP ruft diese drei aus main auf).
  def __engine_frame
    unless @__started
      @__started = true
      start
    end
    update
    if $scene != self and not @__done
      @__done = true
      terminate
    end
  end
  # XP-Kompat-Fassade: ohne blockierende While-Schleife (Engine-Frame-Loop)
  # laeuft main hier als genau ein Frame-Tick — ehrlich dokumentiert.
  def main
    __engine_frame
  end
end
$scene = nil

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
