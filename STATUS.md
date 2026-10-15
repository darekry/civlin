# CivLin — stan implementacji (per-function)

Centralny wykaz: jaka funkcja jest LIVE / STUB (wcześniej status był tylko
w komentarzach nagłówków `src/*.h`). Regeneracja: `python3 tools/gen_status.py
> STATUS.md` (wynik skanuje `src/*.c`; rzeźba ręczna tylko w `OVERRIDES` i w
sekcji „Ścieżka krytyczna" poniżej).

## Legenda

- LIVE — robi prawdziwą robotę (port 1:1 logiki OS-free, implementacja SDL3,
  walidator, cienki delegat do właściciela).
- PARTIAL — robi część roboty, ogon wciąż `CIV_ERR_/RSC_ERR_UNSUPPORTED`
  (np. dispatcher, którego kroki są stubami).
- STUB — znana ścieżka, nie zaimplementowana (zwraca UNSUPPORTED).
- NOOP — celowo puste (void shutdowny, `cursor_set`, placeholdery).
- Kolumna „Orig addr" zbiera adresy `seg:off` wspomniane w komentarzu nad
  funkcją, w jej ciele oraz w nagłówku `src/*.h` (mapa RE żyje w nagłówkach)
  — to wskazówki RE, nie approved mapping.
- „has TODO" = w ciele jest znacznik TODO z adresem Orig i opisem braków.

## Ścieżka krytyczna WinMain @ 1008:0000 (stan na 2026-10-15)

`main.c` leci best-effort: loguje `boot_run`, gra SDL-owe intro
(`title_play_intro_sdl`: MPLOGO.BMP + OPENING.wav, skip), wychodzi z 0.
Prawdziwy `boot_run` pada na pierwszym stubie (`boot_check_memory`).

| Krok WinMain | Funkcja | Stan | Komentarz |
|---|---|---|---|
| Sys_CheckFreeMemory | `sys_check_free_memory` | STUB | mapowanie na SDL_GetSystemRAM — trywialne |
| paths (drive/cwd) | `sys_init_paths` | LIVE | |
| App_RegisterWindowClasses | `app_register_window_classes` | STUB | na SDL brak klas okien — no-op CIV_OK |
| civfonts.fon | `font_enum_families_init` | STUB | FON pakowany EDI; wiernie = unpack + NE-fonty |
| ResMgr_LoadRsc x5 | `rsc_load_rsc` | LIVE | BE-walk katalogu EOF (2 passy, bounds-checked, `RSC_OK`); wpięcie 5 plików w `boot_load_resources` — STUB |
| Video_CheckDriverCaps | `video_check_driver_caps` | LIVE | |
| Art_MaybeConvertArtwork | `art_maybe_convert_artwork` | STUB | na Linuksie zbędne — no-op CIV_OK |
| Config_LoadPrefs | `config_load_prefs` | STUB | wystarczą domyślne (sounds on) — CIV_OK |
| Palette_Init | `palette_init` | LIVE | no-op |
| Game_InitGlobalTables | `boot_init_tables` | PARTIAL | rng + próba malloc żyją; win16-lock nie ma odpowiednika |
| Main_RunStartupSequence | `main_run_startup_sequence` | STUB | wymaga silnika Anim_* @ 10b0 + dialogu 0x94 + MenuZ |
| Init_UnitFontTables | `boot_init_unit_font_tables` | STUB | potrzebne dopiero wizardowi |
| Sound_PlayId(STOP) | `audio_play_id` | LIVE | |
| MainPort/GfxPort | `mainport_get_or_create`, `gfx_port_create` | STUB | mostek na SDL_Texture — średnia praca |
| WdwMap/SmMap/Status_Open | `map_open_*` | STUB | `map_init` → no-op wystarczy na razie |
| Menu_InitMainBar | `menu_init_mainbar` | STUB | no-op CIV_OK |
| pętla Game_TurnLoopTick | `game_turn_tick` | STUB | gra właściwa — poza zakresem |
| WinMain_Shutdown | `boot_shutdown` | LIVE | no-op shutdowny |

Ekran main menu = `dialog_menuz` (STUB) + `dlg_create_tracked` (STUB) nad
`CIV_MAINMENU_TEXT` (tekst już w `mainmenu.h`); wykonalne bez RSC
(proceduralne widgety + debug-text), wierny wygląd czeka na wpięcie 5x RSC
w `boot_load_resources` (STUB) + dekod ramek `rsc_get_indexed` (STUB).

## Korekty mapy RE (do przeniesienia do `src/*.h`)

- `FUN_1030_0000` to NIE `Title_PlayAnimLoop`: ~31 KB, 16 wołających
  (miasta, `Unit_DoMove`, `City_ShowTopFive`, HoF…) — współdzielony
  odtwarzacz PIC/animacji. `title.h` do poprawki.
- `boot_check_video` nie deleguje do żywego `video_check_driver_caps`
  (sygnatura bez renderera) — do posprzątania przy domknięciu `boot_run`.

## Pełna tabela (skan `src/*.c`)


### `src/audio.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `audio_stop_stream` | LIVE |  |  |
| `sound_mci_open` | LIVE | 1000:1184, 1150:001c, 1150:0066, 1150:00a0 |  |
| `sound_mci_close` | LIVE | 1150:0066 |  |
| `sound_play_file` | LIVE | 1000:1184, 1150:00a0 |  |
| `sound_mci_play` | LIVE | 1150:001c, 1150:0066 |  |
| `audio_init` | LIVE | 1150:001c |  |
| `audio_play` | LIVE | 1150:00a0 |  |
| `sound_normalize_id` | LIVE | 1150:014f |  |
| `audio_gap_is_silent` | LIVE | 1150:00c9, 1150:019e, 1150:025b, 1150:0266, 1420:3080 |  |
| `audio_leader_count` | LIVE | 1150:019e, 1150:0266, 1420:3080 |  |
| `audio_play_id` | LIVE | 1000:1184, 1150:00c9, 12d0:0064 |  |
| `audio_is_playing` | LIVE | 1030:0000 |  |
| `audio_shutdown` | LIVE | 1150:0066 |  |

### `src/boot.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `boot_setup_paths` | LIVE | 1000:11bb, 1000:2826 |  |
| `boot_check_memory` | LIVE | 11e8:0000, 1420:43b8 |  |
| `app_register_window_classes` | STUB | 1058:0b4d, 1098:06f8 |  |
| `boot_register_classes` | LIVE | 1098:06f8 |  |
| `boot_load_fonts` | LIVE | 1008:0e99 |  |
| `boot_load_resources` | STUB | 1190:0000 | has TODO |
| `boot_check_video` | STUB | 1008:059a | has TODO |
| `boot_maybe_convert_artwork` | LIVE | 10b8:0000, 10b8:079d, 1420:20a9 |  |
| `boot_load_prefs` | LIVE | 1008:0b4a |  |
| `boot_init_palette` | LIVE | 1090:0488 |  |
| `boot_init_tables` | PARTIAL | 1090:0488, 11c8:0000, 11e0:0000, 11e8:0337, 11f8:0080, 1420:b394 | has TODO |
| `boot_init_unit_font_tables` | STUB | 10e8:0000, 10e8:2d46 |  |
| `cursor_set` | NOOP | 1048:0000, 1098:096c |  |
| `boot_open_windows` | PARTIAL | 1098:114a, 10d0:0000, 1208:0af1, 1210:025b, 1218:0386 | has TODO |
| `menu_init_mainbar` | STUB | 1048:0000, 1098:096c, 14b0:0010, 14b0:0092, 14d0:0028 | has TODO |
| `boot_init_menu` | PARTIAL | 1048:0000, 1098:096c |  |
| `dos_drive_ready` | LIVE | 1000:1184, 1008:0bbf, 1150:00c9 |  |
| `dos_set_path` | LIVE | 1000:116a |  |
| `font_enum_families_init` | STUB | 1008:0e99, 1420:00a4 |  |
| `font_fon_u16le` | LIVE |  |  |
| `font_fon_u32le` | LIVE |  |  |
| `font_fon_parse` | LIVE | 1008:0e99, 1420:00a4 |  |
| `font_face_count` | LIVE | 1008:0e99 |  |
| `font_fnt_parse` | LIVE | 1008:0e99 |  |
| `font_face_bitmap` | PARTIAL | 1008:0e99 | has TODO |
| `font_fnt_char_entry` | PARTIAL | 1008:0e99 | has TODO |
| `font_fnt_bits` | LIVE | 1008:0e99 |  |
| `font_render_glyph` | LIVE | 1008:0e99 |  |
| `font_text_width` | LIVE | 1098:0dda, 11f8:0278 |  |
| `font_text_height` | LIVE | 1098:0dda |  |
| `menu_strings_count` | LIVE | 1048:0000, 1420:435b, 14b0:0010, 14b0:0092, 14d0:0028 |  |
| `config_load_prefs` | STUB | 1008:0b4a | has TODO |
| `config_save_prefs` | STUB | 1000:116a, 1000:1184, 1008:0bbf | has TODO |
| `boot_run` | PARTIAL | 1000:11bb, 1000:2826, 1008:0000, 1008:059a, 1008:0b4a, 1008:0d68, 1008:0e99, 1088:0000, 1090:0488, 1098:06f8, 1098:096c, 10b8:0000, 10e8:2d46, 1190:0000, 11e8:0000, 11e8:0337 |  |
| `boot_shutdown` | LIVE | 1008:0000, 1008:0329, 1008:0d68, 1208:0af1 |  |
| `art_maybe_convert_artwork` | STUB | 10b8:0000, 10b8:079d, 1420:20a9 |  |
| `main_run_startup_sequence` | STUB | 1008:0329, 10e8:0000, 1108:0243, 1130:000e, 11e0:0000, 11e0:0221, 1420:435b |  |
| `config_prefs_defaults` | LIVE | 1008:0b4a, 1008:0bbf, 1150:00c9, 12d0:0064 |  |
| `config_prefs_sounds_on` | LIVE |  |  |
| `boot_video_caps_count` | LIVE | 1008:059a, 1420:0af4 |  |

### `src/civ_common.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `util_to_lower` | LIVE | 1000:1978, 1190:0e72 |  |
| `util_strip_trailing_zeros` | LIVE | 0221:03aa, 1000:01fe, 1000:0221 |  |
| `util_format_float` | STUB | 1000:01fe, 1000:0221, 1000:0754, 1420:4ad6, 1420:50cb | has TODO |
| `util_format_message` | LIVE | 1000:2928, 1000:2c9c, 1000:36b2, 1088:1dd0, 1190:17d0 |  |

### `src/dialog.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `dialog_init` | STUB | 1058:0000, 1058:0440, 1058:049b, 1058:04f6, 1058:0b4d, 1058:0d71, 1058:0e7f, 1098:0157, 1098:124f, 1098:1612, 1098:16e2, 1098:1838, 10b0:0000, 10b0:0461, 1220:0156, 1420:1ea0, 1420:1ec8, 1420:1f40, 1420:4a41, 14b0:0010, 14b0:0092, 14d0:0028 |  |
| `dlg_proc` | LIVE | 1098:124f, 1098:16e2, 1420:1ea0 |  |
| `civ_dialog_proc` | LIVE | 1098:124f, 1098:1838 |  |
| `dialog_shutdown` | NOOP |  |  |
| `dialog_menuz` | STUB | 1220:0156 | has TODO |
| `dialog_ctrl_register` | STUB | 1058:0000, 1058:0440, 1058:04f6, 1058:0b4d, 1058:0d71, 1058:0e7f | has TODO |
| `dialog_ctrl_free` | NOOP | 1058:0d71 |  |
| `dialog_ctrl_radio_set` | STUB | 1058:0e7f | has TODO |
| `dialog_ctrl_paint_button` | LIVE | 1058:0000 |  |
| `dialog_ctrl_paint_static` | LIVE | 1058:04f6 |  |
| `dialog_ctrl_paint_user` | LIVE | 1058:0440 |  |
| `dialog_ctrl_radio_proc` | LIVE | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6 |  |
| `dialog_ctrl_static_proc` | LIVE | 1058:081a |  |
| `dialog_ctrl_user_proc` | LIVE | 1058:0921 |  |
| `dialog_ctrl_region_proc` | LIVE | 1058:09f6 |  |
| `dlg_create_tracked` | STUB | 1098:124f, 1098:1612, 1420:1ea0, 1420:1ec8 | has TODO |
| `dlg_destroy_tracked` | NOOP | 1098:1612, 1420:1ea0 |  |
| `anim_pump_if_active` | STUB | 1098:0157, 10b0:0000, 10b0:0461, 1150:00c9, 1420:1f40 | has TODO |
| `anim_advance_frames` | STUB | 10b0:0461, 1150:00c9 | has TODO |
| `dlg_center_window` | PARTIAL | 1218:0956 | has TODO |
| `dialog_open_help` | STUB |  | has TODO |
| `help_hlp_parse` | LIVE |  |  |
| `help_hlp_system` | LIVE |  |  |
| `help_hlp_topic` | LIVE |  |  |
| `help_hlp_page` | LIVE |  |  |
| `help_hlp_btree_find` | LIVE |  |  |
| `help_hlp_btree_root` | LIVE |  |  |
| `help_hlp_btree_next` | LIVE |  |  |
| `help_topic_list` | LIVE |  |  |
| `help_hlp_topic_header` | LIVE |  |  |
| `help_hlp_topic_text` | PARTIAL |  | has TODO |
| `help_topic_get` | LIVE |  |  |
| `help_hlp_lzss_try` | LIVE | 10f8:040a |  |
| `help_hlp_decompressed_size` | LIVE |  |  |
| `help_hlp_decompress` | PARTIAL | 10f8:040a | has TODO |
| `help_topic_get_full` | PARTIAL |  | has TODO |
| `help_open_topic` | LIVE |  |  |
| `diplomacy_post_contact` | STUB | 1168:0a07, 11c0:0395, 1420:62fc | has TODO |
| `diplomacy_eval_bribe` | STUB | 1078:1480, 1078:455c, 11c0:0395 | has TODO |
| `ui_draw_end_turn_prompt` | STUB | 10d0:0854, 10d8:00e0, 1110:0448, 11c0:0ec2, 1210:07ae | has TODO |
| `dialog_menuz_id_to_index` | LIVE | 1220:0156 |  |
| `dialog_menuz_index_to_id` | LIVE | 1220:0156 |  |

### `src/game.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `game_init` | LIVE | 1008:0000, 12d8:24ee, 12d8:3b9a, 12d8:42f6, 12d8:42f8, 12d8:42fa, 12d8:42fe, 12d8:4300, 12d8:44e2 |  |
| `game_new` | STUB | 1028:0000, 10e8:0000 | has TODO |
| `game_tick` | PARTIAL | 1008:0000, 1008:0329, 1008:0d68, 1088:0000, 1098:0157, 1108:0000, 1208:0af1 | has TODO |
| `game_turn_tick` | STUB | 1088:0000, 1088:1d99, 11f0:0000 | has TODO |
| `game_process_game_end` | STUB | 1040:1137, 1088:0b6d, 1180:0000, 1188:07ad | has TODO |
| `game_spawn_uprisings` | STUB | 1088:0be9, 1110:16c0, 1110:18ce, 1110:1dbd, 11c0:12c8, 11c8:003d | has TODO |
| `game_uprising_rng_scale` | LIVE | 1088:0be9, 11c8:003d |  |
| `city_process_disaster` | STUB | 1030:0000, 1040:068e, 1088:0000, 1110:03ae, 11c8:003d, 1220:00d2, 1420:07f0, 1420:3efe, 1420:54fc, 1420:5500, 1420:5502, 1420:5503 | has TODO |
| `game_calc_power_score` | LIVE | 1088:108a |  |
| `game_update_power_rankings` | LIVE | 1088:108a, 12a8:000c |  |
| `game_update_status_panel` | STUB | 1088:13d5, 1088:1a82, 11f8:0278, 1218:0956, 1220:00d2 | has TODO |
| `game_accum_pollution` | LIVE | 1028:1afd, 1088:1e32, 12d8:42f0 |  |
| `game_do_pollution_and_wonder_race` | STUB | 1028:1afd, 1088:1e32, 1170:3181, 12d8:42f0 | has TODO |
| `game_pick_wonder_for_ai` | LIVE | 1088:1e32, 1170:3181 |  |
| `civ_process_unit_moves` | LIVE | 1078:1480, 1128:0000, 11c0:0395, 11c0:0ec2, 11c0:16a4, 11f0:0000, 11f0:44d4, 1420:62fc |  |
| `status_draw_tech_icons` | STUB | 1088:1a82, 11a8:0cb1 | has TODO |
| `game_append_year_text` | STUB | 1018:015d, 1088:1dd0, 1138:0000, 11b0:0000, 11b8:14e5, 12d8:24fa, 1420:173c, 1420:1740 | has TODO |
| `gfx_draw_tech_icon` | STUB | 1088:1a82, 11a8:0cb1 | has TODO |
| `status_font_height` | LIVE | 1088:1460, 1098:0dda, 1138:0269, 11f0:4754, 1220:0b88 |  |
| `unit_choose_order` | LIVE | 1078:1480, 11c0:16a4, 11c0:1bc3, 11c8:02ec, 1420:5500 |  |
| `unit_step_toward_goal` | LIVE | 1078:55d3, 1110:16c0, 1110:18ce, 1110:1c4b, 1110:1dbd, 1128:0000, 11c0:23fd, 11f0:0ca0, 1338:0002, 1420:17d4 |  |
| `unit_update_visibility` | STUB | 1078:455c, 1110:180e, 1168:0a07, 11c0:0000, 11c0:0395, 11c0:1ee1, 11c8:00c4, 11f0:0259 | has TODO |
| `unit_wait_for_orders` | STUB | 10d0:0854, 10d8:00e0, 1110:0448, 11c0:0ec2, 11e8:02fb, 11f0:0910, 1210:07ae, 1328:0012 | has TODO |
| `unit_maybe_center_view` | STUB | 1110:1461, 11f0:030c, 11f0:44d4 | has TODO |
| `unit_ai_player_turn` | LIVE | 1078:1480, 1078:4f0f, 1078:55d3, 1128:0000 |  |
| `unit_reveal_area` | STUB | 11c0:0395, 11c0:12c8 | has TODO |
| `sys_drain_input` | STUB | 11c0:0395, 11f0:06d2, 11f0:4ea0, 12b0:004c, 1420:1d44 | has TODO |
| `game_calc_progress_threshold` | LIVE | 1088:1d99 |  |
| `unit_show_info_popup` | STUB | 1098:0dda, 11f0:468c, 11f0:4754, 1218:0801, 1220:0156, 1420:62fc | has TODO |
| `combat_resolve_attack` | STUB | 1078:1480, 11c0:1bc3 | has TODO |
| `combat_calc_odds` | LIVE | 1078:1480, 11c0:1bc3 |  |
| `unit_found_city_if_home` | STUB | 11c0:16a4, 1420:5500, 1420:62fc | has TODO |
| `map_check_zoc` | LIVE | 1110:16c0, 1110:18ce, 1110:1dbd, 1128:0000, 11c0:23fd |  |
| `city_spot_reveal` | STUB | 1110:180e, 11c0:0000, 11c0:0395, 11c8:00c4, 1420:5500 | has TODO |
| `city_update_visibility` | STUB | 11c0:0000, 11c8:00c4 | has TODO |
| `unit_find_diplomat_target` | LIVE | 1078:1480, 1128:0000, 11c8:02ec |  |
| `unit_do_found_city` | PARTIAL | 11c0:16a4, 11e8:0dcf, 11f0:0000, 1420:435b, 1420:43b8, 1420:5500, 1420:62fc | has TODO |
| `unit_do_fortify` | LIVE | 11f0:0000, 1420:435b, 1420:43b8, 1420:62fc |  |
| `unit_do_sentry` | LIVE | 11c0:0395, 11f0:0000 |  |
| `unit_do_move_attack` | PARTIAL | 1128:0000, 11c0:0ec2, 11c0:1bc3, 11c8:003d, 11f0:0000, 1420:17d4, 1420:435b, 1420:43b8, 1420:62fc | has TODO |
| `unit_order_is_idle` | LIVE | 11f0:0000 |  |
| `unit_order_is_sleep` | LIVE | 11f0:0000 |  |
| `unit_order_is_fail` | LIVE | 11f0:0000 |  |
| `unit_order_is_no_path` | LIVE | 1128:0000, 11f0:0000 |  |
| `unit_notify_wonder` | STUB | 1088:1e32, 1170:3181 | has TODO |
| `unit_type_count` | LIVE |  |  |
| `city_get_field` | LIVE |  |  |
| `city_get_x` | LIVE |  |  |
| `city_get_y` | LIVE |  |  |
| `city_get_valid` | LIVE |  |  |
| `city_get_size` | LIVE |  |  |
| `city_get_owner` | LIVE |  |  |
| `city_grow` | LIVE | 1420:5500 |  |
| `unit_move_cost` | LIVE | 1128:0000, 1420:17d4 |  |
| `unit_can_enter` | LIVE | 1110:16c0, 1110:18ce, 1110:199b, 1110:1dbd, 1128:0000, 11c0:23fd |  |
| `unit_dir_to_dxdy` | LIVE | 1110:1c4b, 1128:0000 |  |
| `unit_home` | LIVE | 1420:62fc |  |
| `unit_moves_left` | LIVE |  |  |
| `unit_seen_by` | LIVE |  |  |
| `city_get_prod` | LIVE | 1420:5500 |  |
| `unit_move_attack_step` | LIVE | 1110:1c4b, 1110:1dbd, 1128:0000, 1128:081c, 1420:17d4 |  |
| `unit_attack_resolve` | LIVE | 1078:1480, 11c0:1bc3, 11c8:003d |  |
| `terrain_cost_table` | LIVE | 1420:17d4 |  |
| `game_shutdown` | LIVE |  |  |

### `src/gfx_indexed.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `gfx_probe_rsc_image` | LIVE | 1190:0a29 |  |
| `gfx_probe_head_hex` | LIVE | 1000:2928, 1190:0a29 |  |
| `gfx_decode_rle` | LIVE | 10f8:040a, 10f8:05da |  |
| `gfx_rle_expand_cvpc` | LIVE | 10f8:040a, 10f8:05da |  |
| `gfx_decode_raw` | LIVE |  |  |
| `gfx_godpal_count` | LIVE |  |  |
| `gfx_godpal_get` | STUB | 1090:0376, 1090:0488, 1200:00a8, 1420:9966 | has TODO |
| `gfx_try_rll` | LIVE | 10f8:040a, 10f8:05da |  |
| `gfx_try_rll_inv` | LIVE | 10f8:040a |  |
| `gfx_try_lzss_edi` | LIVE |  |  |
| `gfx_try_lzss_1k` | LIVE |  |  |
| `gfx_try_lzss_2k` | LIVE |  |  |
| `gfx_try_bits_msb` | LIVE | 1420:2300, 1420:2372, 1420:23ce |  |
| `gfx_try_bits_bias2` | LIVE | 1420:2300, 1420:2372, 1420:23ce |  |
| `gfx_try_bits_bias4` | LIVE | 1420:2300, 1420:2372, 1420:23ce |  |
| `gfx_try_raw_shifted` | LIVE |  |  |
| `gfx_lzw_init` | LIVE | 1420:2300, 1420:2372, 1420:23ce |  |
| `gfx_lzw_decode` | LIVE | 1420:2372, 1420:23ce |  |
| `gfx_gif_parse` | PARTIAL | 1420:2300, 1420:2372 |  |
| `gfx_try_gif` | LIVE | 1420:2372 |  |
| `gfx_try_pk` | LIVE |  |  |
| `gfx_try_huff_stub` | LIVE |  |  |
| `gfx_godpal_search` | LIVE | 1090:0488, 1420:9966 |  |
| `gfx_rle_expand_best` | LIVE |  |  |
| `gfx_rle_expand_best_ex` | LIVE |  |  |
| `gfx_bmp_parse` | PARTIAL |  |  |
| `gfx_bmp_to_indexed` | PARTIAL |  |  |
| `gfx_bmp_load_indexed` | LIVE |  |  |
| `gfx_try_custom_stub` | LIVE |  |  |

### `src/halloffame.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `hof_weighted_score` | LIVE |  |  |
| `HoF_Clear` | LIVE | 1080:0000 |  |
| `HoF_Load` | STUB | 1080:0023, 1420:14e8 | has TODO |
| `HoF_Save` | STUB | 1080:0092, 1420:14f5 | has TODO |
| `HoF_Show` | STUB | 1080:0119, 1420:1502, 1420:153e, 1420:1572 | has TODO |
| `HoF_TryInsert` | LIVE | 1080:0595 |  |
| `HoF_VictoryCeremony` | STUB | 1080:06ff, 10d0:0854, 1150:019e | has TODO |
| `City_ShowTopFive` | STUB | 1080:099b, 1420:159c, 1420:5500 | has TODO |
| `Hof_CalcInterimScore` | LIVE | 1030:0000, 1040:1137, 1080:0595, 1088:108a |  |
| `Hof_CalcFinalScore` | STUB | 1040:1137, 1088:0b6d | has TODO |
| `Hof_PlayEndAnimation` | STUB | 1088:0b6d, 1188:07ad | has TODO |
| `Hof_ShowEndDialog` | STUB | 1088:0b6d, 1180:0000 | has TODO |
| `hof_load` | LIVE | 1080:0023 |  |
| `hof_save` | LIVE | 1080:0092 |  |
| `hof_show` | LIVE | 1080:0119 |  |
| `hof_try_insert` | LIVE | 1080:0595 |  |
| `hof_victory_ceremony` | LIVE | 1080:06ff |  |
| `city_show_top_five` | LIVE | 1080:099b, 1420:159c |  |

### `src/main.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `main` | LIVE |  |  |

### `src/mainmenu.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `mainmenu_run` | STUB | 1000:36b2, 11e0:0221, 1220:0156, 1420:435b |  |
| `mainmenu_dispatch` | PARTIAL | 1108:0243, 1130:000e | has TODO |

### `src/map.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `map_buffer_len` | LIVE |  |  |
| `map_in_bounds` | LIVE | 11e8:0dcf, 11e8:0e19 |  |
| `map_get_tile` | LIVE | 11e8:0dcf |  |
| `map_set_tile` | LIVE | 11e8:0e19 |  |
| `map_fill_rect` | LIVE | 1028:1d15 |  |
| `map_draw_rect_border` | LIVE | 1028:1ca7 |  |
| `map_try_place_isolated_tile` | LIVE | 1028:05e7 |  |
| `map_init` | STUB |  |  |
| `map_wndproc_main` | LIVE | 1008:0329, 1008:0d68, 1098:06f8, 10d0:0854, 1110:03e9, 1208:0054, 1208:0af1 |  |
| `map_wndproc_small` | LIVE | 1110:03ae, 1110:03e9, 1210:0000, 1210:025b, 1210:09b4 |  |
| `map_wndproc_status` | LIVE | 1088:13d5, 11f8:0278, 1218:0000, 1218:0386, 1218:0801, 1218:0956 |  |
| `map_timer_proc` | STUB | 1008:0000, 1008:0329, 1008:0d68, 1088:0000, 1208:0af1 | has TODO |
| `map_shutdown` | NOOP |  |  |
| `wdwstatus_calc_upper_rect` | STUB | 1088:1479, 1218:0956 | has TODO |
| `wdwstatus_calc_lower_rect` | STUB | 1088:1536, 1218:0a06 | has TODO |
| `path_find_corridor` | LIVE | 1128:0000, 1128:10de, 1420:62fc |  |
| `path_is_passable` | LIVE | 1110:16c0, 1110:18ce, 1128:0000, 1128:081c, 11c0:23fd |  |
| `map_is_sea_land` | LIVE | 1110:16c0, 1128:0000 |  |
| `map_is_occupied` | LIVE | 1110:18ce, 1128:0000, 1420:62fc |  |
| `map_is_city_blocked` | LIVE | 1110:18ce, 1110:199b, 1128:0000, 1420:5500 |  |
| `map_ocean_clear_depth` | LIVE | 1110:1dbd, 1128:0000 |  |
| `map_resolve_direct` | LIVE | 1110:1c4b, 1128:0000 |  |
| `map_reveal_center` | STUB | 1110:1461, 11f0:44d4 |  |
| `map_spot_city` | STUB | 1110:16ee, 1110:180e, 11c0:0000, 11c0:0395, 11c8:00c4, 1420:5500 |  |
| `text_draw_status_row` | STUB | 1088:1479, 1088:1536, 11f8:0278, 1218:0956 | has TODO |
| `text_draw_with_font` | LIVE | 1008:0e99, 11f8:0278, 1218:0956 |  |
| `status_format_main_row` | STUB | 1088:1dd0, 1138:0000, 11f8:0278, 12d8:24fa | has TODO |
| `status_format_second_row` | STUB | 11b0:0000, 11f8:0278, 12d8:24fa | has TODO |
| `status_format_gold_row` | STUB | 11b8:14e5, 11f8:0278, 12d8:24fa | has TODO |
| `map_open_main` | STUB | 1208:0af1 | has TODO |
| `map_open_small` | STUB | 1210:025b | has TODO |
| `map_open_status` | STUB | 1088:13d5, 1218:0386, 1218:0801 | has TODO |
| `map_refresh_status` | STUB | 1088:13d5, 10d0:0854, 11f0:468c, 1218:0801 |  |
| `map_center_on_tile` | STUB | 1110:03ae, 1110:03e9 |  |
| `map_refresh_all_views` | STUB | 1088:13d5, 1110:03e9, 1208:17da, 1210:09b4, 1408:00cc |  |
| `mainport_apply_palette` | STUB | 1090:0376, 1200:00a8 |  |
| `tile_move_cost` | LIVE | 1128:0000, 1420:17d4 |  |

### `src/newgame.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `newgame_pick_difficulty` | STUB | 10e8:0000, 1220:0156, 12d8:42f6, 1420:2a5a | has TODO |
| `newgame_pick_competition` | STUB | 10e8:0000, 1220:0156, 12d8:3b9a, 12d8:42f8, 1420:2aa9, 1420:2ac1, 1420:2ac3 | has TODO |
| `newgame_civ_count_from_index` | LIVE | 12d8:3b9a |  |
| `newgame_alive_mask` | LIVE | 12d8:42f8 |  |
| `newgame_pick_tribe` | STUB | 10e8:0000, 11f8:0080, 1220:0156, 12d8:4300, 1420:2ad2, 1420:2ae5 | has TODO |
| `newgame_customize_world` | STUB | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6, 10e8:0000, 1220:0156, 1220:16fd | has TODO |
| `newgame_custom_world_modal` | STUB | 1058:06ab, 1220:0156, 1220:16fd | has TODO |
| `newgame_finalize` | PARTIAL | 1028:0000, 10e8:0000, 1150:00c9, 12d8:3b9a, 12d8:42f6, 12d8:42f8, 12d8:42fa, 12d8:4300 | has TODO |
| `newgame_wizard_run` | PARTIAL | 10e8:0000, 11e0:0000 |  |

### `src/rsc_loader.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `is_cvd_tag` | LIVE |  |  |
| `is_rsed_tag` | LIVE |  |  |
| `rd_u32le` | LIVE |  |  |
| `rsc_read_u16le` | LIVE | 1190:0e72, 1190:1533, 1190:1953, 1190:19c9, 1190:1a3f, 1420:3d00 |  |
| `rsc_read_u32le` | LIVE | 1190:1a3f |  |
| `rsc_cmp_type` | LIVE | 1190:1533 |  |
| `ascii_lower` | LIVE | 1000:1978 |  |
| `rsc_cmp_name` | LIVE | 1190:0e72 |  |
| `file_read_chunk` | LIVE | 1000:272a, 1190:1abe, 1190:1b47, 1190:1bcb |  |
| `has_marker` | LIVE |  |  |
| `scan_records` | LIVE |  |  |
| `rsc_open` | LIVE |  |  |
| `rsc_close` | LIVE |  |  |
| `rsc_size` | LIVE |  |  |
| `rsc_record_count` | LIVE |  |  |
| `rsc_record_info` | LIVE |  |  |
| `rsc_be16` | LIVE |  |  |
| `rsc_be32` | LIVE |  |  |
| `rsc_be24` | LIVE |  |  |
| `rsc_le32` | LIVE |  |  |
| `rsc_rec_file_off` | LIVE |  |  |
| `rsc_rec_len` | LIVE |  |  |
| `rsc_parse_hdr_variant` | LIVE |  |  |
| `rsc_tag_printable` | LIVE |  |  |
| `rsc_find_dir` | LIVE |  |  |
| `rsc_record_data` | PARTIAL | 1190:0000 |  |
| `rsc_image_count` | PARTIAL |  |  |
| `rsc_get_indexed` | LIVE | 1190:0fda |  |
| `rsc_decode_bitmap` | LIVE |  |  |
| `rsc_decode_strlist` | LIVE | 1190:1953 |  |
| `rsc_decode_text` | LIVE | 1420:435b |  |
| `rsc_decode_gdat` | STUB | 1420:17d6 |  |
| `rsc_decode_kdat` | STUB |  |  |
| `rsc_decode_cvpc` | STUB |  |  |
| `rsc_strlist_count` | LIVE | 1190:1953 |  |
| `rsc_strlist_get` | LIVE |  |  |
| `rsc_text_find` | LIVE | 1420:2a5a, 1420:435b |  |
| `rsc_gdat_detail` | STUB | 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 |  |
| `rsc_kdat_detail` | STUB |  |  |
| `rsc_string_lookup` | PARTIAL | 1190:1533, 1420:2a5a, 1420:435b |  |
| `rsc_free_res_list` | LIVE | 1190:0c92, 1190:10e6 |  |
| `rsc_mgr_dispose` | LIVE | 1190:10e6, 1190:113f |  |
| `rsc_mgr_free` | LIVE | 1190:0c92 |  |
| `rsc_get_type_count` | LIVE | 1190:0978 |  |
| `rsc_load_to_mem` | LIVE | 1000:272a, 1190:0ee5, 1190:0fda |  |
| `rsc_table_name` | LIVE |  |  |
| `rsc_dir_needs_streaming` | LIVE | 1000:272a, 1190:0501 |  |
| `rsc_read_dir_chunked` | LIVE |  |  |
| `rsc_drop_file` | LIVE |  |  |
| `rsc_load_rsc` | LIVE | 1000:272a, 1008:0000, 1190:0000, 1190:0501, 1190:12ee, 1190:13a8, 1190:1692, 1420:00b1 |  |
| `rsc_load_rsc_disk` | LIVE | 1000:272a, 1190:0000, 1190:0501, 1190:1abe, 1190:1b47, 1190:1bcb |  |
| `rsc_load_rsc_auto` | PARTIAL |  |  |
| `rsc_read_u16_disk` | LIVE | 1000:272a, 1190:1953, 1190:1abe, 1420:3d00 |  |
| `rsc_read_u16_disk2` | LIVE | 1190:1b47 |  |
| `rsc_read_u32_disk` | LIVE | 1190:1a3f, 1190:1bcb |  |
| `rsc_dump_stats` | LIVE | 1000:2c9c, 1190:17d0 |  |
| `rsc_dispose_manager` | LIVE | 1000:1f7c, 1190:0c92 |  |
| `rsc_cvpc_head_off` | LIVE |  |  |
| `rsc_gdat_stride` | STUB | 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 |  |
| `rsc_kdat_stride` | STUB |  |  |
| `rsc_text_index` | LIVE |  |  |
| `rsc_strlist_all` | LIVE |  |  |
| `rsc_cvpc_is_compressed` | LIVE |  |  |
| `rsc_slice_len` | LIVE | 1190:0000 |  |
| `rsc_text_stats` | PARTIAL | 1420:2a5a, 1420:435b |  |
| `rsc_text_get_all` | PARTIAL |  |  |
| `rsc_text_search_all` | PARTIAL |  |  |
| `rsc_gdat_decode_row` | LIVE | 1028:0fb4, 1190:1953, 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 |  |
| `rsc_kdat_decode_row` | LIVE |  |  |

### `src/save.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `save_prompt_open` | STUB | 1130:0075 | has TODO |
| `save_prompt_save` | STUB | 1130:0194 | has TODO |
| `save_file_open` | LIVE | 1000:2ad9, 10f8:0000, 10f8:02bd, 1140:0139 |  |
| `save_file_size` | LIVE | 10f8:0000, 1140:0295 |  |
| `save_file_read` | LIVE | 1080:0023, 10f8:0000, 1140:019d |  |
| `save_file_write` | LIVE | 10f8:02bd, 1140:0245 |  |
| `save_file_close` | LIVE | 1000:1f7c, 1140:037c |  |
| `file_close_wrapper` | LIVE | 1000:1f7c, 1140:037c |  |
| `file_seek_wrapper` | LIVE | 1000:1f7c, 1000:272a, 1000:2ad9, 1140:0295, 1190:0fda |  |
| `save_serialize` | LIVE | 10f8:02bd, 10f8:090c, 1420:2cac |  |
| `save_deserialize` | LIVE | 1028:162c, 10f8:0000, 1140:0139, 1140:019d, 1140:0295 |  |
| `save_pack` | STUB | 10f8:02bd, 10f8:05da |  |
| `save_unpack` | STUB | 10f8:0000, 10f8:040a |  |
| `save_rll_encode` | LIVE |  |  |
| `save_rll_decode` | LIVE |  |  |
| `marshal_seek` | LIVE |  |  |
| `save_marshal_fields` | LIVE | 10f8:090c, 1310:0000, 1310:0008, 1420:a2d6, 1420:a2da |  |
| `xfer_reserve` | LIVE |  |  |
| `save_xfer_words` | LIVE | 10f8:0f40, 1310:0008 |  |
| `save_xfer_block` | LIVE | 10f8:0fcf |  |
| `save_xfer_array` | LIVE | 10f8:1035 |  |
| `save_xfer_strided` | LIVE | 10f8:10bd |  |
| `civ_save_ctx_init` | LIVE |  |  |
| `civ_save_ctx_add` | LIVE |  |  |
| `save_live_check` | LIVE |  |  |
| `save_live_copy_one` | LIVE |  |  |
| `save_xfer_words_live` | LIVE | 10f8:0f40, 1310:0008 |  |
| `save_xfer_block_live` | LIVE | 10f8:0fcf |  |
| `save_xfer_array_live` | LIVE | 10f8:1035 |  |
| `save_xfer_strided_live` | LIVE | 10f8:10bd |  |
| `save_marshal_fields_live` | LIVE | 10f8:090c |  |

### `src/sys.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `rng_seed` | LIVE | 1000:1716 |  |
| `rng_next_raw` | LIVE | 1000:16fe, 1000:1729 |  |
| `rng_range` | LIVE | 11c8:003d |  |
| `rng_init` | LIVE | 11c8:0000 |  |
| `util_clamp` | LIVE | 11c8:0063 |  |
| `util_progress` | LIVE | 1000:29ea |  |
| `debug_print` | LIVE | 1000:2c9c, 1190:17d0 |  |
| `dos_get_current_drive` | LIVE | 1000:1252 |  |
| `dos_get_current_drive_index` | LIVE | 1000:13e6 |  |
| `dos_get_current_dir` | LIVE | 1000:139c |  |
| `sys_get_tick_scaled` | LIVE | 1100:02e1 |  |
| `sys_init_paths` | LIVE | 1228:0002, 1228:0004 |  |
| `sys_current_drive` | LIVE | 1228:0002 |  |
| `sys_yield` | LIVE | 1098:0157, 10b0:0000 |  |
| `sys_pump_messages` | LIVE | 10b0:0000, 1108:0000, 11c0:0ec2, 11e8:02fb, 1328:0012 |  |
| `sys_get_client_rect` | STUB | 1000:137f, 1218:0956, 1218:0a06 | has TODO |
| `status_measure_row` | STUB | 1088:1460, 1098:0dda, 1138:0269, 11f0:4754, 1220:0b88 | has TODO |
| `sys_check_runtime_memory` | STUB | 11e8:0075, 1420:43b4, 1420:4473, 1420:44ff | has TODO |
| `sys_check_free_memory` | STUB | 11e8:0000, 1420:43b8 |  |
| `file_write_chunk` | LIVE | 1000:272a |  |
| `ne_entry_count` | LIVE |  |  |
| `ne_entry_addr` | LIVE |  |  |
| `sys_run_init_table` | LIVE | 1000:0000, 1000:00de |  |
| `sys_run_exit_table` | LIVE | 1000:00b2, 1000:0122, 1000:127f |  |
| `sys_run_exit_table_single` | STUB | 1000:00b2, 1000:0122, 1420:5140 | has TODO |
| `sys_noop_exit_hook1` | NOOP | 1000:00c4, 1000:00c5, 1000:127f |  |
| `sys_noop_exit_hook2` | NOOP | 1000:00c5 |  |
| `dos_exit_process` | STUB | 1000:00c6, 1000:12d6 | has TODO |
| `sys_do_exit` | STUB | 1000:00b2, 1000:00c4, 1000:00c6, 1000:127f | has TODO |
| `sys_exit_fallback` | STUB | 1000:0000, 1000:12d6 | has TODO |
| `float_check_range` | STUB | 1000:0164, 1000:06cf, 1420:4ad6, 1420:50cb | has TODO |
| `float_store_converted` | STUB | 1000:06cf | has TODO |
| `float_decompose_exponent` | STUB | 1000:0221, 1000:0754, 1000:0784, 1000:0988, 1000:0e20 | has TODO |
| `sys_noop_pad` | NOOP | 1000:00c4, 1000:0221, 1000:03b6 |  |
| `float_discard_st0` | NOOP | 1000:06c7 |  |
| `float_pack_bcd_chunk` | LIVE | 1000:096a, 1000:0988 |  |
| `float_convert_extended_to_bcd` | LIVE | 1000:096a, 1000:0988 |  |
| `float_log2` | LIVE | 1000:0c03 |  |
| `float_exp2` | LIVE | 1000:0c7d, 1000:0c7e |  |
| `float_matherr_filter` | LIVE | 1000:0e2e |  |
| `util_sprintf` | LIVE | 1000:0221, 1000:1056, 1000:19d8, 1000:1a3c, 1000:2c9c, 1000:35f6, 1000:42d3, 1130:0075, 1420:4eeb |  |
| `sys_fatal_abort` | STUB | 1000:35f6, 1000:37b2, 1000:4124, 1608:01a4 | has TODO |
| `float_report_error` | LIVE | 1000:1056, 1000:4124, 1420:4b96, 1420:4b9e |  |
| `float_pow10` | LIVE | 1000:0d56, 1420:4b0e |  |
| `float_pow10_far` | LIVE | 1000:0d56, 1000:0e20 |  |
| `float_exp2_checked` | LIVE | 1000:0c7d, 1000:0e5a, 1420:4a92, 1420:4b96 |  |
| `float_pow` | LIVE | 1000:0c03, 1000:0e5a, 1000:0ec8, 1420:4b9a |  |
| `float_scan_is_space` | LIVE |  |  |
| `float_scan_float` | LIVE | 1000:03bc, 1000:0605, 1420:4af6, 1420:4afe |  |
| `util_emit_hex_digit` | LIVE | 1000:19d0 |  |
| `util_emit_hex_byte` | LIVE | 1000:19c7 |  |
| `util_emit_hex_word` | LIVE | 1000:19c0 |  |
| `util_vsprintf_strlen` | LIVE | 1000:19f2 |  |
| `util_format_int` | LIVE | 1000:157a |  |

### `src/title.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `title_play_intro` | STUB |  | has TODO |
| `title_play_anim_loop` | STUB | 1030:0000, 1030:79ce, 1038:0000, 1040:068e, 1040:1137, 1080:099b, 1098:124f | has TODO |
| `title_decode_frame` | STUB | 1018:015d, 1088:1dd0, 1138:0000, 11b0:0000, 11b8:14e5 | has TODO |
| `pic_decompress_chunk` | LIVE | 10f8:040a, 10f8:05da, 1420:23ce |  |
| `pic_read_header` | STUB | 10d0:0956, 1140:019d, 1420:2300, 1420:2372 | has TODO |
| `title_load_opening_frame` | STUB | 1030:0000, 10d0:0956, 1190:0000 | has TODO |
| `title_poll_skip` | LIVE |  |  |
| `title_max_ms` | LIVE |  |  |
| `title_play_intro_sdl` | LIVE | 1030:0000 |  |
| `title_mplogo_show` | PARTIAL |  |  |

### `src/video.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `video_init` | LIVE | 1008:059a, 1080:0119, 1080:06ff, 1090:0376, 1090:0488, 10b0:0461, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0956, 10d0:0cfa, 1200:00a8, 1420:0af4 |  |
| `video_blit_indexed` | LIVE | 10d0:0854 |  |
| `video_present` | LIVE | 10d0:0854 |  |
| `video_shutdown` | NOOP |  |  |
| `palette_animate_range` | LIVE | 1090:0376, 10b0:0461, 1200:00a8 |  |
| `video_check_driver_caps` | LIVE | 1008:059a, 1420:0af4 |  |
| `palette_init` | LIVE | 1090:0488 |  |
| `gfx_port_create` | STUB | 10d0:0000, 10d0:0cfa | has TODO |
| `gfx_port_alloc_bitmap` | LIVE | 10d0:0000, 10d0:08f1, 10d0:0cfa |  |
| `gfx_port_set_active` | STUB | 10d0:0956 | has TODO |
| `gfx_port_present_rect` | LIVE | 1080:0119, 1080:06ff, 10d0:0854, 10d0:08f1, 10d0:0cfa |  |
| `gfx_port_blit` | PARTIAL | 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0cfa | has TODO |
| `mainport_get_or_create` | STUB | 1098:114a, 10d0:0956, 1208:0af1, 1210:025b, 1218:0386 | has TODO |
| `gfx_port_fill_or_copy` | STUB | 10b0:0461, 10b8:079d, 10d0:0854 | has TODO |
| `gfx_port_load_rsc_frame` | STUB | 1090:0376, 1090:0488, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0cfa | has TODO |
| `util_draw_progress_bar` | STUB | 1000:29ea, 10d0:08f1 | has TODO |
| `gfx_port_change_color` | STUB | 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0956, 10d0:09ee, 10d0:0cfa | has TODO |
| `gfx_port_get_dc` | STUB | 1098:114a, 10d0:0000 | has TODO |
| `gfx_port_release_dc` | STUB | 1098:114a, 10d0:0000 | has TODO |
| `gfx_port_create_compatible` | PARTIAL | 10d0:0000, 10d0:0cfa | has TODO |
| `palette_apply_to_port` | LIVE | 1080:0119, 1080:06ff, 1090:0376, 10b0:0461, 10d0:0854, 1200:00a8 |  |
| `video_shutdown_display` | NOOP | 1008:0329, 1008:0e99, 10d0:0854, 10d0:08f1, 10d0:0cfa, 1420:00a4 |  |
| `font_blit_glyph` | LIVE | 1008:0e99, 10d0:0854, 10d0:08f1 |  |

### `src/worldgen.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `worldgen_dir_dx_at` | LIVE |  |  |
| `worldgen_dir_dy_at` | LIVE |  |  |
| `worldgen_generate_random` | STUB | 1028:0000, 1028:066a, 1028:0a93, 1028:0fb4, 1028:12c6, 11c8:003d | has TODO |
| `worldgen_load_preset` | STUB | 1028:0a93, 1028:1d15, 10b8:079d | has TODO |
| `worldgen_load_earth` | STUB | 1028:0a93, 1028:1d15, 10b8:079d, 1108:0243, 11e0:0221 | has TODO |
| `worldgen_prepare_earth` | STUB | 1108:01bc, 1108:0243 | has TODO |
| `worldgen_copy_preset` | LIVE | 1028:0000, 10b8:0000, 10b8:079d, 1420:20a9 |  |
| `worldgen_customize` | STUB |  | has TODO |
| `worldgen_customize_dialog` | STUB | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6, 1058:0b4d, 1098:124f, 1098:1612 | has TODO |
| `worldgen_seed_one_island` | STUB | 1028:0945, 1248:0002 | has TODO |
| `worldgen_paint_blob` | LIVE | 1028:09ef |  |
| `worldgen_label_continents` | STUB | 1028:0a93, 1028:12c6, 1028:1ca7, 1028:1d15 | has TODO |
| `worldgen_gen_rivers` | STUB | 1028:066a, 1028:06c1 | has TODO |
| `worldgen_spread_pollution` | STUB | 1028:1afd, 1110:1b36 | has TODO |
| `worldgen_score_city_sites` | STUB | 1028:0f0c, 1028:0fb4, 1420:17d6 | has TODO |
| `worldgen_build_continent_links` | STUB | 1000:29ea, 1028:12c6, 1098:0157, 10b0:0000, 1128:16d0 | has TODO |
| `worldgen_build_ocean_links` | STUB | 1000:29ea, 1028:0f01, 1028:162c, 10f8:01e9 | has TODO |
| `worldgen_copy_rect` | LIVE | 1028:06c1, 1028:1d15, 1028:1dbd, 1028:1dfb |  |
| `path_check_continent_link` | LIVE | 1028:12c6, 1128:16d0 |  |
