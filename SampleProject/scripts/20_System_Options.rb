# ============================================================================
# 20_System_Options.rb - Optionsmenue als Skript-Referenz (PAKET 45)
# ============================================================================
# XP-Philosophie wie beim Nachrichtensystem (18_System_Message.rb): WIE das
# Optionsmenue aussieht, ist SPIEL-CODE und gehoert in den Script-Editor.
# Dieses Skript ist die Engine-Referenz und vor allem die LIVE-DEMO der
# PAKET-43-Laufzeit-Optionen:
#
#   Audio.bgm_volume / =    Musik-Mixer (0.0..1.0)
#   Audio.bgs_volume / =    Hintergrund-Geraeusche
#   Audio.se_volume  / =    Soundeffekte
#   Audio.me_volume  / =    Fanfaren (Music Effects)
#   Audio.volume     / =    Master ( hier nicht belegt; bei Bedarf Zeile
#                             hinzufuegen - Volumenmodell unten )
#   Graphics.fullscreen / = Vollbild (Player; im Editor absichtlich No-Op)
#   Input.gamepad_connected?  Pad-Hinweise statt Tastatur-Hinweise
#
# AKTIVIEREN: 00_Config.rb -> SYSTEM_OPTIONS = true (Standard: aus)
# TASTE:      F6 oeffnet/schliest das Menue. Startwerte kommen aus der
#             Game.ini (BgmVolume/BgsVolume/SeVolume/MeVolume/Fullscreen).
# PFLICHT des Skripts: nichts - es ist eigenstaendig (kein Engine-Hook).
# HINWEIS: Die Werte gelten zur Laufzeit; wer sie in Savegames behalten
#          will, speichert @vals selbst (wie XP: $game_system).
# ============================================================================

if defined?(Rui) && defined?(Audio) && defined?(Graphics) && defined?(Input)

module SystemOptions
  WIN = "sys.options"
  STEPNAMES = ["Musik", "Hintergrund", "Effekte", "Fanfaren"].freeze
  N_VOL = 4            # Zeilen 0..3 = Lautstaerken, Zeile 4 = Vollbild
  ROWS = N_VOL + 1

  @open  = false
  @active = false      # Config-Opt-in (unten gesetzt)
  @row   = 0
  @vals  = [100, 100, 100, 100]
  @fs    = false
  @repFrames = 0       # Halte-Wiederholung links/rechts
  @win   = nil
  @list  = nil
  @gauge = nil
  @hint  = nil

  SW = (defined?(RPGMaker3D::Config::SCREEN_WIDTH) ?
        RPGMaker3D::Config::SCREEN_WIDTH : 1280).to_f
  SH = (defined?(RPGMaker3D::Config::SCREEN_HEIGHT) ?
        RPGMaker3D::Config::SCREEN_HEIGHT : 720).to_f

  def self.vol_get(i)
    case i
    when 0 then Audio.bgm_volume
    when 1 then Audio.bgs_volume
    when 2 then Audio.se_volume
    else        Audio.me_volume
    end
  end

  def self.vol_set(i, v01)
    case i
    when 0 then Audio.bgm_volume = v01
    when 1 then Audio.bgs_volume = v01
    when 2 then Audio.se_volume  = v01
    else        Audio.me_volume = v01
    end
  end

  def self.row_caption(i)
    if i < N_VOL
      "#{STEPNAMES[i]}   < #{@vals[i]} >"
    else
      "Vollbild   < #{@fs ? 'an' : 'aus'} >"
    end
  end

  def self.refresh_rows
    return unless @list
    items = []
    ROWS.times { |i| items << row_caption(i) }
    @list.items = items
    if @gauge
      if @row < N_VOL
        @gauge.maximum = 100
        @gauge.current = @vals[@row]
      else
        @gauge.maximum = 1
        @gauge.current = 0 # Zeile Vollbild hat keinen Balken
      end
    end
  end

  def self.open_menu
    return if @open
    @open = true
    # Startwerte aus den Mixer-Gettern (Game.ini hat sie ggf. vorgegeben)
    N_VOL.times { |i| @vals[i] = (vol_get(i) * 100.0).round }
    @fs = (Graphics.fullscreen rescue false)
    @row = 0
    w = 470.0
    h = 272.0
    win = Rui.window(WIN, (SW - w) * 0.5, (SH - h) * 0.5, w, h)
    win.openness = 0.0
    win.open
    win.add_label("title", "Optionen", 2.0, 0.0, 0, 1.1)
    items = []
    ROWS.times { |i| items << row_caption(i) }
    @list = win.add_list("rows", items, 2.0, 30.0, w - 90.0, ROWS * 26.0)
    @list.on_cancel do
      SystemOptions.close_menu
    end
    @list.on_pick do |idx|
      if idx == N_VOL
        SystemOptions.toggle_fullscreen
      else
        SystemOptions.bump(idx, 10) # Enter schaltet bequemer hoch
      end
    end
    # Balken der markierten Lautstaerke-Zeile (Darstellungs-Demo add_gauge)
    @gauge = win.add_gauge("vol", 2.0, 30.0 + ROWS * 26.0 + 10.0,
                           w - 260.0, 12.0, @vals[0], 100)
    pad = (Input.gamepad_connected? rescue false)
    @hint = win.add_label("hint",
      pad ? "Kreuz: waehlen  links/rechts: regeln  A: wechseln  B: zurueck"
          : "Pfeile: waehlen/regeln  Enter: wechseln  Esc/F6: zurueck",
      2.0, h - 60.0, 0, 1.0)
    @win = win
    Rui.set_focus_list(WIN, "rows")
    refresh_rows
  end

  def self.close_menu
    return unless @open
    @open = false
    Rui.clear_focus
    if @win
      @win.destroy rescue nil
    end
    @win = nil
    @list = nil
    @gauge = nil
    @hint = nil
  end

  def self.toggle_fullscreen
    @fs = !@fs
    Graphics.fullscreen = @fs rescue nil # Editor: absichtlich No-Op
    refresh_rows
  end

  def self.bump(i, delta)
    return if i >= N_VOL
    v = @vals[i] + delta
    v = 0 if v < 0
    v = 100 if v > 100
    return if v == @vals[i]
    @vals[i] = v
    vol_set(i, v / 100.0)
    refresh_rows
  end

  def self.update
    return unless @active

    unless @open
      if Input.key_pressed?("f6")
        # Kein Aufspringen ueber einem Skript-Dialog (PAKET-42-System)
        busy = (UI.script_dialog_active? rescue false)
        open_menu unless busy
      end
      return
    end

    # Zeile mitverfolgen (Fokus-Navigation/Maus-Hover) -> Balken aktualisieren
    cur = @list ? @list.selected : 0
    cur = 0 if cur < 0
    if cur != @row
      @row = cur
      refresh_rows
    end

    # Regeln: Rand + Halte-Wiederholung (erst 12 Frames, dann alle 3)
    dir = 0
    if Input.key_pressed?("left")
      dir = -10
      @repFrames = 0
    elsif Input.key_pressed?("right")
      dir = 10
      @repFrames = 0
    else
      @repFrames += 1
      if @repFrames > 12 && (@repFrames % 3) == 0
        dir = -10 if Input.key_down?("left")
        dir = 10  if Input.key_down?("right")
      end
    end
    if dir != 0
      if @row < N_VOL
        bump(@row, dir)
      else
        toggle_fullscreen
      end
    end

    close_menu if Input.key_pressed?("f6")
  end

  def self.activate
    @active = true
  end
end

# Opt-in aus 00_Config.rb (Standard: kein Optionsmenue, Verhalten unveraendert)
if defined?(RPGMaker3D::Config::SYSTEM_OPTIONS) &&
   RPGMaker3D::Config::SYSTEM_OPTIONS
  SystemOptions.activate
  Engine.log("Optionsmenue per Skript aktiv — F6 (20_System_Options.rb)") rescue nil
end

end # defined?(Rui) && defined?(Audio) && defined?(Graphics) && defined?(Input)
