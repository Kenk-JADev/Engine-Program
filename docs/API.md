# Oeffentliche Ruby-API — RPG Maker 3D

> **Automatisch generiert** aus den `mrb_define_*`-Bindungen
> (`src/RubyVM.cpp`, `src/RubyRgss.cpp`) via
> `tools/generate_api_docs.py`. Nicht von Hand editieren!
>
> SADS Kap. 19/21: Nur was hier steht, ist die oeffentliche,
> stabile Schnittstelle; Ruby hat keinen Direktzugriff auf
> interne C++-Objekte (Zugriff ausschliesslich ueber diese API).

**38 Module/Klassen, 576 Funktionen/Methoden.**

## `Actor`

| Art | Name | Argumente |
|---|---|---|
| Methode | `move_to` | 3 |
| Methode | `move` | 3 |
| Methode | `position` | 0 |
| Methode | `rotation` | 0 |
| Methode | `scale` | 0 |
| Methode | `set_rotation` | 3 |
| Methode | `set_scale` | 3 |
| Methode | `name` | 0 |
| Methode | `set_model` | 1 |
| Methode | `set_model_file` | 1 |
| Methode | `play_clip` | 1..2 |
| Methode | `stop_clip` | 0 |
| Methode | `set_color` | 3 |

## `Audio`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `bgm_play` | 1..5 |
| Modulfunktion | `se_play` | 1..3 |
| Modulfunktion | `bgs_play` | 1..5 |
| Modulfunktion | `me_play` | 1..3 |
| Modulfunktion | `bgm_stop` | 0 |
| Modulfunktion | `bgs_stop` | 0 |
| Modulfunktion | `fade_out` | 0..1 |
| Modulfunktion | `volume=` | 1 |
| Modulfunktion | `bgm_volume=` | 1 |
| Modulfunktion | `se_volume=` | 1 |
| Modulfunktion | `bgs_volume=` | 1 |
| Modulfunktion | `me_volume=` | 1 |
| Modulfunktion | `volume` | 0 |
| Modulfunktion | `bgm_volume` | 0 |
| Modulfunktion | `bgs_volume` | 0 |
| Modulfunktion | `se_volume` | 0 |
| Modulfunktion | `me_volume` | 0 |
| Modulfunktion | `play_music` | 1..5 |
| Modulfunktion | `play_sound` | 1..3 |

## `Battle`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `setup` | 1..3 |
| Modulfunktion | `in_battle?` | 0 |
| Modulfunktion | `needs_input?` | 0 |
| Modulfunktion | `input_actor_index` | 0 |
| Modulfunktion | `input_actor_id` | 0 |
| Modulfunktion | `can_escape?` | 0 |
| Modulfunktion | `turn` | 0 |
| Modulfunktion | `last_outcome` | 0 |
| Modulfunktion | `last_exp` | 0 |
| Modulfunktion | `last_gold` | 0 |
| Modulfunktion | `actors` | 0 |
| Modulfunktion | `enemies` | 0 |
| Modulfunktion | `set_action` | 1..5 |
| Modulfunktion | `abort` | 0 |
| Modulfunktion | `damage_enemy` | 2 |
| Modulfunktion | `damage_actor` | 2 |
| Modulfunktion | `heal_enemy` | 2..3 |
| Modulfunktion | `heal_actor` | 2..3 |
| Modulfunktion | `message` | 1 |

## `Bitmap`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `width` | 0 |
| Methode | `height` | 0 |
| Methode | `rect` | 0 |
| Methode | `blt` | beliebig |
| Methode | `stretch_blt` | beliebig |
| Methode | `fill_rect` | beliebig |
| Methode | `gradient_fill_rect` | beliebig |
| Methode | `clear` | 0 |
| Methode | `clear_rect` | beliebig |
| Methode | `get_pixel` | 2 |
| Methode | `set_pixel` | beliebig |
| Methode | `hue_change` | 1 |
| Methode | `blur` | 0 |
| Methode | `radial_blur` | 2 |
| Methode | `draw_text` | beliebig |
| Methode | `text_size` | 1 |
| Methode | `font` | 0 |
| Methode | `font=` | 1 |
| Methode | `clone` | 0 |
| Methode | `dup` | 0 |

## `Camera`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `position` | 0 |
| Modulfunktion | `set_position` | 3 |
| Modulfunktion | `rotation` | 0 |
| Modulfunktion | `set_rotation` | 3 |

## `Color`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `red` | 0 |
| Methode | `red=` | 1 |
| Methode | `green` | 0 |
| Methode | `green=` | 1 |
| Methode | `blue` | 0 |
| Methode | `blue=` | 1 |
| Methode | `alpha` | 0 |
| Methode | `alpha=` | 1 |
| Methode | `set` | beliebig |

## `Engine`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `time` | 0 |
| Modulfunktion | `delta_time` | 0 |
| Modulfunktion | `log` | 1 |

## `Entity`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `exists?` | 1 |
| Modulfunktion | `name` | 1 |
| Modulfunktion | `find_by_name` | 1 |
| Modulfunktion | `all_ids` | 0 |
| Modulfunktion | `position` | 1 |
| Modulfunktion | `set_position` | 4 |
| Modulfunktion | `rotation` | 1 |
| Modulfunktion | `set_rotation` | 4 |
| Modulfunktion | `scale` | 1 |
| Modulfunktion | `set_scale` | 4 |
| Modulfunktion | `move` | 4 |
| Modulfunktion | `start_clip` | 2..3 |
| Modulfunktion | `stop_clip` | 1 |
| Modulfunktion | `attach_behaviour` | 2 |
| Modulfunktion | `detach_behaviour` | 1 |

## `Font`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `name` | 0 |
| Methode | `name=` | 1 |
| Methode | `size` | 0 |
| Methode | `size=` | 1 |
| Methode | `bold` | 0 |
| Methode | `bold=` | 1 |
| Methode | `italic` | 0 |
| Methode | `italic=` | 1 |
| Methode | `color` | 0 |
| Methode | `color=` | 1 |
| Methode | `shadow` | 0 |
| Methode | `shadow=` | 1 |

## `FontDefaults`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `name` | 0 |
| Modulfunktion | `name=` | 1 |
| Modulfunktion | `size` | 0 |
| Modulfunktion | `size=` | 1 |
| Modulfunktion | `bold` | 0 |
| Modulfunktion | `bold=` | 1 |
| Modulfunktion | `italic` | 0 |
| Modulfunktion | `italic=` | 1 |
| Modulfunktion | `color` | 0 |
| Modulfunktion | `color=` | 1 |
| Modulfunktion | `shadow` | 0 |
| Modulfunktion | `shadow=` | 1 |

## `Game`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `new_game` | 0 |
| Modulfunktion | `start_game` | 0 |
| Modulfunktion | `show_message` | 1 |
| Modulfunktion | `show_screen_text` | 1..7 |
| Modulfunktion | `show_world_text` | 1..8 |
| Modulfunktion | `show_picture` | 1..6 |
| Modulfunktion | `move_picture` | 1..5 |
| Modulfunktion | `tween_picture` | 1..8 |
| Modulfunktion | `remove_picture` | 0..1 |
| Modulfunktion | `save` | 0..1 |
| Modulfunktion | `load` | 0..1 |
| Modulfunktion | `switch` | 1 |
| Modulfunktion | `set_switch` | 2 |
| Modulfunktion | `variable` | 1 |
| Modulfunktion | `set_variable` | 2 |
| Modulfunktion | `start_battle` | 0..1 |
| Modulfunktion | `in_battle?` | 0 |
| Modulfunktion | `map_visible` | 0 |
| Modulfunktion | `set_map_visible` | 1 |
| Modulfunktion | `map_id` | 0 |
| Modulfunktion | `setup_map` | 1 |

## `Game_Actor`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | 0..1 |
| Methode | `id` | 0 |
| Methode | `actor_id` | 0 |
| Methode | `exist?` | 0 |
| Methode | `name` | 0 |
| Methode | `name=` | 1 |
| Methode | `class_id` | 0 |
| Methode | `level` | 0 |
| Methode | `level=` | 1 |
| Methode | `exp` | 0 |
| Methode | `exp=` | 1 |
| Methode | `next_exp` | 0 |
| Methode | `add_exp` | 1 |
| Methode | `hp` | 0 |
| Methode | `hp=` | 1 |
| Methode | `sp` | 0 |
| Methode | `sp=` | 1 |
| Methode | `maxhp` | 0 |
| Methode | `maxsp` | 0 |
| Methode | `atk` | 0 |
| Methode | `def` | 0 |
| Methode | `agi` | 0 |
| Methode | `dead?` | 0 |
| Methode | `recover_all` | 0 |
| Methode | `states` | 0 |
| Methode | `add_state` | 1 |
| Methode | `remove_state` | 1 |
| Methode | `skills` | 0 |
| Methode | `learn_skill` | 1 |
| Methode | `forget_skill` | 1 |
| Methode | `weapon_id` | 0 |
| Methode | `armor1_id` | 0 |
| Methode | `armor2_id` | 0 |
| Methode | `armor3_id` | 0 |
| Methode | `armor4_id` | 0 |
| Methode | `character_name` | 0 |
| Methode | `face_index` | 0 |
| Methode | `change_equip` | 2 |
| Methode | `x` | 0 |
| Methode | `y` | 0 |
| Methode | `screen_x` | 0 |
| Methode | `screen_x=` | 1 |
| Methode | `screen_y` | 0 |
| Methode | `screen_y=` | 1 |

## `Game_Enemy`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | 0..1 |
| Methode | `__attach` | 1 |
| Methode | `id` | 0 |
| Methode | `enemy_id` | 0 |
| Methode | `index` | 0 |
| Methode | `exist?` | 0 |
| Methode | `name` | 0 |
| Methode | `battler_name` | 0 |
| Methode | `battler_hue` | 0 |
| Methode | `hp` | 0 |
| Methode | `hp=` | 1 |
| Methode | `sp` | 0 |
| Methode | `sp=` | 1 |
| Methode | `maxhp` | 0 |
| Methode | `maxsp` | 0 |
| Methode | `atk` | 0 |
| Methode | `def` | 0 |
| Methode | `agi` | 0 |
| Methode | `dead?` | 0 |
| Methode | `recover_all` | 0 |
| Methode | `exp` | 0 |
| Methode | `gold` | 0 |
| Methode | `transform` | 1 |
| Methode | `animation1_id` | 0 |
| Methode | `animation2_id` | 0 |
| Methode | `x` | 0 |
| Methode | `y` | 0 |
| Methode | `screen_x` | 0 |
| Methode | `screen_x=` | 1 |
| Methode | `screen_y` | 0 |
| Methode | `screen_y=` | 1 |

## `Game_Event`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | 0..2 |
| Methode | `map_id` | 0 |
| Methode | `id` | 0 |
| Methode | `valid?` | 0 |
| Methode | `name` | 0 |
| Methode | `x` | 0 |
| Methode | `y` | 0 |
| Methode | `direction` | 0 |
| Methode | `through` | 0 |
| Methode | `through=` | 1 |
| Methode | `transparent` | 0 |
| Methode | `transparent=` | 1 |
| Methode | `move_speed` | 0 |
| Methode | `move_speed=` | 1 |
| Methode | `moveto` | 2 |
| Methode | `erase` | 0 |
| Methode | `erased` | 0 |
| Methode | `erased?` | 0 |
| Methode | `refresh` | 0 |

## `Game_Map`

| Art | Name | Argumente |
|---|---|---|
| Methode | `visible?` | 0 |
| Methode | `visible=` | 1 |
| Methode | `id` | 0 |
| Methode | `map_id` | 0 |
| Methode | `setup` | 1 |
| Methode | `width` | 0 |
| Methode | `height` | 0 |
| Methode | `passable?` | 0 |
| Methode | `bush?` | 2 |
| Methode | `terrain_tag` | 2 |
| Methode | `data` | 0 |
| Methode | `display_x` | 0 |
| Methode | `display_x=` | 1 |
| Methode | `display_y` | 0 |
| Methode | `display_y=` | 1 |
| Methode | `events` | 0 |
| Methode | `refresh` | 0 |
| Methode | `need_refresh` | 0 |
| Methode | `need_refresh=` | 1 |

## `Game_Party`

| Art | Name | Argumente |
|---|---|---|
| Methode | `gold` | 0 |
| Methode | `gain_gold` | 1 |
| Methode | `lose_gold` | 1 |
| Methode | `item_count` | 1 |
| Methode | `gain_item` | 2 |
| Methode | `weapon_count` | 1 |
| Methode | `gain_weapon` | 2 |
| Methode | `armor_count` | 1 |
| Methode | `gain_armor` | 2 |
| Methode | `has_actor` | 1 |
| Methode | `add_actor` | 1 |
| Methode | `remove_actor` | 1 |
| Methode | `members_size` | 0 |
| Methode | `members` | 0 |
| Methode | `item_number` | 1 |
| Methode | `weapon_number` | 1 |
| Methode | `armor_number` | 1 |
| Methode | `has_item` | 1 |
| Methode | `all_dead?` | 0 |
| Methode | `__actor_ids` | 0 |
| Methode | `__item_ids` | 0 |
| Methode | `__weapon_ids` | 0 |
| Methode | `__armor_ids` | 0 |

## `Game_Player`

| Art | Name | Argumente |
|---|---|---|
| Methode | `x` | 0 |
| Methode | `y` | 0 |
| Methode | `z` | 0 |
| Methode | `move_to` | 3 |
| Methode | `locked?` | 0 |
| Methode | `locked=` | 1 |
| Methode | `moving?` | 0 |

## `Game_Screen`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | 0 |
| Methode | `start_flash` | 2 |
| Methode | `flash_color` | 0 |
| Methode | `start_tone_change` | 2 |
| Methode | `tone` | 0 |
| Methode | `start_shake` | 3 |
| Methode | `shake` | 0 |

## `Game_SelfSwitches`

| Art | Name | Argumente |
|---|---|---|
| Methode | `[]` | 1 |
| Methode | `[]=` | 2 |
| Methode | `size` | 0 |

## `Game_Switches`

| Art | Name | Argumente |
|---|---|---|
| Methode | `[]` | 1 |
| Methode | `[]=` | 2 |
| Methode | `size` | 0 |

## `Game_Troop`

| Art | Name | Argumente |
|---|---|---|
| Methode | `__enemy_ids` | 1 |

## `Game_Variables`

| Art | Name | Argumente |
|---|---|---|
| Methode | `[]` | 1 |
| Methode | `[]=` | 2 |
| Methode | `size` | 0 |

## `Graphics`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `update` | 0 |
| Modulfunktion | `freeze` | 0 |
| Modulfunktion | `transition` | beliebig |
| Modulfunktion | `frame_reset` | 0 |
| Modulfunktion | `wait` | 0..1 |
| Modulfunktion | `frame_rate` | 0 |
| Modulfunktion | `frame_rate=` | 1 |
| Modulfunktion | `frame_count` | 0 |
| Modulfunktion | `frame_count=` | 1 |
| Modulfunktion | `width` | 0 |
| Modulfunktion | `height` | 0 |
| Modulfunktion | `fullscreen` | 0 |
| Modulfunktion | `fullscreen=` | 1 |

## `Input`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `key_down?` | 1 |
| Modulfunktion | `key_down` | 1 |
| Modulfunktion | `key_pressed?` | 1 |
| Modulfunktion | `gamepad_connected?` | 0 |

## `Map`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `set_tile` | 4 |
| Modulfunktion | `get_tile` | 3 |
| Modulfunktion | `width` | 0 |
| Modulfunktion | `height` | 0 |

## `Physics`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `raycast` | 6..7 |

## `Plane`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `viewport` | 0 |
| Methode | `bitmap` | 0 |
| Methode | `bitmap=` | 1 |
| Methode | `visible` | 0 |
| Methode | `visible=` | 1 |
| Methode | `z` | 0 |
| Methode | `z=` | 1 |
| Methode | `ox` | 0 |
| Methode | `ox=` | 1 |
| Methode | `oy` | 0 |
| Methode | `oy=` | 1 |
| Methode | `zoom_x` | 0 |
| Methode | `zoom_x=` | 1 |
| Methode | `zoom_y` | 0 |
| Methode | `zoom_y=` | 1 |
| Methode | `opacity` | 0 |
| Methode | `opacity=` | 1 |
| Methode | `blend_type` | 0 |
| Methode | `blend_type=` | 1 |
| Methode | `color` | 0 |
| Methode | `color=` | 1 |
| Methode | `tone` | 0 |
| Methode | `tone=` | 1 |

## `RGSS`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `clear_windows` | 0 |

## `Rect`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `x` | 0 |
| Methode | `x=` | 1 |
| Methode | `y` | 0 |
| Methode | `y=` | 1 |
| Methode | `width` | 0 |
| Methode | `width=` | 1 |
| Methode | `height` | 0 |
| Methode | `height=` | 1 |
| Methode | `set` | beliebig |
| Methode | `empty` | 0 |

## `Rui`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `window` | 5 |
| Modulfunktion | `find` | 1 |
| Modulfunktion | `clear` | 0 |
| Modulfunktion | `set_focus_list` | 2 |
| Modulfunktion | `clear_focus` | 0 |
| Modulfunktion | `has_focus?` | 0 |
| Modulfunktion | `windowskin=` | 1 |
| Modulfunktion | `windowskin` | 0 |
| Modulfunktion | `theme_color` | 1 |
| Modulfunktion | `set_theme_color` | 4..5 |
| Modulfunktion | `theme_metric` | 1 |
| Modulfunktion | `set_theme_metric` | 2 |

## `Sprite`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `viewport` | 0 |
| Methode | `bitmap` | 0 |
| Methode | `bitmap=` | 1 |
| Methode | `src_rect` | 0 |
| Methode | `src_rect=` | 1 |
| Methode | `visible` | 0 |
| Methode | `visible=` | 1 |
| Methode | `x` | 0 |
| Methode | `x=` | 1 |
| Methode | `y` | 0 |
| Methode | `y=` | 1 |
| Methode | `z` | 0 |
| Methode | `z=` | 1 |
| Methode | `ox` | 0 |
| Methode | `ox=` | 1 |
| Methode | `oy` | 0 |
| Methode | `oy=` | 1 |
| Methode | `zoom_x` | 0 |
| Methode | `zoom_x=` | 1 |
| Methode | `zoom_y` | 0 |
| Methode | `zoom_y=` | 1 |
| Methode | `angle` | 0 |
| Methode | `angle=` | 1 |
| Methode | `mirror` | 0 |
| Methode | `mirror=` | 1 |
| Methode | `bush_depth` | 0 |
| Methode | `bush_depth=` | 1 |
| Methode | `wave_height` | 0 |
| Methode | `wave_height=` | 1 |
| Methode | `wave_amp` | 0 |
| Methode | `wave_amp=` | 1 |
| Methode | `wave_length` | 0 |
| Methode | `wave_length=` | 1 |
| Methode | `wave_speed` | 0 |
| Methode | `wave_speed=` | 1 |
| Methode | `opacity` | 0 |
| Methode | `opacity=` | 1 |
| Methode | `blend_type` | 0 |
| Methode | `blend_type=` | 1 |
| Methode | `color` | 0 |
| Methode | `color=` | 1 |
| Methode | `tone` | 0 |
| Methode | `tone=` | 1 |
| Methode | `flash` | beliebig |
| Methode | `update` | 0 |

## `Table`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `[]` | beliebig |
| Methode | `[]=` | beliebig |
| Methode | `xsize` | 0 |
| Methode | `ysize` | 0 |
| Methode | `zsize` | 0 |
| Methode | `resize` | beliebig |

## `Tilemap`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `viewport` | 0 |
| Methode | `tileset` | 0 |
| Methode | `tileset=` | 1 |
| Methode | `autotiles` | 0 |
| Methode | `map_data` | 0 |
| Methode | `map_data=` | 1 |
| Methode | `flash_data` | 0 |
| Methode | `flash_data=` | 1 |
| Methode | `priorities` | 0 |
| Methode | `priorities=` | 1 |
| Methode | `visible` | 0 |
| Methode | `visible=` | 1 |
| Methode | `ox` | 0 |
| Methode | `ox=` | 1 |
| Methode | `oy` | 0 |
| Methode | `oy=` | 1 |
| Methode | `update` | 0 |

## `TilemapAutotiles`

| Art | Name | Argumente |
|---|---|---|
| Methode | `[]` | 1 |
| Methode | `[]=` | beliebig |

## `Tone`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `red` | 0 |
| Methode | `red=` | 1 |
| Methode | `green` | 0 |
| Methode | `green=` | 1 |
| Methode | `blue` | 0 |
| Methode | `blue=` | 1 |
| Methode | `gray` | 0 |
| Methode | `gray=` | 1 |
| Methode | `set` | beliebig |

## `UI`

| Art | Name | Argumente |
|---|---|---|
| Modulfunktion | `show_message` | 1 |
| Modulfunktion | `show_text` | 1..7 |
| Modulfunktion | `show_screen_text` | 1..7 |
| Modulfunktion | `show_world_text` | 1..8 |
| Modulfunktion | `clear_texts` | 0 |
| Modulfunktion | `gold` | 0 |
| Modulfunktion | `add_gold` | 1 |
| Modulfunktion | `show_picture` | 1..6 |
| Modulfunktion | `move_picture` | 1..5 |
| Modulfunktion | `tween_picture` | 1..8 |
| Modulfunktion | `remove_picture` | 0..1 |
| Modulfunktion | `clear_pictures` | 0 |
| Modulfunktion | `hud_visible=` | 1 |
| Modulfunktion | `hud_visible?` | 0 |
| Modulfunktion | `open_menu` | 0 |
| Modulfunktion | `open_save_screen` | 0..1 |
| Modulfunktion | `open_load_screen` | 0 |
| Modulfunktion | `open_list_menu` | 2+ |
| Modulfunktion | `open_name_input` | 1+ |
| Modulfunktion | `native_title=` | 1 |
| Modulfunktion | `native_title?` | 0 |
| Modulfunktion | `native_hud=` | 1 |
| Modulfunktion | `native_hud?` | 0 |
| Modulfunktion | `native_game_menu=` | 1 |
| Modulfunktion | `native_game_menu?` | 0 |
| Modulfunktion | `native_battle_menu=` | 1 |
| Modulfunktion | `native_battle_menu?` | 0 |
| Modulfunktion | `native_battle_status=` | 1 |
| Modulfunktion | `native_battle_status?` | 0 |
| Modulfunktion | `native_message=` | 1 |
| Modulfunktion | `native_message?` | 0 |
| Modulfunktion | `deliver_message_done` | 0 |
| Modulfunktion | `deliver_choice` | 1 |
| Modulfunktion | `deliver_number` | 1 |
| Modulfunktion | `deliver_name` | 1 |
| Modulfunktion | `script_dialog_active?` | 0 |
| Modulfunktion | `xp_scene_mode=` | 1 |
| Modulfunktion | `xp_scene_mode?` | 0 |

## `Viewport`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | beliebig |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `rect` | 0 |
| Methode | `rect=` | 1 |
| Methode | `visible` | 0 |
| Methode | `visible=` | 1 |
| Methode | `z` | 0 |
| Methode | `z=` | 1 |
| Methode | `ox` | 0 |
| Methode | `ox=` | 1 |
| Methode | `oy` | 0 |
| Methode | `oy=` | 1 |
| Methode | `color` | 0 |
| Methode | `color=` | 1 |
| Methode | `tone` | 0 |
| Methode | `tone=` | 1 |
| Methode | `flash` | beliebig |
| Methode | `update` | 0 |

## `Window`

| Art | Name | Argumente |
|---|---|---|
| Methode | `initialize` | 0..4 |
| Methode | `x` | 0 |
| Methode | `x=` | 1 |
| Methode | `y` | 0 |
| Methode | `y=` | 1 |
| Methode | `z` | 0 |
| Methode | `z=` | 1 |
| Methode | `width` | 0 |
| Methode | `width=` | 1 |
| Methode | `height` | 0 |
| Methode | `height=` | 1 |
| Methode | `openness` | 0 |
| Methode | `openness=` | 1 |
| Methode | `visible` | 0 |
| Methode | `visible=` | 1 |
| Methode | `windowskin` | 0 |
| Methode | `windowskin=` | 1 |
| Methode | `text` | 0 |
| Methode | `text=` | 1 |
| Methode | `text_color=` | 1 |
| Methode | `dispose` | 0 |
| Methode | `disposed?` | 0 |
| Methode | `update` | 0 |
| Methode | `viewport` | 0 |
| Methode | `contents` | 0 |
| Methode | `contents=` | 1 |
| Methode | `cursor_rect` | 0 |
| Methode | `cursor_rect=` | 1 |
| Methode | `active` | 0 |
| Methode | `active=` | 1 |
| Methode | `pause` | 0 |
| Methode | `pause=` | 1 |
| Methode | `stretch` | 0 |
| Methode | `stretch=` | 1 |
| Methode | `opacity` | 0 |
| Methode | `opacity=` | 1 |
| Methode | `back_opacity` | 0 |
| Methode | `back_opacity=` | 1 |
| Methode | `contents_opacity` | 0 |
| Methode | `contents_opacity=` | 1 |
| Methode | `ox` | 0 |
| Methode | `ox=` | 1 |
| Methode | `oy` | 0 |
| Methode | `oy=` | 1 |
