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
| Art_MaybeConvertArtwork | `art_maybe_convert_artwork` | LIVE | na Linuksie zbędne — no-op CIV_OK |
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
| `audio_resolve_path` | LIVE |  |  |
| `audio_stop_stream` | LIVE |  |  |
| `sound_mci_open` | LIVE | 1000:1184, 1150:001c, 1150:0066, 1150:00a0 |  |
| `sound_mci_close` | LIVE | 1150:0066 |  |
| `sound_play_file` | LIVE | 1000:1184, 1150:00a0 |  |
| `sound_mci_play` | LIVE | 1150:001c, 1150:0066 |  |
| `audio_init` | LIVE | 1150:001c |  |
| `audio_play` | LIVE | 1150:00a0 |  |
| `sound_normalize_id` | LIVE | 1150:014f |  |
| `sound_map_id_to_wav` | LIVE | 1150:019e, 1150:025b, 1420:3080 |  |
| `audio_gap_is_silent` | LIVE | 1150:00c9, 1150:019e, 1150:025b, 1150:0266, 1420:3080 |  |
| `sound_is_stop` | LIVE | 1150:00c9 |  |
| `sound_file_id_for` | LIVE | 1150:014f, 1150:019e |  |
| `sound_is_opening` | LIVE | 1028:0000, 11e0:0000, 1420:3114 |  |
| `audio_leader_count` | LIVE | 1150:019e, 1150:0266, 1420:3080 |  |
| `audio_leader_names` | LIVE |  |  |
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
| `font_face_list` | STUB | 1008:0e99 | has TODO |
| `font_fnt_parse` | LIVE | 1008:0e99 |  |
| `font_face_bitmap` | LIVE | 1008:0e99 |  |
| `font_fnt_row_ptr` | LIVE |  |  |
| `font_install_slot` | STUB | 1008:062f, 1008:0e12, 11e8:0d1d | has TODO |
| `font_fnt_face_name` | LIVE | 1008:0e99 |  |
| `font_slot_claim` | LIVE | 1008:0e12 |  |
| `font_free_slots` | LIVE | 1008:0000, 1008:0329, 1008:062f, 1008:066d, 1008:075b, 1008:0e12, 1008:0e99, 1098:0186, 1098:0194, 1100:0a33, 11e8:0b40, 11e8:0cb0, 11e8:0d1d, 1420:00fe |  |
| `font_enum_families_callback_tail` | NOOP | 1008:09d3, 1008:0d4c, 1008:0e12 |  |
| `font_fnt_char_entry` | PARTIAL | 1008:0e99 |  |
| `font_fnt_bits` | LIVE | 1008:0e99 |  |
| `font_render_glyph` | LIVE | 1008:0e99 |  |
| `font_text_width` | LIVE | 1098:0dda, 11f8:0278 |  |
| `font_text_height` | LIVE | 1098:0dda |  |
| `font_fnt_stride` | LIVE | 1008:0e99 |  |
| `font_fnt_glyph_len` | LIVE | 1008:0e99 |  |
| `menu_strings_count` | LIVE | 1048:0000, 1420:435b, 14b0:0010, 14b0:0092, 14d0:0028 |  |
| `menu_string_at` | LIVE |  |  |
| `config_load_prefs` | STUB | 1008:0b4a, 12d0:0064, 1420:010b | has TODO |
| `config_save_prefs` | STUB | 1000:116a, 1000:1184, 1008:0bbf, 1420:0113 | has TODO |
| `boot_run` | PARTIAL | 1000:11bb, 1000:2826, 1008:0000, 1008:059a, 1008:0b4a, 1008:0d68, 1008:0e99, 1088:0000, 1090:0488, 1098:06f8, 1098:096c, 10b8:0000, 10e8:2d46, 1190:0000, 11e8:0000, 11e8:0337 |  |
| `boot_shutdown` | LIVE | 1008:0000, 1008:0329, 1008:0d68, 1208:0af1 |  |
| `art_maybe_convert_artwork` | LIVE | 1090:0488, 10b8:0000, 10b8:04df, 10b8:062d, 10b8:06a0, 10b8:076a, 10b8:079d, 10b8:0aa8, 10b8:0c1c, 10b8:0ded, 10b8:0e36, 10b8:11bc, 10b8:158c, 10d0:0cfa, 1420:20a9 |  |
| `main_run_startup_sequence` | STUB | 1008:0329, 1090:0376, 1098:096c, 1098:1104, 1098:124f, 10b0:008a, 10b0:0461, 10e8:0000, 1108:0243, 1130:000e, 1150:00c9, 11e0:0000, 11e0:0221, 1420:435b |  |
| `config_prefs_defaults` | LIVE | 1008:0b4a, 1008:0bbf, 1150:00c9, 12d0:0064 |  |
| `config_prefs_sounds_on` | LIVE |  |  |
| `boot_video_caps_count` | LIVE | 1008:059a, 1420:0af4 |  |
| `boot_video_caps_describe` | STUB |  |  |
| `boot_shutdown_step_count` | LIVE | 1000:127f, 1000:12d6, 1000:1fc6, 1000:2098, 1008:0000, 1008:0329, 1008:062f, 1008:066d, 1008:075b, 1008:0bbf, 1008:0e12, 1008:0e99, 1038:0c1d, 1058:0d71, 1088:0b6d, 1090:0596, 1098:02d1, 1098:062f, 10b8:0000, 10d0:0172, 1100:060b, 1110:0127, 1148:0020, 1180:0000, 1190:0c92, 11e0:0221, 12b0:0004, 12b0:0006, 12d8:28fa, 12d8:4598, 12d8:460e, 1330:0004, 1420:00fe, 1420:0130, 1420:5148, 1420:514a, 1420:547e, 1420:9314, 1420:9964, 1420:9966, 1420:b388 |  |
| `boot_shutdown_step_name` | LIVE | 1008:0bbf |  |
| `boot_shutdown_exit_code` | LIVE | 1000:127f, 1000:12d6 |  |
| `config_prefs_pack` | LIVE | 1008:0bbf |  |
| `config_prefs_unpack` | LIVE | 1008:0b4a, 12d0:0064 |  |
| `config_prefs_needs_path_setup` | LIVE | 1000:116a, 1000:1184, 1008:0bbf, 1228:0002 |  |
| `main_quit_msg_is_quit` | LIVE | 1008:0329, 1098:062f |  |
| `main_quit_flag_requests_shutdown` | LIVE | 12d8:24ee |  |
| `main_dispatch_quit_requests_shutdown` | LIVE | 1048:01cd |  |
| `main_drain_should_continue` | LIVE | 1098:02d1, 11e8:02fb, 12b0:004c |  |
| `main_pump_quit_should_continue` | LIVE | 1008:0f77 |  |
| `main_pump_msg_is_char` | LIVE | 1008:0a8d, 1098:02d1 |  |
| `main_pump_msg_is_keydown` | LIVE | 1208:0000 |  |
| `main_drain_tail_offset` | LIVE | 11e8:02fb, 1420:11f2 |  |
| `main_pump_until_flag_should_continue` | LIVE | 11e8:02a4 |  |
| `main_pump_timer_tick` | LIVE | 1008:0d68 |  |
| `boot_timer_proc_interval_ms` | LIVE | 1008:0000, 1008:00cd, 1008:0329, 1008:0d68, 12d8:460e |  |
| `main_startup_anim_loops` | LIVE | 10b0:008a, 10b0:0461, 10e8:0000, 1108:01bc, 11e0:0000 |  |
| `main_startup_intro_sound` | LIVE | 1420:3114 |  |
| `main_startup_stop_sound` | LIVE |  |  |
| `main_startup_should_run_wizard` | LIVE | 1098:1612, 10e8:0000, 1108:01bc, 11e0:0000 |  |
| `boot_free_memory_is_low` | LIVE | 11e8:0000 |  |
| `boot_runtime_needs_first_warning` | LIVE | 1088:0000, 11e8:0075 |  |
| `boot_runtime_needs_final_warning` | LIVE |  |  |

### `src/civ_common.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `civ_strerror` | PARTIAL |  |  |
| `util_str_copy` | LIVE | 1000:363e, 1000:36b2 |  |
| `util_str_append` | LIVE | 1000:363e |  |
| `util_int_to_dec` | LIVE | 1000:2928, 1088:1dd0, 12d8:24fa |  |
| `util_to_lower` | LIVE | 1000:1978, 1190:0e72 |  |
| `util_mem_copy_far` | LIVE | 1000:137f, 1008:0bbf, 1080:0607, 1098:0d43, 1218:0956 |  |
| `util_mem_set_core` | LIVE | 1000:29c4 |  |
| `util_mem_set` | LIVE | 1000:29c4, 1000:29ea, 1130:0075, 11e0:0221, 12d8:4334 |  |
| `util_mem_copy` | LIVE | 1000:137f, 1000:299e, 1000:2e85 |  |
| `util_mem_move_core` | LIVE | 1000:2a30 |  |
| `util_mem_move` | LIVE | 1000:2a30, 1000:2a89 |  |
| `util_str_copy_nul` | LIVE | 1140:040e |  |
| `util_pascal_to_c` | LIVE | 1100:0053, 1100:078a, 1198:00a3 |  |
| `util_byte_swap16` | LIVE | 1000:04b5, 1140:04b5 |  |
| `util_swap_high16` | LIVE | 1140:045c |  |
| `util_strip_trailing_zeros` | LIVE | 0221:03aa, 1000:01fe, 1000:0221 |  |
| `util_format_float` | STUB | 1000:01fe, 1000:0221, 1000:0754, 1420:4ad6, 1420:50cb | has TODO |
| `util_str_case_cmp` | LIVE | 1000:36de, 11f8:01af |  |
| `util_format_message` | LIVE | 1000:2928, 1000:2c9c, 1000:36b2, 1088:1dd0, 1190:17d0 |  |
| `util_compare_bounded` | LIVE | 1000:36de, 1000:3744, 1000:377e, 1008:0e12, 1008:0e99, 11f8:014a, 11f8:01af, 11f8:0207, 1420:011b |  |

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
| `anim_pump_if_active` | STUB | 1098:0157, 10a8:0000, 10a8:00b4, 10a8:00fc, 10a8:0228, 10b0:0000, 10b0:0461, 10b0:151b, 1150:00c9, 1420:1f40 | has TODO |
| `anim_advance_frames` | STUB | 10b0:0461, 1150:00c9 | has TODO |
| `dlg_center_window` | PARTIAL | 1218:0956 | has TODO |
| `dialog_open_help` | STUB |  | has TODO |
| `help_hlp_parse` | LIVE |  |  |
| `help_hlp_system` | LIVE |  |  |
| `help_hlp_topic` | LIVE |  |  |
| `ctrl_class_names` | LIVE | 1058:0b4d |  |
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
| `diplomacy_post_contact` | STUB | 1168:0a07, 1168:0dda, 1168:0e8e, 1168:0f47, 1168:1220, 11c0:0395, 1420:62fc | has TODO |
| `diplomacy_eval_bribe` | STUB | 1078:1480, 1078:455c, 11c0:0395 | has TODO |
| `ui_draw_end_turn_prompt` | STUB | 10d0:0854, 10d8:00e0, 1110:0448, 11c0:0ec2, 1210:07ae | has TODO |
| `dialog_menuz_id_to_index` | LIVE | 1220:0156 |  |
| `dialog_menuz_index_to_id` | LIVE | 1220:0156 |  |
| `text_align_x` | LIVE | 1098:0e0b, 1098:0ed1 |  |
| `text_center_y` | LIVE | 1098:0dda, 1098:0ed1 |  |
| `dialog_dlgunits_to_pixels` | LIVE | 1098:0d43, 1100:03ff |  |
| `dialog_client_rect` | LIVE | 1100:03ff |  |
| `dlg_center_rect` | LIVE | 1068:0958, 1098:0a5b, 1098:0ae0, 1098:0b67, 1170:2623, 11f8:0000, 1210:0e29 |  |
| `text_draw_aligned` | STUB | 1080:08e9, 1088:1554, 1098:00b5, 1098:01b0, 1098:0e0b, 1098:0ed1, 1098:103f, 1098:20f4, 10b0:137a, 10e8:0667, 1110:07c3, 1180:02d3, 1220:0889 | has TODO |
| `dialog_get_item_rect` | STUB | 1098:0d43, 1100:03ff, 1220:0156 | has TODO |
| `dialog_show_message` | STUB | 1040:068e, 1098:124f, 1098:1612, 1220:00d2, 1220:0156, 1220:0eb8, 12b0:0004 | has TODO |
| `text_res_list_advance` | LIVE | 1098:1f4f, 1100:0053, 1100:078a |  |
| `main_enqueue_res_string` | LIVE | 1008:0a8d, 1420:4473 |  |
| `dialog_res_list_spec` | LIVE | 1068:0295 |  |
| `main_res_queue_full` | LIVE | 1008:0a8d, 1008:0add, 1098:02d1, 11f0:4ea0 |  |
| `dialog_init_res_string_tables` | STUB | 1068:0295, 1098:1f4f | has TODO |
| `main_show_res_string` | STUB | 1008:0a8d, 1008:0add, 1100:078a | has TODO |
| `text_dlg_string_source` | LIVE | 1098:2796, 1100:078a, 1100:0947 |  |
| `text_set_dlg_item_text` | STUB | 1098:2796, 1100:0053, 1100:0947 | has TODO |
| `text_draw_dlg_item_string` | STUB | 1098:2796, 10f0:0066, 1100:03ff, 1100:0947 | has TODO |
| `text_fill_res_string_list` | STUB | 1068:0295, 1098:1f4f, 1100:0053, 1100:078a | has TODO |
| `diplo_contact_flags` | LIVE | 1168:0a07, 1168:0dda |  |
| `diplo_is_peace` | LIVE | 1168:0a07 |  |
| `diplo_is_contact` | LIVE |  |  |
| `diplo_has_embassy` | LIVE |  |  |
| `diplo_apply_peace` | LIVE | 1168:0dda |  |
| `diplo_apply_war` | LIVE | 1168:0e8e |  |
| `diplo_tribute_amount` | LIVE | 1160:328e, 1168:0000, 11c8:0063 |  |
| `diplo_should_exchange` | LIVE | 1168:1220, 1170:2aaa, 1170:3181 |  |
| `diplo_government_tick` | LIVE | 1168:0000, 1168:06c1 |  |
| `diplo_spy_decay_step` | LIVE | 1168:0000, 11c8:003d |  |
| `diplo_war_end_tick` | LIVE | 1168:0000 |  |
| `diplo_war_end_clear` | LIVE | 1168:0000 |  |
| `diplo_tribute_turn` | LIVE | 1168:0000 |  |
| `diplo_government_factor` | LIVE | 1168:06c1 |  |
| `diplo_government_needs_tech` | LIVE | 1030:7e57, 1168:06c1, 1170:3181 |  |
| `diplo_government_available` | LIVE | 1168:06c1 |  |
| `diplo_peace_forced` | LIVE | 1168:0f47, 1168:1366, 1168:13b4 |  |
| `diplo_tech_table_off` | LIVE | 1168:1366, 1168:138d, 12d8:3c9c |  |
| `diplo_tech_value_at` | LIVE | 1168:1366, 12d8:3c9c |  |
| `diplo_tech_cost_at` | LIVE | 1168:138d, 12d8:3d9c |  |
| `diplo_military_at` | LIVE | 1168:13b4, 12d8:4160 |  |
| `diplo_contact_tribute_scaled` | LIVE | 1160:0000, 11c8:0063 |  |
| `diplo_contact_afford_snap` | LIVE |  |  |
| `diplo_mercenary_amount` | LIVE | 1160:0000 |  |
| `diplo_threat_gold` | LIVE | 1160:0000 |  |
| `diplo_peace_stamp` | LIVE | 1160:0000 |  |
| `diplo_open_contact_dialog` | STUB | 1030:7e57, 1160:0000, 1168:1366, 1168:13b4 | has TODO |
| `diplo_prompt_contact` | STUB | 1098:124f, 1150:019e, 1160:20c0 | has TODO |
| `diplo_run_contact_menu` | STUB | 1160:328e, 1220:0156, 1420:31b4 | has TODO |
| `dialog_frame3d_layers` | LIVE | 1220:0d38 |  |
| `dialog_frame3d_dark` | LIVE |  |  |
| `dialog_frame3d_light` | LIVE |  |  |
| `dialog_frame3d_layer` | LIVE |  |  |
| `dialog_draw_frame3d` | STUB | 1018:24c5, 1220:0156, 1220:0d38 | has TODO |
| `text_shadow_offset` | LIVE | 10d8:02df |  |
| `text_centered_x` | LIVE | 10d8:0495, 10d8:0528 |  |
| `text_measure_scaled` | LIVE | 10d8:05b8, 1100:00a1 |  |
| `text_pascal_len` | LIVE | 1100:0000 |  |
| `text_font_table` | LIVE | 11e8:0d1d |  |
| `text_font_table_lookup` | LIVE | 11e8:0b40, 11e8:0d1d |  |
| `text_font_slot_for_id` | LIVE | 11e8:0b40, 11e8:0d1d |  |
| `text_font_height_for_id` | LIVE |  |  |
| `text_dispatch_font` | LIVE | 1098:0186, 1098:0194, 11e8:0b40, 11e8:0cb0 |  |
| `text_pack_pascal` | LIVE | 1100:0000, 1100:0053, 11f8:00fc, 11f8:014a |  |
| `text_pascal_payload` | LIVE | 1100:0000, 1100:00a1 |  |
| `text_draw_shadowed` | STUB | 1098:0e0b, 10d8:02df, 11e8:0b40, 1220:0f2f | has TODO |
| `text_draw_at` | STUB | 1098:0e0b, 10d8:01fa | has TODO |
| `text_draw_clipped` | STUB | 10d8:00e0, 10d8:01b0 | has TODO |
| `text_draw_direct` | STUB | 10d8:00e0, 1100:0000, 1100:017b | has TODO |
| `text_draw_centered` | STUB | 10d8:0528, 10d8:05b8 | has TODO |
| `text_draw_header_centered` | STUB | 10d8:046e, 10d8:0495 | has TODO |
| `text_select_font` | PARTIAL | 1008:0e12, 1098:0186, 1098:0194, 1100:0a33, 11e8:0b40, 11e8:0cb0 | has TODO |
| `text_font_pre_apply` | NOOP | 1098:0186, 11e8:0cb0 |  |
| `text_font_post_apply` | NOOP | 1098:0194, 11e8:0d0e |  |
| `text_fit_pascal_ellipsis` | LIVE | 1098:0000, 1100:00a1 |  |
| `text_fit_scaled_width` | LIVE | 10d8:05b8, 11d0:02e4 |  |
| `text_fit_pascal_ellipsis_scaled` | LIVE | 1018:0be0, 1030:0000, 11d0:02e4 |  |
| `unit_popup_text_limit` | LIVE | 11f0:468c, 11f0:4808, 11f0:4a76 |  |
| `anim_opcode_is_valid` | LIVE | 10b0:0461 |  |
| `anim_opcode_is_palette` | LIVE | 1090:0376, 1098:201d |  |
| `anim_opcode_is_slot` | LIVE | 10a8:0000, 10a8:00b4, 10a8:00fc, 10a8:018e, 1150:00c9, 11e0:0000 |  |
| `anim_opcode_needs_store` | LIVE | 1098:1104 |  |
| `anim_opcode_is_callback` | LIVE |  |  |
| `anim_opcode_is_anim` | LIVE |  |  |
| `anim_tick_needs_store` | LIVE | 1000:137f, 1098:1104 |  |
| `anim_seq_is_finished` | LIVE |  |  |
| `anim_slot_init` | LIVE | 10a8:0000 |  |
| `anim_slot_frame_at` | LIVE | 10a8:00fc |  |
| `anim_slot_is_finished` | LIVE |  |  |
| `anim_slot_blend` | LIVE | 10a8:0228 |  |
| `anim_slot_play_frame` | LIVE |  |  |
| `anim_seq_phase_step` | LIVE | 10b0:0000, 10b0:008a, 10b0:0287, 10b0:0461, 10b8:079d |  |
| `anim_dissolve_rand_index` | LIVE | 1000:1729, 10b0:151b, 1100:03ea, 11c8:003d |  |
| `anim_dissolve_perm_init` | LIVE |  |  |
| `anim_dissolve_swap` | LIVE |  |  |
| `anim_dissolve_col` | LIVE | 10b0:17e5 |  |
| `anim_rebase_clock` | LIVE | 10b0:005b, 10b0:008a, 1100:02e1, 11e0:0000, 11e0:0221 |  |
| `dialog_menuz_art_mode` | LIVE | 1220:0156 |  |
| `dialog_show_message_id` | LIVE | 1220:00d2, 1220:0eb8 |  |
| `dialog_menuz_should_loop` | LIVE | 11e0:0221, 12d8:44e2 |  |
| `dialog_menuz_is_cancel` | LIVE |  |  |
| `dlg_id_needs_preset_size` | LIVE | 1098:124f |  |
| `dialog_split_is_sep` | LIVE | 1220:0000 |  |
| `dialog_split_menu_count` | LIVE |  |  |
| `dialog_split_menu_copy` | LIVE |  |  |
| `dialog_split_menu_text` | PARTIAL | 1100:00ea, 1220:0000 | has TODO |
| `dialog_menu_track_max` | LIVE | 1100:00ea, 1220:0000 |  |
| `dialog_menu_title_base` | LIVE |  |  |
| `text_aligned_list_x` | LIVE | 1068:258f, 1098:0ed1 |  |
| `text_aligned_list_row_y` | LIVE | 1068:2072, 1068:258f |  |
| `text_aligned_list_should_stop` | LIVE | 1068:2072, 1068:258f |  |
| `text_aligned_list_needs_unlock` | LIVE | 1068:255a |  |
| `text_wrapped_list_cap` | LIVE | 1068:2072 |  |
| `text_draw_aligned_list` | STUB | 1068:258f, 1100:00ea, 1608:01cc | has TODO |
| `text_draw_aligned_list_free` | STUB | 1068:255a | has TODO |

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
| `game_pick_wonder_for_ai` | LIVE | 1088:1e32, 1170:05ca, 1170:3181 |  |
| `civ_process_unit_moves` | LIVE | 1078:1480, 1128:0000, 11c0:0395, 11c0:0ec2, 11c0:16a4, 11f0:0000, 11f0:44d4, 1420:62fc |  |
| `status_draw_tech_icons` | STUB | 1088:1a82, 11a8:0cb1 | has TODO |
| `game_append_year_text` | LIVE | 1000:18d4, 1000:1ed2, 1000:2928, 1000:363e, 1000:377e, 1000:386e, 1000:3ba0, 1000:3d90, 1018:015d, 1088:13d5, 1088:1dd0, 1138:0000, 1198:00a3, 11b0:0000, 11b8:14e5, 12d8:24fa, 1420:173c, 1420:1740 |  |
| `year_suffix` | LIVE | 1000:2928, 1088:1dd0, 12d8:24fa, 1420:173c, 1420:1740 |  |
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
| `unit_popup_is_end_of_turn` | LIVE | 1000:36b2, 1018:067b, 1030:0000, 1038:0bba, 1038:0c1d, 1080:0119, 1098:0000, 1098:00b5, 1098:0dda, 1098:0e0b, 1098:1104, 10d0:0854, 10d0:09a2, 10d8:00e0, 10e8:0000, 1100:0a33, 1110:0ff6, 1110:16c0, 1110:199b, 1180:0000, 11c8:00c4, 11f0:468c, 11f0:4808, 11f0:4a76, 11f8:0080, 11f8:0278, 1218:0abf, 1420:1a3c, 1420:47dd, 1420:47e9, 1420:47f6, 1420:4803, 1420:480b, 1420:481d, 1420:482f, 1420:483c, 1420:4845 |  |
| `unit_popup_has_veteran` | LIVE | 1420:4803 |  |
| `unit_popup_moves_full` | LIVE | 11c8:0063 |  |
| `unit_popup_moves_frac` | LIVE |  |  |
| `unit_popup_badge_needs_wrap` | LIVE |  |  |
| `combat_resolve_attack` | STUB | 1050:0000, 1078:1480, 11c0:1bc3 | has TODO |
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
| `unit_type_name` | LIVE | 1420:62ff |  |
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
| `tech_names` | LIVE | 1088:108a, 10e8:03c6, 1100:078a, 1170:3181, 1180:02d3, 11f8:0080 |  |
| `wonder_names` | LIVE | 1088:1e32, 1100:078a, 1170:3181, 11f8:0080 |  |
| `terrain_cost_table` | LIVE | 1110:16c0, 1420:17d4 |  |
| `game_shutdown` | LIVE |  |  |
| `city_format_pop_count` | LIVE | 11c8:03ac, 12d8:42ec, 1420:172b |  |
| `city_calc_pop_total` | LIVE | 11c8:0334, 1420:5500, 1420:5502, 1420:5503, 1420:5507 |  |
| `city_format_name` | LIVE | 1048:0101, 11f8:014a |  |
| `city_format_census_ratio` | LIVE | 1000:2928, 1018:202a |  |
| `demo_approval` | LIVE | 1188:123a |  |
| `demo_literacy` | LIVE | 1170:3181, 1188:123a, 11c8:0063 |  |
| `demo_disease_rate` | LIVE | 1170:3181, 1188:123a |  |
| `demo_life_years` | LIVE | 1188:123a |  |
| `demo_rank_player` | LIVE | 1188:220a |  |
| `status_lcg_next` | LIVE | 1088:1a82 |  |
| `demo_show_demographics` | STUB | 1018:0380, 1110:16c0, 1110:16ee, 1188:123a, 1188:220a, 1420:5500 | has TODO |
| `demo_rank_row` | STUB | 1188:220a, 1420:3ced | has TODO |
| `city_show_census_report` | STUB | 1018:0000, 1018:0380, 1018:1d5b, 1018:202a, 1030:0000 | has TODO |
| `census_show_tech_bars` | STUB | 1018:0000, 1018:0380, 1018:03f0, 11c8:008a | has TODO |
| `census_show_unit_list` | STUB | 1018:067b, 11c8:008a | has TODO |
| `census_show_terrain_counts` | STUB | 1018:09cc | has TODO |
| `trade_calc_bonus` | LIVE | 1038:0d09, 1110:1d99, 1170:3181, 11c8:02ec, 11e8:0dcf, 1420:5506 |  |
| `trade_route_value` | LIVE | 1038:0d09, 1420:5513 |  |
| `trade_deliver_caravan_goods` | STUB | 1038:0d09, 1088:13d5, 11c0:16a4, 1220:00d2, 1420:5500 | has TODO |
| `diplomat_revolt_cost` | LIVE | 1168:12c1, 11b8:0000, 1420:5502 |  |
| `diplomat_bribe_cost` | LIVE | 1168:12c1, 11b8:07c2 |  |
| `diplomat_city_action` | STUB | 11b8:0000, 12d8:3a0a | has TODO |
| `diplomat_bribe_unit` | STUB | 1088:13d5, 1110:03ae, 11b8:07c2, 11c0:12c8, 11c0:16a4, 11c0:1bc3 | has TODO |
| `city_do_revolt` | STUB | 1038:0000, 1110:171f, 11b8:0f26, 11c0:12c8, 1220:00d2, 1420:4168 | has TODO |
| `pic_play_shared` | STUB | 1030:0000, 1030:79ce, 1030:7c28, 1030:7e57, 1030:7ece, 1030:7f65, 1030:7fc1, 1098:124f | has TODO |
| `tech_has_tech` | LIVE | 1170:3181 |  |
| `pic_mode_is_modal` | LIVE | 1030:0000, 1098:124f, 1170:05ca |  |
| `pic_mode_is_sim` | LIVE | 1030:0000 |  |
| `city_get_specialist` | LIVE | 1030:7fc1, 1420:5510 |  |
| `city_set_specialist` | LIVE | 1030:7f65 |  |
| `unit_is_settler` | LIVE | 11c8:00c4, 11f0:0000, 11f0:1ecd, 11f0:1f24, 11f0:2012, 11f0:228a, 1420:62ff |  |
| `unit_is_caravan` | LIVE | 11c8:02ec, 11f0:0000, 11f0:2012, 11f0:201d |  |
| `caravan_dist_ok` | LIVE | 1110:1d99, 11c8:02ec, 11f0:0000, 11f0:20a8, 11f0:20b0, 11f0:211b |  |
| `caravan_prod_ok` | LIVE | 11f0:20a8 |  |
| `caravan_foreign_bonus` | LIVE | 1110:1d99, 11f0:211b |  |
| `city_is_wonder_open` | LIVE | 1030:7e57, 1170:3181 |  |
| `tech_find_first_owner` | LIVE | 1030:7ece |  |
| `city_prod_is_unit` | LIVE | 1170:05ca |  |
| `city_prod_is_building` | LIVE |  |  |
| `city_prod_is_wonder` | LIVE |  |  |
| `city_choose_production` | STUB | 1110:1d99, 1170:05ca, 1170:1f84, 1170:3181, 1420:5500 | has TODO |
| `city_tile_yield` | STUB | 1030:7c28, 1110:168f, 1110:16c0, 1110:199b, 1110:1c72, 1420:17d6 | has TODO |
| `city_render_worked_tile` | STUB | 1030:79ce, 1030:90a8, 11a8:15ce, 1330:4010 | has TODO |
| `city_prod_unit_gate` | LIVE | 1170:05ca, 1420:1a38, 1420:1a3a, 1420:1a4c |  |
| `city_prod_unit_score` | LIVE | 1170:05ca |  |
| `city_prod_building_score` | LIVE | 1170:05ca |  |
| `city_open_production_view` | STUB | 1030:79ce, 1030:7c28, 1098:124f, 1170:05af | has TODO |
| `city_tile_base_yield` | LIVE | 1030:7c28, 1110:168f, 1110:1c72, 11e8:0dcf, 1420:17d6 |  |
| `city_tile_yield_adjust` | LIVE | 1030:7c28, 1110:199b, 1250:0000 |  |
| `unit_is_diplomat` | LIVE | 11b8:0000, 11f0:1e80, 11f0:1fff |  |
| `unit_type_is_valid` | LIVE | 1170:05ca |  |
| `unit_dispatch_class` | LIVE | 11f0:1e80, 11f0:1ecd, 11f0:1f24, 11f0:2012 |  |
| `unit_order_is_fortify` | LIVE | 11f0:0000 |  |
| `unit_order_is_sentry` | LIVE | 11f0:0000 |  |
| `unit_order_is_found` | LIVE | 11f0:0000 |  |
| `unit_order_is_move` | LIVE | 11f0:0000 |  |
| `city_transfer_treasury` | LIVE | 1038:0000, 1420:43b8 |  |
| `civ_unit_turn_step` | PARTIAL | 1078:1480, 1110:1461, 1128:0000, 11c0:0395, 11c0:12c8, 11f0:0000, 11f0:44d4, 1420:62fc |  |
| `city_pump_result_is_accept` | LIVE | 1170:05ca, 1170:1f84 |  |
| `city_pump_result_is_cancel` | LIVE |  |  |
| `city_production_best_pick` | LIVE | 1170:05ca |  |
| `city_pump_production_dialog` | STUB | 1098:124f, 1170:1f84 | has TODO |
| `unit_choose_delegates_to_player` | LIVE | 1078:1480, 1078:4f0f, 11c8:02ec |  |
| `unit_needs_sentry_idle` | LIVE | 1078:1480 |  |
| `unit_diplomat_needs_goal` | LIVE | 1078:1480, 11c8:02ec |  |
| `unit_ai_best_dir` | LIVE | 1078:4f0f, 1110:1c4b, 1128:0000 |  |
| `unit_ai_reverse_penalty` | LIVE | 1128:0000 |  |
| `unit_seen_index` | LIVE | 11b8:0f26, 11c0:0395 |  |
| `unit_seen_bit` | LIVE |  |  |
| `unit_reveal_minimap_value` | LIVE | 1110:03e9, 11c0:0395, 1210:09b4 |  |
| `unit_sight_far_allowed` | LIVE | 1110:16c0, 11c0:0395 |  |
| `unit_sight_count` | LIVE | 1128:177e |  |
| `unit_find_free_slot` | LIVE | 11c0:12c8, 11c8:00c4, 11c8:0124, 11c8:02ec, 1420:62ff |  |
| `city_find_by_tile` | LIVE | 11c0:0000, 11c8:00c4, 11c8:0124 |  |
| `city_find_nearest` | LIVE | 11c8:0124, 11c8:02ec |  |
| `combat_effective_attack` | LIVE | 1050:0000, 1420:1a40 |  |
| `combat_effective_defense` | LIVE | 1050:0000, 1420:17d5, 1420:1a42 |  |
| `combat_preview_score` | LIVE | 1050:0000, 11c8:003d |  |
| `unit_count_stack` | LIVE | 11c0:1ee1 |  |
| `unit_stack_insert` | LIVE | 1050:1105 |  |
| `unit_find_nearest_at_tile` | LIVE | 11c8:0124, 11c8:01ab, 11c8:02ec |  |
| `unit_spawn_unit` | LIVE | 1110:171f, 11c0:0395, 11c0:12c8, 11c8:0124 |  |
| `unit_queue_is_duplicate` | LIVE | 1078:455c |  |
| `unit_queue_pick_slot` | LIVE | 1078:455c, 1078:4837 |  |
| `unit_queue_shift` | LIVE | 1078:4837 |  |
| `combat_roll_winner` | LIVE | 1050:0000, 11c8:003d |  |
| `combat_should_promote` | LIVE | 1050:0000, 11c8:003d |  |
| `combat_difficulty_scale` | LIVE | 1050:0000 |  |
| `unit_iterate_stack` | LIVE | 11c0:1ee1 |  |
| `unit_stack_defense_score` | LIVE | 1110:16c0, 11c0:1a81, 1420:17d5, 1420:1a42 |  |
| `unit_stack_best` | LIVE | 11c0:1a2c |  |
| `unit_stack_draw_pick` | LIVE | 1110:1516 |  |
| `unit_draw_single_layer` | LIVE | 1110:0dba, 1110:0ff6, 11a8:0cb1, 1208:2061 |  |
| `combat_adjacent_add` | LIVE | 1050:13af, 1078:1480, 11c0:1bc3 |  |
| `combat_adjacent_diplo_flag` | LIVE | 1050:13af |  |
| `diplo_nearest_palace_dist` | LIVE | 1168:12c1, 11c8:02ec, 1420:54fc |  |
| `combat_check_peace_block` | LIVE | 1050:12be, 1220:00d2, 12d8:3a0a |  |
| `unit_stack_draw_needs_sea_search` | LIVE | 1110:0dba, 1110:1516, 11a8:0cb1, 11c0:1a2c |  |
| `city_open_census_view` | STUB | 1018:0000, 1018:015d, 1098:0e0b, 1098:124f, 10d0:0000, 10d0:0956 | has TODO |
| `city_close_census_view` | STUB | 1018:0380, 10d0:0854, 10d0:09a2 | has TODO |
| `city_draw_census_header` | STUB | 1018:015d, 1088:1dd0 | has TODO |
| `census_draw_tech_badge` | STUB | 1018:24c5 | has TODO |
| `census_tech_target` | LIVE | 1018:03f0 |  |
| `census_bar_segment` | LIVE | 11c8:0063 |  |
| `census_bar_width` | LIVE |  |  |
| `census_bar_origin` | LIVE |  |  |
| `census_bar_count` | LIVE |  |  |
| `census_tech_cell` | LIVE | 1018:03f0 |  |
| `census_tech_owner_color` | LIVE |  |  |
| `census_unit_row_y` | LIVE | 1018:067b |  |
| `census_unit_flag_x` | LIVE | 1018:09cc |  |
| `census_unit_has_row` | LIVE | 1018:067b |  |
| `census_terrain_row_y` | LIVE | 1018:09cc |  |
| `census_terrain_cell_x` | LIVE |  |  |
| `census_report_row_y` | LIVE | 1018:1d5b |  |
| `census_report_needs_page` | LIVE | 1018:015d |  |
| `census_report_next_x` | LIVE |  |  |
| `census_building_icon_shown` | LIVE |  |  |
| `census_empty_guard` | LIVE | 1018:1d5b, 1018:202a |  |
| `city_census_gov_title_id` | LIVE | 1018:0000, 1018:015d, 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1040:068e, 1040:1137, 1080:0119, 1088:0000, 1088:0b6d, 1088:1dd0, 1188:123a, 1260:00d6 |  |
| `city_census_header_civ` | LIVE |  |  |
| `city_census_header_pos` | LIVE | 1088:1dd0 |  |
| `census_trade_base` | LIVE | 1018:0be0 |  |
| `census_trade_wonder_total` | LIVE |  |  |
| `census_prod_accum_trade` | LIVE |  |  |
| `census_prod_accum_shields` | LIVE |  |  |
| `census_prod_year_band` | LIVE |  |  |
| `census_caravan_value` | LIVE |  |  |
| `census_building_add` | LIVE |  |  |
| `census_status_prod_kind` | LIVE | 1018:208f, 10d8:002d |  |
| `census_status_shields_total` | LIVE |  |  |
| `pic_pump_select_to_mode` | LIVE | 1030:0000, 1030:9aed, 1048:01cd |  |
| `pic_pump_msg_is_command` | LIVE |  |  |
| `pic_pump_msg_is_paint` | LIVE | 1030:900d |  |
| `ship_part_connected` | LIVE | 1070:11f2 |  |
| `ship_support_percent` | LIVE | 1070:0327, 11c8:0063 |  |
| `ship_support_frac` | LIVE |  |  |
| `ship_power_percent` | LIVE |  |  |
| `ship_power_frac` | LIVE |  |  |
| `ship_flight_tenths` | LIVE |  |  |
| `ship_flight_years` | LIVE |  |  |
| `ship_flight_frac` | LIVE |  |  |
| `ship_success_percent` | LIVE |  |  |
| `ship_arrival_year` | LIVE |  |  |
| `sys_drain_state_words` | LIVE | 11f0:4ea0, 12b0:004c, 1420:1d44 |  |
| `sys_drain_queue_cap` | LIVE | 1008:0a8d, 11e8:02fb |  |
| `sys_drain_queue_reset` | LIVE | 11f0:4ea0 |  |
| `pic_paint_port_id` | LIVE | 1030:900d, 1030:94d2 |  |
| `pic_paint_w` | LIVE |  |  |
| `pic_paint_h` | LIVE |  |  |
| `ship_starfield_count` | LIVE | 1070:12b5 |  |
| `ship_starfield_speed` | LIVE |  |  |
| `ship_starfield_step_x` | LIVE |  |  |
| `ship_starfield_needs_wrap` | LIVE |  |  |
| `ship_starfield_wrap_x` | LIVE |  |  |
| `ship_starfield_wrap_y` | LIVE | 11c8:003d |  |
| `ship_view_msg_is_char` | LIVE | 1070:10cb, 1070:11e2 |  |
| `ship_view_msg_is_command` | LIVE |  |  |
| `ship_view_msg_is_lbuttondown` | LIVE | 1420:5228 |  |
| `ship_view_msg_is_lbuttonup` | LIVE |  |  |
| `ship_view_launch_id` | LIVE |  |  |
| `ship_view_result_is_close` | LIVE | 1070:1018 |  |
| `ship_view_result_is_launch` | LIVE |  |  |
| `ship_pump_view_input` | STUB | 1070:0327, 1070:1018, 1070:10cb | has TODO |
| `pic_finish_paint_step_count` | LIVE | 1030:94d2, 1030:95c2, 1030:96ad, 1030:982f |  |
| `pic_finish_paint_step_is_caption` | LIVE |  |  |
| `pic_finish_paint` | STUB | 1030:900d, 1030:94d2, 1030:96ad, 1030:982f, 1098:1104, 10d0:0854, 1100:09a9 | has TODO |
| `ship_view_dispatch` | LIVE | 1070:10cb, 1070:11e2, 1608:01a8 |  |
| `ship_command_is_launch` | LIVE | 1070:110f |  |
| `ship_lbuttonup_is_armed` | LIVE | 1070:1129 |  |
| `ship_hit_sets_armed` | LIVE | 1070:117f, 1608:00b0 |  |
| `pic_arrow_dir` | LIVE | 1030:96ad |  |
| `pic_arrow_row_count` | LIVE |  |  |
| `pic_arrow_is_greyed` | LIVE |  |  |
| `pic_caption_text_dx` | LIVE | 1030:982f |  |
| `pic_dispatch_is_caption` | LIVE | 1030:95c2, 1030:982f |  |
| `pic_dispatch_is_arrow` | LIVE | 1030:96ad |  |
| `pic_dispatch_arrow_variant` | LIVE |  |  |
| `pic_pump_owner_is_shared` | LIVE | 1030:0000, 1030:900d, 1030:9aed, 1048:01cd |  |

### `src/gfx_indexed.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `gfx_indexed_to_surface` | LIVE | 10d0:0854, 1190:0a29 |  |
| `gfx_indexed_to_texture` | LIVE |  |  |
| `gfx_probe_rsc_image` | LIVE | 1190:0a29 |  |
| `gfx_probe_head_hex` | LIVE | 1000:2928, 1190:0a29 |  |
| `gfx_decode_rle` | LIVE | 10f8:040a, 10f8:05da |  |
| `gfx_rle_expand_cvpc` | LIVE | 10f8:040a, 10f8:05da |  |
| `gfx_decode_raw` | LIVE |  |  |
| `gfx_godpal_count` | LIVE |  |  |
| `gfx_godpal_get` | STUB | 1090:0376, 1090:0488, 1200:00a8, 1420:9966 | has TODO |
| `gfx_try_fail_str` | LIVE |  |  |
| `gfx_expand_method_str` | LIVE |  |  |
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
| `font_glyph_to_surface` | LIVE | 1008:0e99 |  |
| `gfx_try_custom_stub` | LIVE |  |  |

### `src/halloffame.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `hof_get_u16le` | LIVE |  |  |
| `hof_put_u16le` | LIVE |  |  |
| `hof_name_len` | LIVE |  |  |
| `hof_weighted_score` | LIVE |  |  |
| `hof_diff_names` | LIVE |  |  |
| `HoF_Clear` | LIVE | 1080:0000 |  |
| `HoF_Load` | LIVE | 1080:0023, 1420:14e8 |  |
| `HoF_Save` | LIVE | 1080:0092, 1420:14f5 |  |
| `HoF_Show` | STUB | 1080:0119, 1098:0ed1, 1098:20f4, 1420:1502, 1420:153e, 1420:1572 | has TODO |
| `hof_format_slot` | LIVE | 1080:0119, 1080:0435, 1080:05b0, 1098:0ed1, 1098:20f4, 1100:03ff, 11c8:03ac, 1420:1502, 1420:1522, 1420:153e, 1420:1589 |  |
| `HoF_TryInsert` | LIVE | 1080:0595, 1080:05b0 |  |
| `hof_encode` | LIVE |  |  |
| `hof_decode` | LIVE |  |  |
| `HoF_VictoryCeremony` | STUB | 1080:06ff, 10d0:0854, 1150:019e | has TODO |
| `City_ShowTopFive` | STUB | 1080:099b, 1420:159c, 1420:5500 | has TODO |
| `Hof_CalcInterimScore` | LIVE | 1030:0000, 1040:1137, 1080:0595, 1080:05b0, 1088:108a |  |
| `Hof_CalcFinalScore` | PARTIAL | 1018:0000, 1018:0380, 1030:0000, 1030:84be, 1040:068e, 1040:1137, 1040:1b51, 1088:0000, 1088:0b6d, 10d8:01fa, 11e8:0504 | has TODO |
| `hof_score_layout_value` | LIVE | 1040:1137, 11c8:0063 |  |
| `hof_score_layout_fit` | LIVE |  |  |
| `hof_final_accum` | LIVE | 1040:1137, 12a8:0022 |  |
| `hof_final_cap` | LIVE |  |  |
| `hof_score_bar_width` | LIVE | 11e8:0504 |  |
| `hof_score_icon_advance` | LIVE | 1040:1b51 |  |
| `Hof_PlayEndAnimation` | STUB | 1088:0b6d, 1188:07ad | has TODO |
| `Hof_ShowEndDialog` | STUB | 1088:0b6d, 1180:0000 | has TODO |
| `hof_load` | LIVE | 1080:0023 |  |
| `hof_save` | LIVE | 1080:0092 |  |
| `hof_show` | LIVE | 1080:0119 |  |
| `hof_try_insert` | LIVE | 1080:0595 |  |
| `hof_victory_ceremony` | LIVE | 1080:06ff |  |
| `city_show_top_five` | LIVE | 1080:099b, 1420:159c |  |
| `hof_tech_icon_skipped` | LIVE | 1030:84be |  |
| `hof_tech_icon_index` | LIVE |  |  |
| `hof_score_icon_index` | LIVE | 1040:1137 |  |
| `hof_draw_score_icon_step` | STUB | 1040:1b51, 11a8:0cb1, 11e8:0674 | has TODO |
| `hof_draw_tech_icon` | STUB | 1030:84be, 10d8:01b0, 11a8:0cb1, 11e8:0d81 | has TODO |

### `src/main.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `main` | LIVE |  |  |

### `src/mainmenu.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `mainmenu_run` | STUB | 1000:36b2, 11e0:0221, 1220:0156, 1420:435b |  |
| `mainmenu_dispatch` | PARTIAL | 1108:0243, 1130:000e | has TODO |
| `menu_dispatch_count` | LIVE | 1048:01e4 |  |
| `menu_dispatch_find` | LIVE | 1048:01cd |  |
| `menu_dispatch_id_at` | LIVE |  |  |
| `menu_dispatch_target_at` | LIVE |  |  |
| `menu_command_group` | LIVE |  |  |
| `menu_dispatch_command` | STUB | 1048:01cd |  |
| `menu_toggle_show_cmd` | LIVE | 1210:1134, 1218:077d |  |
| `menu_toggle_view_window` | LIVE | 1048:02af, 1208:0af1, 1210:025b, 1218:0386, 12b0:0004, 12b0:001c, 12b0:0034 |  |

### `src/map.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `map_buffer` | LIVE |  |  |
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
| `map_timer_interval_ms` | LIVE | 1008:0000, 1008:0329, 1208:0054, 1208:0af1, 1208:0e7b |  |
| `map_shutdown` | NOOP |  |  |
| `wdwstatus_calc_upper_rect` | STUB | 1088:1479, 1218:0956 | has TODO |
| `wdwstatus_calc_lower_rect` | STUB | 1088:1536, 1218:0a06 | has TODO |
| `wdwstatus_is_dirty` | LIVE | 1218:063e, 1218:0801 |  |
| `wdwstatus_frame_metrics` | LIVE | 1218:0956 |  |
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
| `tile_type_name` | LIVE | 1110:16c0, 11e8:0dcf |  |
| `tile_move_cost` | LIVE | 1128:0000, 1420:17d4 |  |
| `map_step_dist` | LIVE | 11c8:025d |  |
| `map_wrap_step_dist` | LIVE | 1180:1096, 11c8:025d, 11c8:02ec |  |
| `map_check_tile_bounds` | LIVE | 1110:168f, 11e8:0dcf |  |
| `map_check_tile_improved` | LIVE | 1030:7c28, 1110:16c0, 1110:1c72 |  |
| `map_get_occupancy_flags` | LIVE | 1030:7c28, 1110:199b |  |
| `map_get_continent_id` | LIVE | 1110:1d99, 1110:1dbd, 1170:05ca |  |
| `map_sign_step` | LIVE | 1078:4f0f, 1110:1c4b, 1128:0000 |  |
| `map_clear_occupancy_bits` | LIVE | 1110:1b36, 11e8:0e19 |  |
| `map_wrap_x` | LIVE | 1110:0dba, 1128:177e, 11c0:0395, 11c0:2437 |  |
| `map_unit_slot_from_tile` | LIVE | 1110:18ce |  |
| `map_tile_owner_from_tile` | LIVE | 1110:16ee, 11c8:00c4, 1420:5507 |  |
| `map_scan_zoc_detail` | LIVE | 1110:1461, 1110:16c0, 1110:18ce, 11c0:23fd, 11c0:2437 |  |
| `map_mirror_occupancy` | LIVE | 1110:1a2f, 11e8:0e19 |  |
| `map_set_tile_owner` | LIVE | 1110:171f |  |
| `map_place_unit_marker` | LIVE | 1050:1105, 1110:1767 |  |
| `map_set_view_origin` | LIVE | 1208:20b1, 1208:213b |  |
| `map_clamp_view_x` | LIVE | 1128:177e, 1208:213b |  |
| `map_clamp_view_y_plain` | LIVE | 1170:3181, 1208:213b |  |
| `map_clamp_view_y_scroll` | LIVE | 1170:3181, 1208:213b, 1408:00c4 |  |
| `map_clamp_view_scroll` | STUB | 1000:137f, 1208:213b | has TODO |
| `map_paint_tiles_clipped` | STUB | 10d0:0854, 1100:0685, 1110:1461, 1208:1534, 1208:1f64, 1330:4010, 1408:00c6 | has TODO |
| `map_repaint_view_shuffled` | STUB | 1100:0685, 11c8:003d, 1208:1866 | has TODO |
| `map_draw_city_labels` | STUB | 1098:0ed1, 1100:0685, 1208:1f64 | has TODO |
| `map_paint_start` | LIVE | 1208:1534, 1330:4010 |  |
| `map_paint_end` | LIVE |  |  |
| `map_paint_tile_px` | LIVE |  |  |
| `map_tile_needs_blank` | LIVE | 1100:0876, 1110:0448, 1110:04d0, 1110:1461, 1128:177e, 1208:1534, 1208:1866, 1408:00cc |  |
| `map_repaint_mode_is_present` | LIVE | 10d0:0854, 10d0:09a2, 11c8:003d, 1208:1866 |  |
| `map_repaint_mode_is_shuffled` | LIVE | 11c8:003d |  |
| `map_repaint_mode_is_rowmajor` | LIVE | 1208:1f64 |  |
| `map_shuffle_grid_count` | LIVE | 1208:1866, 1408:00c6 |  |
| `map_shuffle_tile_at` | LIVE |  |  |
| `map_shuffle_px` | LIVE | 1408:00c6 |  |
| `map_refresh_tile_px` | LIVE | 1110:0448, 1128:177e, 1330:4010, 1408:00c6 |  |
| `map_city_label_box` | LIVE | 1100:0685, 1208:1f64 |  |
| `map_city_label_res_id` | LIVE | 1000:36b2, 1048:0101, 1098:0ed1, 1420:5507 |  |
| `map_tile_cache_capacity` | LIVE | 1110:0127, 1110:01a3 |  |
| `map_tile_cache_slot_len` | LIVE |  |  |
| `map_tile_cache_match` | LIVE | 1110:030c |  |
| `map_tile_cache_is_empty` | LIVE | 1110:0127 |  |
| `map_tile_cache_find` | LIVE | 1110:030c |  |
| `map_tile_cache_victim` | LIVE | 1110:01a3 |  |
| `map_tile_cache_dispose` | LIVE | 1110:0127, 1330:0002, 1330:0004, 1420:2e42 |  |
| `map_tile_cache_next_tick` | LIVE | 1110:01a3, 1110:030c |  |
| `smmap_click_tile_delta` | LIVE | 1210:056e, 1420:b3cc |  |
| `smmap_center_target` | LIVE | 1110:03e9, 1330:400e, 1330:4010 |  |
| `smmap_click_scale` | LIVE |  |  |
| `corridor_abs` | LIVE |  |  |
| `path_corridor_gate_ok` | LIVE | 1128:16d0 |  |
| `path_corridor_window_ok` | LIVE | 1128:10de, 1128:177e |  |
| `path_corridor_sea_ok` | LIVE | 1028:12c6, 1128:10de |  |
| `path_corridor_step_cost` | LIVE | 1420:17d4, 1420:1a3c |  |
| `path_corridor_queue_adv` | LIVE | 1128:10de |  |
| `path_corridor_should_relax` | LIVE | 1128:10de |  |
| `path_continent_link_result` | LIVE | 1128:16d0 |  |
| `map_get_ocean_or_cont_val` | LIVE | 1110:16c0, 1110:1d99, 1110:1dbd, 1420:43b4 |  |

### `src/newgame.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `newgame_pick_difficulty` | STUB | 10e8:0000, 1220:0156, 12d8:42f6, 1420:2a5a | has TODO |
| `newgame_pick_competition` | STUB | 10e8:0000, 1220:0156, 12d8:3b9a, 12d8:42f8, 1420:2aa9, 1420:2ac1, 1420:2ac3 | has TODO |
| `newgame_civ_count_from_index` | LIVE | 12d8:3b9a |  |
| `newgame_alive_mask` | LIVE | 12d8:42f8 |  |
| `newgame_pick_tribe` | STUB | 10e8:0000, 11f8:0080, 1220:0156, 12d8:4300, 1420:2ad2, 1420:2ae5 | has TODO |
| `tribe_get_name_list` | STUB | 10e8:0000, 10e8:2d46, 1100:078a, 11f8:0080, 1420:2ad2, 1420:2ae5 |  |
| `newgame_customize_world` | STUB | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6, 10e8:0000, 1220:0156, 1220:16fd | has TODO |
| `newgame_custom_world_modal` | STUB | 1058:06ab, 1220:0156, 1220:16fd | has TODO |
| `newgame_finalize` | PARTIAL | 1028:0000, 10e8:0000, 1150:00c9, 12d8:3b9a, 12d8:42f6, 12d8:42f8, 12d8:42fa, 12d8:4300 | has TODO |
| `menu_prompt_nation_needed` | LIVE | 1070:1f58 |  |
| `menu_prompt_nation_count` | LIVE | 11f8:0080, 12d8:03da |  |
| `menu_prompt_nation_avail_mask` | LIVE |  |  |
| `menu_prompt_nation_choice_valid` | LIVE | 1220:00d2 |  |
| `newgame_difficulty_from_index` | LIVE | 10e8:0000, 12d8:42f6, 1420:2a5a |  |
| `newgame_competition_resolve` | LIVE | 11c8:0063, 12d8:3b9a, 1420:2aa9 |  |
| `newgame_player_civ_resolve` | LIVE | 11c8:003d, 11f8:0080 |  |
| `newgame_start_year_raw` | LIVE | 1000:2928, 1088:1dd0, 12d8:42fa |  |
| `newgame_start_year_signed` | LIVE |  |  |
| `newgame_wizard_run` | PARTIAL | 10e8:0000, 11e0:0000 |  |
| `newgame_custom_result_is_back` | LIVE | 10e8:0000, 1220:16fd |  |
| `newgame_wizard_step_back` | LIVE |  |  |
| `newgame_custom_settings_valid` | LIVE | 1108:0243 |  |
| `newgame_custom_help_for_row` | LIVE | 10e0:0000, 1108:0243, 1420:2e28, 1420:2e34 |  |
| `newgame_show_custom_help_param` | LIVE | 10e0:0000 |  |
| `newgame_show_custom_help_next` | LIVE |  |  |
| `newgame_civ_note_early_needed` | LIVE | 10e0:077f |  |
| `newgame_civ_note_player_needed` | LIVE | 10e0:07e8 |  |
| `newgame_show_custom_help` | STUB | 10e0:0000, 10e0:03a2 | has TODO |
| `newgame_custom_help_dialog` | STUB | 1098:124f, 10e0:03a2, 1100:03ff | has TODO |

### `src/rsc_loader.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `rsc_strerror` | PARTIAL |  |  |
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
| `rsc_path` | LIVE |  |  |
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
| `rsc_get_blob_string` | LIVE | 1048:0101, 10e8:03c6, 1100:0053, 1100:078a, 1190:09ed, 1190:0aea, 1190:1953, 1198:00a3, 11e0:0221, 11f8:0080, 11f8:00c5, 11f8:014a |  |
| `rsc_mgr_new` | LIVE | 1420:b2bc |  |
| `rsc_free_res_list` | LIVE | 1190:0c92, 1190:10e6 |  |
| `rsc_mgr_dispose` | LIVE | 1190:0c92, 1190:10e6, 1190:113f |  |
| `rsc_mgr_free` | LIVE | 1190:0c92 |  |
| `rsc_add_file` | LIVE | 1190:1692 |  |
| `rsc_add_type` | LIVE | 1190:13a8, 1190:1533 |  |
| `rsc_new_resource` | LIVE | 1190:12ee |  |
| `rsc_find_file` | LIVE | 1190:1180 |  |
| `rsc_find_type` | LIVE | 1190:121d, 1190:1533 |  |
| `rsc_find_by_handle` | LIVE | 1190:0ee5 |  |
| `rsc_get_type_count` | LIVE | 1190:0978 |  |
| `rsc_get_res_indexed` | LIVE | 1190:0a29 |  |
| `rsc_get_by_id` | LIVE | 1190:0aea |  |
| `rsc_get_by_name` | LIVE | 1190:0bbe, 1190:0e72 |  |
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
| `rsc_add_file_disk` | LIVE | 1000:272a, 1190:1570, 1190:1692 |  |
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
| `rsc_gdat_row` | LIVE | 1028:0fb4, 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 |  |
| `rsc_kdat_row` | LIVE |  |  |
| `rsc_text_get_all` | PARTIAL |  |  |
| `rsc_text_search_all` | PARTIAL |  |  |
| `rsc_gdat_decode_row` | LIVE | 1028:0fb4, 1190:1953, 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 |  |
| `rsc_kdat_decode_row` | LIVE |  |  |
| `res_append_pascal` | LIVE | 1048:0101, 1100:078a, 11f8:014a |  |
| `res_compare_pascal` | LIVE | 11f8:0207 |  |
| `res_copy_pascal` | LIVE | 11f8:00fc |  |
| `res_get_string_copy` | STUB | 1100:0053, 1100:078a, 11f8:0080 | has TODO |
| `res_get_string_raw` | STUB | 11f8:00c5 | has TODO |
| `res_blob_entry_kind` | LIVE | 1100:078a |  |

### `src/save.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `save_prompt_open` | STUB | 1130:0075 | has TODO |
| `save_prompt_save` | STUB | 1130:0194 | has TODO |
| `save_init_filespec` | LIVE | 1000:36b2, 1000:3722, 1080:0023, 1080:0092, 1140:0095, 1420:14e8, 1420:14f5 |  |
| `save_load_dialog` | STUB | 10b0:0000, 10f8:0000, 1130:000e, 1130:0075, 11e0:0221 | has TODO |
| `save_save_dialog` | STUB | 1098:26d4, 1130:0194, 1180:0000, 1198:00a3 | has TODO |
| `save_save_game` | STUB | 1000:2928, 1000:363e, 1098:26d4, 10f8:02bd, 1198:00a3 | has TODO |
| `save_sanitize_filename` | LIVE | 1000:36b2, 1180:0000, 1198:0000, 1198:00a3 |  |
| `save_file_open` | LIVE | 1000:2ad9, 10f8:0000, 10f8:02bd, 1140:0139 |  |
| `save_file_size` | LIVE | 10f8:0000, 1140:0295 |  |
| `save_file_read` | LIVE | 1080:0023, 10f8:0000, 1140:019d |  |
| `save_file_write` | LIVE | 10f8:02bd, 1140:0245 |  |
| `save_file_close` | LIVE | 1000:1f7c, 1140:037c |  |
| `file_open_wrapper` | LIVE | 1000:2ad9, 1140:0139 |  |
| `file_close_wrapper` | LIVE | 1000:1f7c, 1140:037c |  |
| `file_seek_wrapper` | LIVE | 1000:1f7c, 1000:272a, 1000:2ad9, 1140:0295, 1190:0fda |  |
| `save_serialize` | LIVE | 10f8:02bd, 10f8:090c, 1420:2cac |  |
| `save_deserialize` | LIVE | 1028:162c, 10f8:0000, 1140:0139, 1140:019d, 1140:0295 |  |
| `save_pack` | STUB | 10f8:02bd, 10f8:05da |  |
| `save_unpack` | STUB | 10f8:0000, 10f8:040a |  |
| `save_pack_buffer` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
| `save_unpack_buffer` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
| `save_build_filename` | LIVE | 1000:2928, 1000:363e, 1000:36b2, 1080:0119, 1198:0000, 1198:00a3, 1420:173c |  |
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
| `save_ctx_find` | LIVE |  |  |
| `save_live_check` | LIVE |  |  |
| `save_live_copy_one` | LIVE |  |  |
| `save_xfer_words_live` | LIVE | 10f8:0f40, 1310:0008 |  |
| `save_xfer_block_live` | LIVE | 10f8:0fcf |  |
| `save_xfer_array_live` | LIVE | 10f8:1035 |  |
| `save_xfer_strided_live` | LIVE | 10f8:10bd |  |
| `save_marshal_fields_live` | LIVE | 10f8:090c |  |
| `replay_year_from_turn` | LIVE | 1180:0f7c |  |
| `replay_append_log` | LIVE | 11c0:3beb, 1388:0000, 1390:03af |  |
| `replay_find_empty_slot` | LIVE | 1180:0fe5, 1390:00a0 |  |
| `replay_update_slot_owner` | LIVE | 1180:1044 |  |
| `replay_wrap_step_dist` | LIVE | 11c8:025d, 11c8:02ec |  |
| `replay_find_nearest_slot` | LIVE | 1180:1096, 11c8:02ec |  |
| `replay_tile_window` | LIVE | 1180:110c, 11c8:0063 |  |
| `replay_map_text_cell` | LIVE | 1110:16c0, 1110:199b, 1180:137f |  |
| `replay_mode_is_record` | LIVE | 1180:0000 |  |
| `replay_mode_needs_view` | LIVE | 1180:017a |  |
| `replay_entry_len` | LIVE | 1180:02d3 |  |
| `replay_show_entry` | STUB | 1098:0e0b, 1180:0e78 | has TODO |
| `replay_open_or_play` | STUB | 1098:26d4, 1140:0139, 1180:0000, 1180:017a, 1180:0297, 1180:02d3 | has TODO |
| `replay_show_civ_destroyed` | STUB | 10d8:046e, 1150:00c9, 1180:17e9, 11a8:0cb1 | has TODO |
| `save_needs_ocean_rebuild` | LIVE | 1028:162c, 10f8:0000 |  |

### `src/sys.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `rng_seed` | LIVE | 1000:1716 |  |
| `rng_next_raw` | LIVE | 1000:16fe, 1000:1729 |  |
| `rng_range` | LIVE | 11c8:003d |  |
| `rng_init` | LIVE | 11c8:0000 |  |
| `util_clamp` | LIVE | 11c8:0063 |  |
| `util_progress` | LIVE |  |  |
| `debug_print` | LIVE | 1000:19d8, 1000:1a3c, 1000:2c9c, 1000:2e85, 1190:17d0, 1420:4cd0 |  |
| `debug_print_line` | LIVE | 1000:2cd8, 1000:2e85, 1000:3088, 1000:3722, 1190:17d0, 1420:4cd0 |  |
| `file_write_buffered` | STUB | 1000:2c9c, 1000:2cba, 1000:2e85, 1000:3fc4, 1420:4cd0 | has TODO |
| `file_put_char_buffered` | STUB | 1000:2152, 1000:2cd8, 1000:3fc4, 1420:4cd0 | has TODO |
| `file_emit_char` | STUB | 1000:2cba, 1000:2cd8, 1000:2e85 | has TODO |
| `dos_map_error` | STUB | 1000:151a, 1420:4e7c | has TODO |
| `dos_is_char_device` | STUB | 1000:1566, 1000:1750, 1000:3fc4, 1420:4fc4, 1420:4fc8 | has TODO |
| `dos_seek` | STUB | 1000:161c | has TODO |
| `dos_read` | STUB | 1000:1750, 1000:33c0, 1140:019d, 1420:4fc4 | has TODO |
| `dos_unlink` | STUB | 1000:19a6 | has TODO |
| `dos_file_attr` | STUB | 1000:1f5c, 1000:2ad9 | has TODO |
| `dos_close` | STUB | 1000:1f7c, 1000:1fa6 | has TODO |
| `dos_is_eof` | STUB | 1000:201e | has TODO |
| `dos_create` | STUB | 1000:2aac, 1000:2ad9 | has TODO |
| `dos_truncate` | STUB | 1000:2ac7, 1000:2ad9 | has TODO |
| `dos_open` | STUB | 1000:2ad9, 1000:2c4a | has TODO |
| `dos_write` | STUB | 1000:3e70, 1000:3fc4, 1140:0245, 1420:4fc8 | has TODO |
| `dos_ioctl` | STUB | 1000:4252 | has TODO |
| `dos_get_current_drive` | LIVE | 1000:1252 |  |
| `dos_get_current_drive_index` | LIVE | 1000:13e6 |  |
| `dos_get_current_dir` | LIVE | 1000:139c |  |
| `sys_get_tick_scaled` | LIVE | 1100:02e1 |  |
| `sys_build_current_dir_path` | LIVE | 1000:2826 |  |
| `sys_init_paths` | LIVE | 1228:0002, 1228:0004 |  |
| `sys_current_drive` | LIVE | 1228:0002 |  |
| `sys_base_path` | LIVE | 1228:0004 |  |
| `sys_yield` | LIVE | 1098:0157, 10b0:0000 |  |
| `sys_pump_messages` | LIVE | 10b0:0000, 1108:0000, 11c0:0ec2, 11e8:02fb, 1328:0012 |  |
| `sys_wait_for_input` | STUB | 1018:0380, 1018:067b, 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1038:0c1d, 1080:0119, 1080:06ff, 10e8:0000, 1180:0000, 11f0:468c, 11f0:4ea0 | has TODO |
| `main_pump_message` | STUB | 1008:0329, 1008:0a8d, 1098:0157, 1098:02d1, 1108:0000, 11e8:02fb, 1208:0000, 1608:0134, 1608:013c, 1608:0140 | has TODO |
| `main_key_to_res_char` | LIVE | 1098:02d1, 1208:0000, 1420:1400, 1420:142c |  |
| `sys_get_client_rect` | STUB | 1000:137f, 1218:0956, 1218:0a06 | has TODO |
| `status_measure_row` | STUB | 1088:1460, 1098:0dda, 1138:0269, 11f0:4754, 1220:0b88 | has TODO |
| `sys_check_runtime_memory` | STUB | 1088:0000, 11e8:0075, 1420:43b4, 1420:4473, 1420:44ff | has TODO |
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
| `float_matherr_name` | LIVE | 1420:4aae, 1420:4aca |  |
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
| `util_parse_int` | LIVE | 1000:1ed2 |  |
| `debug_log_trace` | LIVE | 1148:003c |  |
| `mem_dispose_ptr` | STUB | 1008:0329, 1028:0000, 10f8:02bd, 1100:060b | has TODO |
| `mem_dispose_handle` | STUB | 1100:0652 | has TODO |
| `mem_realloc_handle` | STUB | 1100:030f | has TODO |
| `sys_wait_msg_is_key` | LIVE | 1008:0d68, 1018:0380, 1018:067b, 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1038:0c1d, 1080:0119, 1080:06ff, 1098:0157, 10e8:0000, 1180:0000, 1180:02d3, 1180:152f, 11f0:4ea0 |  |
| `sys_wait_msg_is_lbutton` | LIVE |  |  |
| `sys_wait_scale_coord` | LIVE |  |  |
| `sys_wait_cursor_id` | LIVE |  |  |
| `file_table_first_fd` | LIVE | 1000:1fc6 |  |
| `file_table_entry_len` | LIVE |  |  |
| `file_table_fd_off` | LIVE |  |  |
| `file_entry_is_open` | LIVE | 1000:1f7c, 1000:1fa6, 1000:2098 |  |
| `file_table_slot_rel` | LIVE | 1000:1fc6 |  |
| `sys_path_guard_should_set` | LIVE | 1008:0bbf, 10b8:0aa8, 10b8:0c1c, 1150:00c9, 1228:0002 |  |
| `sys_drive_ready_ok` | LIVE | 1000:1184 |  |
| `sys_path_guard_abort` | LIVE | 1000:116a, 1008:0bbf, 1150:00c9 |  |
| `dos_chdir_raw` | STUB | 1000:116a, 1000:151a, 1008:0bff, 1228:0004 | has TODO |
| `dos_select_drive` | STUB | 1000:1184, 1000:1252, 1000:1266 | has TODO |
| `file_temp_env_name` | LIVE | 1000:1648, 1420:4ed6, 1420:4eda |  |
| `file_temp_suffix` | LIVE | 1000:363e, 1420:4eda |  |
| `file_build_temp_path` | LIVE | 1000:1210, 1000:157a, 1000:1648, 1000:16a5, 1000:363e |  |
| `sys_pump_mode_select` | LIVE | 1108:0000 |  |
| `sys_pump_scale_coord` | LIVE | 1038:0c1d, 1108:0000 |  |
| `sys_runtime_warn_style_first` | LIVE | 11e8:0075, 1420:44f7 |  |
| `sys_runtime_warn_style_final` | LIVE | 1420:4578 |  |
| `sys_runtime_warn_title_first` | LIVE | 1420:44f7, 1420:4578 |  |
| `sys_runtime_warn_body_first` | LIVE | 1420:4473 |  |
| `sys_runtime_warn_title_final` | LIVE | 1420:4578 |  |
| `sys_runtime_warn_body_final` | LIVE | 1420:44ff |  |
| `sys_runtime_check_order` | LIVE | 1088:0000 |  |

### `src/title.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `title_play_intro` | STUB |  | has TODO |
| `title_play_anim_loop` | STUB | 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1030:79ce, 1038:0000, 1038:0d09, 1040:068e, 1040:1137, 1080:099b, 1098:124f, 10b8:079d, 1170:0000, 1170:05af, 1170:05ca, 1188:0db8, 1188:123a, 11b8:0f26 | has TODO |
| `title_decode_frame` | STUB | 1018:015d, 1088:1dd0, 1138:0000, 11b0:0000, 11b8:14e5 | has TODO |
| `pic_decompress_chunk` | LIVE | 10f8:040a, 10f8:05da, 1420:23ce |  |
| `pic_read_header` | STUB | 10d0:0956, 1140:019d, 1420:2300, 1420:2372 | has TODO |
| `title_load_opening_frame` | STUB | 1030:0000, 10d0:0956, 1190:0000 | has TODO |
| `title_poll_skip` | LIVE |  |  |
| `title_resolve_asset` | LIVE |  |  |
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
| `gfx_brush_is_solid` | LIVE | 1098:00b5 |  |
| `gfx_color_table_kind` | LIVE | 1098:01b0 |  |
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
| `art_progress_fill_x` | LIVE | 10b8:062d |  |
| `video_unit_frame_index` | LIVE | 1110:0dba, 1110:0ff6 |  |
| `video_unit_tile_box` | LIVE | 1100:0685, 1128:177e, 1208:2061 |  |
| `video_draw_unit_sprite` | STUB | 10d0:0854, 10d0:0956, 1110:0dba, 1110:0ff6, 11a8:0cb1 | has TODO |
| `video_clip_rect` | LIVE | 1100:0685, 1110:0dba, 1208:2061 |  |
| `unit_flag_icon_index` | LIVE | 1018:067b, 1110:0ff6, 11c8:008a |  |
| `gfx_icon_cell_box` | LIVE | 1100:0685, 11e8:0674 |  |
| `gfx_draw_icon_clipped` | STUB | 10d0:0854, 1100:0685, 11a8:0cb1, 11e8:0674 | has TODO |
| `gfx_present_tile_rect` | STUB | 10d0:0854, 1100:0685, 11e8:07c3 | has TODO |
| `city_draw_tile_view` | STUB | 1100:0685, 1110:04d0, 11a8:0cb1 | has TODO |
| `art_load_frame` | STUB | 10b8:079d, 10b8:0aa8, 10b8:0e00, 10b8:0e36, 10b8:11bc, 10b8:158c, 10d0:0000, 10f8:040a | has TODO |
| `gfx_color_remap_match` | LIVE | 10d0:09ee |  |
| `gfx_remap_stride` | LIVE |  |  |
| `gfx_remap_index` | LIVE | 10d0:09ee |  |
| `gfx_tile_box` | LIVE | 10d0:0854, 1100:0685, 11e8:07c3 |  |
| `gfx_bar_fill_extent` | LIVE | 11e8:0504 |  |
| `gfx_bar_fill_cap` | LIVE |  |  |
| `art_port_is_even_width` | LIVE | 10b8:0e36, 10d0:0cfa |  |
| `art_port_padded_width` | LIVE |  |  |
| `census_badge_table_off` | LIVE | 1018:24c5, 11a8:0cb1, 11a8:0f56 |  |
| `census_badge_size` | LIVE |  |  |
| `gfx_scale_icon_dim` | LIVE | 11e8:0504, 11e8:0d81, 11e8:0da8, 1220:0f2f |  |
| `gfx_bar_fill_color` | LIVE | 11e8:0504 |  |
| `gfx_bar_brush_is_alt` | LIVE |  |  |
| `gfx_bar_range_needs_fill` | LIVE | 11e8:0912 |  |
| `gfx_bar_range_width` | LIVE |  |  |
| `gfx_bar_range_height` | LIVE |  |  |
| `gfx_port_select_kind` | LIVE | 1220:0f2f, 1260:002c, 12b0:0004, 12b0:0034 |  |
| `gfx_port_dialog_coord` | LIVE |  |  |
| `gfx_port_palette_word` | LIVE | 10d0:0262 |  |
| `gfx_port_slot_after_clear` | LIVE | 10d0:0172, 10d0:09a2 |  |
| `gfx_port_clear_active` | LIVE | 10d0:0172, 10d0:09a2 |  |
| `gfx_port_bitmap_pitch` | LIVE | 10b8:0e36, 10d0:0000, 10d0:0cfa |  |
| `gfx_port_is_mono` | LIVE | 1098:00b5, 1098:201d, 10d0:0262 |  |
| `gfx_tech_bg_resolve` | LIVE | 11a8:0f56, 11a8:13c0 |  |
| `gfx_tech_needs_scale` | LIVE | 11a8:0000, 11a8:002c |  |
| `gfx_tech_palette_pick` | LIVE | 11a8:0cb1 |  |
| `palette_copy_enabled` | LIVE | 1098:201d |  |
| `palette_copy_range_len` | LIVE |  |  |
| `gfx_icon_buffer_size` | LIVE | 11a8:0054 |  |
| `gfx_icon_scaled_dim` | LIVE | 11a8:13c0, 11e8:0d81 |  |
| `gfx_icon_scale_is_unit` | LIVE | 1040:1137, 11a8:002c, 11a8:0f56, 13a0:0000 |  |
| `gfx_icon_copy_is_fast` | LIVE | 11a8:13c0 |  |
| `gfx_icon_row_pad` | LIVE |  |  |
| `gfx_alloc_icon_buffer` | PARTIAL | 1110:04d0, 11a8:0054, 11a8:00d1, 11a8:0f56, 11e8:0337 | has TODO |
| `gfx_copy_icon_bits` | STUB | 11a8:0328, 11a8:0450, 11a8:0aa8, 11a8:13c0 | has TODO |
| `gfx_blit_clip_len` | LIVE | 11a8:0000, 11a8:0aa8 |  |
| `gfx_copy_span` | LIVE | 11a8:0328 |  |
| `gfx_blit_scaled_is_direct` | LIVE | 1040:1137, 11a8:002c, 11a8:0450 |  |
| `gfx_nearest_src` | LIVE | 11a8:13c0 |  |
| `gfx_icon_mask_row_bytes` | LIVE | 1030:925b, 1068:06af, 10d0:09ee, 10e8:2d46, 1110:04d0, 1110:0ff6, 1118:0000, 11a8:00fe, 11a8:022f, 11a8:02ad, 11a8:076f, 11a8:0cfd, 11a8:0d6e, 11a8:0f56 |  |
| `gfx_icon_transparent_match` | LIVE | 11a8:076f, 11a8:0d6e |  |
| `gfx_icon_content_bounds` | LIVE | 11a8:00fe |  |
| `gfx_fill_icon_rect` | LIVE | 11a8:02ad |  |
| `gfx_icon_copy_clipped` | LIVE | 11a8:076f |  |
| `gfx_blit_rop_direct` | LIVE | 11a8:0f56, 11a8:1147, 11a8:12eb |  |
| `gfx_blit_rop_mask_step` | LIVE | 11a8:11bb, 11a8:11dc, 11a8:11fd |  |
| `gfx_mask_needs_bitmap` | LIVE |  |  |
| `gfx_icon_registry_groups` | LIVE | 1008:075b |  |
| `gfx_icon_registry_group_count` | LIVE |  |  |
| `gfx_icon_registry_total` | LIVE |  |  |
| `gfx_icon_registry_free` | LIVE | 1008:062f, 1008:075b |  |
| `gfx_store_dc` | STUB | 1098:1104, 1100:09a9 | has TODO |
| `gfx_dc_push` | STUB |  | has TODO |
| `gfx_dc_pop` | STUB | 10d0:0854, 10d0:09a2 | has TODO |
| `gfx_store_tick_needs_save` | LIVE | 10b0:0461 |  |
| `gfx_store_modal_needs_save` | LIVE | 1030:0000, 1098:124f, 1208:0af1, 1210:025b, 1220:0156 |  |
| `gfx_store_order_is_save_first` | LIVE | 1098:00b5, 10d0:0854 |  |
| `palette_dispose` | NOOP | 1090:0488, 1090:0596 |  |
| `palette_handle_is_live` | LIVE | 1090:0596 |  |

### `src/worldgen.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `worldgen_dir_dx_at` | LIVE |  |  |
| `worldgen_dir_dy_at` | LIVE |  |  |
| `worldgen_generate_random` | STUB | 1028:0000, 1028:066a, 1028:0a93, 1028:0fb4, 1028:12c6, 11c8:003d | has TODO |
| `worldgen_load_preset` | STUB | 1028:0a93, 1028:1d15, 10b8:079d | has TODO |
| `worldgen_load_earth` | STUB | 1028:0a93, 1028:1d15, 10b8:079d, 10b8:0e00, 10e0:0000, 1100:03ff, 1108:0243, 11e0:0221, 1420:2e28, 1420:2e34 | has TODO |
| `worldgen_dialog_id_to_cell` | LIVE | 10e0:0000, 1108:0243, 1420:2e28, 1420:2e34 |  |
| `worldgen_custom_apply` | LIVE | 10e0:0000, 1100:03ff, 1108:0243, 1420:2e28, 1420:2e34 |  |
| `worldgen_prepare_earth` | STUB | 1028:0000, 10b8:079d, 10e8:0000, 1108:01bc, 1108:0243, 1150:00c9, 11e0:0000, 12d8:44e2 | has TODO |
| `worldgen_prepare_mode` | LIVE | 1028:0000, 1108:01bc, 12d8:44e2 |  |
| `worldgen_is_preset` | LIVE | 1028:0000 |  |
| `worldgen_earth_frame` | LIVE | 1028:0000, 10b8:0aa8, 10b8:11bc |  |
| `worldgen_earth_preset_bytes` | LIVE | 10b8:079d |  |
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
| `worldgen_island_target` | LIVE | 1028:0000 |  |
| `worldgen_island_done` | LIVE | 1028:0945 |  |
| `worldgen_blob_in_bounds` | LIVE | 1028:09ef |  |
| `worldgen_river_commit_len` | LIVE | 1028:066a, 1028:1dfb |  |
| `worldgen_river_target` | LIVE |  |  |
| `worldgen_pollution_should_paint` | LIVE | 1028:1afd |  |
| `worldgen_score_clamp` | LIVE | 1028:0fb4, 11c8:0063 |  |
| `worldgen_score_layer` | LIVE |  |  |
| `worldgen_earth_frame_w` | LIVE |  |  |
| `worldgen_earth_frame_h` | LIVE |  |  |
| `worldgen_continent_rank_cmp` | LIVE | 1028:0a93 |  |
| `worldgen_score_weight` | LIVE | 1028:0fb4, 1420:17d6 |  |
| `worldgen_score_water_bonus` | LIVE |  |  |
| `worldgen_score_dither` | LIVE |  |  |
| `worldgen_score_accum` | LIVE |  |  |
| `worldgen_link_grid_w` | LIVE | 1028:12c6, 1028:162c, 1248:0004, 1248:0108 |  |
| `worldgen_link_grid_h` | LIVE |  |  |
| `worldgen_link_idx` | LIVE |  |  |
| `worldgen_link_bit` | LIVE |  |  |
| `worldgen_link_rev_bit` | LIVE |  |  |
| `worldgen_continent_link_allowed` | LIVE | 1028:12c6, 1128:16d0 |  |
| `worldgen_ocean_run_ok` | LIVE | 1028:162c |  |
| `worldgen_label_is_land_pass` | LIVE | 1028:0a93 |  |
| `worldgen_probe_font_height` | LIVE | 1060:0000, 1060:01c1 |  |
| `worldgen_probe_font_weight` | LIVE |  |  |
| `worldgen_probe_custom_font` | STUB | 1060:0000, 1060:01c1, 1098:124f | has TODO |
| `worldgen_scatter_count` | LIVE | 1028:0000 |  |
| `worldgen_scatter_is_resample` | LIVE |  |  |
| `worldgen_scatter_tile_action` | LIVE | 1028:05e7, 11e8:0dcf |  |
| `worldgen_seed_origin_x` | LIVE | 1028:0945 |  |
| `worldgen_seed_origin_y` | LIVE |  |  |
| `worldgen_video_caps_query_count` | LIVE | 1060:01c1 |  |
| `worldgen_video_caps_query_at` | LIVE |  |  |
| `worldgen_video_caps_needs_raster_ext` | LIVE |  |  |
| `worldgen_show_video_caps_dialog` | STUB | 1060:0000, 1060:005b, 1060:01c1, 1098:0e0b, 1098:124f, 11e8:0b40 | has TODO |
