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

## Ścieżka krytyczna WinMain @ 1008:0000 (stan na 2026-10-16, fala H)

Fala H rule: `CIV_OK` = efekt osiągnięty albo krok dowodliwie pusty na SDL;
w przeciwnym razie uczciwy `CIV_ERR_UNSUPPORTED` (koniec z fake-OK).
`main.c`: SDL_Init + okno/renderer + arena 16 MB (`win16_mem_init`) ->
`boot_run` zatrzymuje się uczciwie na `boot_register_classes` (STUB:
pędzle/metryki/klasy, U2); fail buta = abort jak orig MESSAGEBOX+exit
(return 1). Za każdym lądowaniem fali binarka dochodzi dalej; frontier
czytaj z logu `boot:`. Za bootem reszta leci best-effort (log, nigdy fatal).
Warstwa Win16/DOS żyje w `src/win16.{h,c}` (SDL3/stdio, arena zamiast
GLOBALALLOC/GETFREESPACE; WONTFIX-y jako no-op `CIV_OK` z adresem Orig
tylko gdy krok jest dowodliwie pusty).

| Krok WinMain | Funkcja | Stan | Komentarz |
|---|---|---|---|
| Sys_CheckFreeMemory | `sys_check_free_memory` | LIVE | delegat do `win16_check_free_memory` (SDL_GetSystemRAM vs 2,5 MB + 0x23) |
| paths (drive/cwd) | `sys_init_paths` | LIVE | |
| App_RegisterWindowClasses | `app_register_window_classes` | STUB | HONEST STOP (fala H): pędzle/metryki/klasy/custom TBD (U2) |
| civfonts.fon | `font_enum_families_init` | LIVE | sonda plików via `win16_probe_fonts`; parse na żądanie (`font_fon/fnt_*` LIVE), install GDI WONTFIX |
| ResMgr_LoadRsc x5 | `boot_load_resources` | LIVE | sonda 5 plików via `win16_probe_resources` (case-insensitive); `rsc_load_rsc` LIVE, właściciel mgr TBD |
| Video_CheckDriverCaps | `video_init` | LIVE | check renderera tam gdzie renderer istnieje (arg-less step usunięty, fala H) |
| Art_MaybeConvertArtwork | `art_maybe_convert_artwork` | LIVE | na Linuksie zbędne — no-op CIV_OK |
| Config_LoadPrefs | `boot_load_prefs` | LIVE | domyślne (sounds on); I/O `civ.prf` LIVE w `config_load_prefs`/`config_save_prefs` via `win16_l*` na SDL_IO |
| Palette_Init | `palette_init` | LIVE | no-op (paleta per-tekstura) |
| Game_InitGlobalTables | `boot_init_tables` | LIVE | rng + arena 16 MB; WONTFIX: Win16-lock selektora (bufor mapy w `map.c` `.bss`) |
| Main_RunStartupSequence (intro) | `main_run_startup_sequence` | STUB | **krytyczna ścieżka**: Anim_OpenSequence @ 10b0:008a + pętla AdvanceFrames @ 10b0:0461 nad dialogiem 0x94 (P0); menu dopiero po intrze |
| Init_UnitFontTables | `boot_init_unit_font_tables` | STUB | potrzebne dopiero wizardowi (P1) |
| Sound_PlayId(STOP) | `audio_play_id` | LIVE | |
| MainPort/GfxPort | `mainport_get_or_create`, `gfx_port_create` | STUB | mostek na SDL_Texture — średnia praca (P3) |
| WdwMap/SmMap/Status_Open | `map_open_*` | STUB | `map_init` HONEST STOP (bufor/widoki, U4); `boot_open_windows` STUB po `video_init` |
| Menu_InitMainBar | `menu_init_mainbar` | LIVE | WONTFIX: brak USER-menu na SDL — no-op (stringi w `menu_strings` LIVE); `boot_init_menu` STUB via `dialog_init` (U2) |
| pętla Game_TurnLoopTick | `game_turn_tick` | STUB | gra właściwa — poza zakresem |
| WinMain_Shutdown | `boot_shutdown` | LIVE | no-op shutdowny + `win16_mem_shutdown` w `main.c` |

Ekran main menu = `dialog_menuz` (STUB) + `dlg_create_tracked` (STUB) nad
`CIV_MAINMENU_TEXT` (tekst już w `mainmenu.h`); wykonalne bez RSC
(proceduralne widgety + debug-text), wierny wygląd czeka na dekod ramek
`rsc_get_indexed` (sonda plików już LIVE).

## Priorytety RE/dump (wg strict-stopów z 2026-10-16)

- P0 intro (krytyczna ścieżka, przed menu): `Anim_OpenSequence @ 10b0:008a`,
  pętla `Anim_AdvanceFrames @ 10b0:0461`, współdzielony player `FUN_1030_0000`
  (~31 KB, 16 wołających), dialog 0x94 (`Dlg_CreateTracked @ 1098:124f` +
  `DestroyTracked @ 1098:1612`), `Gfx_StoreDC @ 1098:1104`,
  `Anim_CloseSequence @ 10b0:0287`, `Anim_RebaseClock @ 10b0:005b`.
  (SDL odpowiednik `title_play_intro_sdl` już gra MPLOGO.BMP + OPENING.wav,
  ale prawdziwa ścieżka idzie przez opkody Anim.)
- P1 menu (dopiero po intrze): `Dialog_MenuZ_Modal @ 1220:0156` nad tekstem
  `1420:435b`.
- P2 new game: `NewGame_Wizard @ 10e8:0000`, `Init_UnitFontTables @ 10e8:2d46`,
  `Tribe_GetNameList @ 11f8:0080`, `Map_GenerateRandomOrLoad @ 1028:0000`.
- P3 turn loop: `Game_TurnLoopTick @ 1088:0000`, `Sys_DrainInput @ 11f0:4ea0`.
- P4 render: `GfxPort_Create/Present @ 10d0:0000/0854`, repaint
  `@ 1208:1866/1f64`, `Text_Draw* @ 10d8:*`, dump godpal 0xec.
- WONTFIX (nie dumpować): semantyka GLOBALALLOC-lock, GDI-install fontów,
  bajty checklisty GETDEVICECAPS, chunki drivera w `civ.prf`, wnętrze MCI.

## Korekty mapy RE (do przeniesienia do `src/*.h`)

- `FUN_1030_0000` to NIE `Title_PlayAnimLoop`: ~31 KB, 16 wołających
  (miasta, `Unit_DoMove`, `City_ShowTopFive`, HoF…) — współdzielony
  odtwarzacz PIC/animacji. `title.h` do poprawki.

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
| `snd_nop000e` | LIVE | 1150:000e |  |

### `src/boot.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `boot_setup_paths` | LIVE | 1000:11bb, 1000:2826 |  |
| `boot_check_memory` | LIVE | 11e8:0000, 1420:43b8 |  |
| `app_register_window_classes` | PARTIAL | 1058:0b4d, 1098:00b5, 1098:01b0, 1098:06f8, 1098:1838, 1208:0054, 1210:0000, 1218:0000 | has TODO |
| `boot_register_classes` | LIVE | 1098:06f8 |  |
| `boot_load_fonts` | LIVE | 1008:0e99 |  |
| `boot_load_resources` | LIVE | 1190:0000 |  |
| `boot_maybe_convert_artwork` | LIVE | 1008:059a, 10b8:0000, 10b8:079d, 1420:20a9 |  |
| `boot_load_prefs` | LIVE | 1008:0b4a |  |
| `boot_init_palette` | LIVE | 1090:0488 |  |
| `boot_key_remap_value` | LIVE | 10e8:2d46, 11e8:0337 |  |
| `boot_init_tables` | LIVE | 1090:0488, 10e8:2d46, 11c8:0000, 11e0:0000, 11e8:0337, 11f8:0080, 1420:b394 |  |
| `boot_init_unit_font_tables` | PARTIAL | 1098:0157, 10e8:0000, 10e8:2d46, 11a8:022f, 1608:000c | has TODO |
| `cursor_set` | NOOP | 1048:0000, 1098:096c |  |
| `boot_open_windows` | PARTIAL | 1098:114a, 10d0:0000, 1208:0af1, 1210:025b, 1218:0386 | has TODO |
| `menu_init_mainbar` | LIVE | 1048:0000, 1098:096c, 14b0:0010, 14b0:0092, 14d0:0028 |  |
| `boot_init_menu` | LIVE | 1048:0000, 1098:096c |  |
| `dos_drive_ready` | LIVE | 1000:1184, 1008:0bbf, 1150:00c9 |  |
| `dos_set_path` | LIVE | 1000:116a |  |
| `font_enum_families_init` | LIVE | 1008:0e99, 1420:00a4 |  |
| `font_fon_u16le` | LIVE |  |  |
| `font_fon_u32le` | LIVE |  |  |
| `font_fon_parse` | LIVE | 1008:0e99, 1420:00a4 |  |
| `font_face_count` | LIVE | 1008:0e99 |  |
| `font_face_list` | LIVE | 1008:0e99 |  |
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
| `config_load_prefs` | LIVE | 1008:0b4a, 12d0:0064, 1420:010b |  |
| `config_save_prefs` | LIVE | 1000:116a, 1000:1184, 1008:0bbf, 1228:0002, 12d0:0064, 1420:010b, 1420:0113 |  |
| `boot_run` | LIVE | 1008:0000, 1058:0b4d, 1088:0000, 1098:06f8, 11e8:0000 |  |
| `boot_shutdown` | LIVE | 1008:0329, 1008:0d68, 12d8:28fa, 12d8:460e |  |
| `art_maybe_convert_artwork` | LIVE | 1090:0488, 10b8:0000, 10b8:04df, 10b8:062d, 10b8:06a0, 10b8:076a, 10b8:079d, 10b8:0aa8, 10b8:0c1c, 10b8:0ded, 10b8:0e36, 10b8:11bc, 10b8:158c, 10d0:0cfa, 1420:20a9 |  |
| `main_run_startup_sequence` | PARTIAL | 1008:0329, 1098:0b33, 1098:124f, 1098:1612, 10b0:005b, 10b0:008a, 10b0:0461, 10b8:079d, 10d0:0000, 10e8:0000, 1108:0243, 1130:000e, 11e0:0000, 11e0:0221, 1220:0156, 1420:435b | has TODO |
| `config_prefs_defaults` | LIVE | 1008:0b4a, 1008:0bbf, 1150:00c9, 12d0:0064 |  |
| `config_prefs_sounds_on` | LIVE |  |  |
| `boot_video_caps_count` | LIVE | 1008:059a, 1060:01c1, 1420:0af4, 1608:00f4, 1608:00f8 |  |
| `boot_video_caps_describe` | LIVE | 1008:059a, 1420:0af4 |  |
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
| `app_timer_interval_ms` | LIVE | 1008:0329, 1098:06f8, 1098:0948, 12d8:28fa, 1608:008c |  |
| `menuz_timer_interval_ms` | LIVE | 11e8:02fb, 1220:0156, 1220:0202, 1220:0cfd, 1420:4a41, 1608:008c, 1608:0134 |  |
| `main_startup_anim_loops` | LIVE | 10b0:008a, 10b0:0461, 10e8:0000, 1108:01bc, 11e0:0000 |  |
| `main_startup_intro_sound` | LIVE | 1420:3114 |  |
| `main_startup_stop_sound` | LIVE |  |  |
| `main_startup_should_run_wizard` | LIVE | 1098:1612, 10e8:0000, 1108:01bc, 11e0:0000 |  |
| `main_startup_should_break` | LIVE | 10b0:0461, 11e0:0000, 11e0:0221 |  |
| `boot_free_memory_is_low` | LIVE | 11e8:0000 |  |
| `boot_runtime_needs_first_warning` | LIVE | 1088:0000, 11e8:0075 |  |
| `boot_runtime_needs_final_warning` | LIVE |  |  |
| `main_startup_intro_dialog_id` | LIVE | 1098:124f, 11e0:0000 |  |
| `main_startup_intro_dialog_flags` | LIVE |  |  |
| `main_startup_earth_seq_id` | LIVE | 10b0:008a |  |
| `main_startup_rebase_ticks` | LIVE | 10b0:005b, 11e0:0221 |  |
| `main_startup_intro_proc_w` | LIVE | 1098:124f |  |
| `main_startup_intro_proc_h` | LIVE | 1098:124f |  |
| `main_startup_procedural_ready` | LIVE | 1098:124f, 1190:0a29 |  |
| `main_startup_needs_rsc_frame` | LIVE | 1190:0a29, 1190:0fda |  |
| `main_startup_host_palette_ok` | LIVE | 1098:124f, 1098:201d, 10b0:0461, 11e0:0000 |  |
| `main_startup_port_pair_ok` | LIVE | 1098:124f, 10b0:008a, 10d0:0000, 11e0:0000 |  |
| `main_startup_modal_is_done` | LIVE | 10b0:0461, 11e0:0000, 11e0:0221 |  |
| `main_startup_event_is_dismiss` | LIVE | 1220:0156 |  |
| `main_startup_pump_events` | LIVE | 1098:124f, 1100:02e1, 11e0:0000, 1220:0156, 1220:0202 |  |
| `main_startup_tick_is_due` | LIVE | 1100:02e1, 11e0:0221, 1220:0156, 1220:0202, 1220:0cfd, 1420:4a41 |  |
| `main_startup_anim_ctx_ok` | LIVE | 1098:124f, 10b0:008a |  |
| `main_startup_intro_seq_resolve` | LIVE | 10b0:008a |  |
| `main_startup_menu_text_ok` | LIVE | 1000:36b2, 11e0:0000, 11e0:0221, 1220:0000, 1220:0156, 1420:435b |  |
| `boot_unit_font_firstmove_count` | LIVE | 10e8:2cc1, 10e8:2d46 |  |
| `boot_unit_font_pillage_count` | LIVE |  |  |
| `boot_unit_font_pillage_first` | LIVE |  |  |
| `boot_unit_font_palette_entries` | LIVE |  |  |
| `boot_unit_font_map_count` | LIVE |  |  |
| `boot_unit_font_icon_count` | LIVE | 1098:0157, 11a8:022f |  |
| `boot_unit_font_icon_len` | LIVE |  |  |
| `boot_unit_font_needs_alt` | LIVE |  |  |
| `boot_unit_font_best_pick` | LIVE | 10e8:2cc1 |  |
| `boot_unit_font_palette_bytes` | LIVE | 10e8:2cc1, 10e8:2d46 |  |
| `boot_unit_font_palette_lock_ok` | LIVE | 10e8:2cc1 |  |
| `boot_unit_font_replace_color` | LIVE | 1098:0157, 10e8:2d46, 11a8:022f |  |
| `text_draw_shadowed_centered` | PARTIAL | 10d8:02df, 10d8:046e, 10d8:0495, 10d8:05b8, 11e8:0b40 | has TODO |
| `text_measure_width` | LIVE | 10d8:05b8, 1100:0000, 1100:00a1 |  |
| `text_draw_pascal_at_origin` | PARTIAL | 10d8:002d, 10d8:00e0, 10d8:05b8, 1100:0000, 1100:0053, 1100:00a1, 1100:017b, 1608:01cc | has TODO |
| `boot_unit_font_target_small_len` | LIVE | 10e8:2d46 |  |
| `boot_unit_font_target_big_len` | LIVE | 10e8:2d46 |  |
| `boot_draw_aligned_list` | PARTIAL | 1068:258f, 1100:00ea, 1608:01cc | has TODO |
| `boot_draw_aligned_list_free` | PARTIAL | 1068:255a | has TODO |
| `main_dequeue_res_char` | LIVE | 1008:09e3, 1008:0a22, 1008:0a8d, 1018:166b, 1030:58c2, 11e8:02fb, 11f0:0939, 1220:0ab2 |  |
| `main_dequeue_res_string` | LIVE | 1008:0a22, 1008:0a8d, 11f0:4f2d |  |
| `main_return_zero_stub` | LIVE | 1008:0a8d, 1008:0acb |  |
| `win_copy_window_rect` | STUB | 1000:137f, 1008:0b13, 1608:00ac | has TODO |
| `boot_noop_thunk` | NOOP | 1008:09f8, 1008:0a06, 1008:0a14, 1008:0d4c, 1008:0d5a, 1008:0d68, 1008:0e02, 1008:0e12 |  |
| `boot_noop_thunk2` | NOOP | 1008:0a06 |  |
| `boot_empty_thunk_0a14` | NOOP | 1008:0a14 |  |
| `boot_empty_thunk_0d4c` | NOOP | 1008:0d4c |  |
| `boot_empty_thunk_0d5a` | NOOP | 1008:0d5a |  |
| `boot_empty_thunk_0e02` | NOOP | 1008:0e02 |  |
| `win_destroy_window_and_clear` | STUB | 1100:03bb, 1608:00dc | has TODO |
| `win_resize_window_stored_rect` | STUB | 1100:04e1, 1608:00ac, 1608:00e0 | has TODO |
| `win_is_point_in_client_rect` | LIVE | 1100:0534, 1608:0104 |  |
| `main_pump_until_quit` | STUB | 1008:0f2b, 1038:0c1d, 1098:062f, 1420:54e6, 1420:54e8 | has TODO |
| `main_noop_stub` | LIVE | 1008:0acb, 1008:101d |  |
| `main_beep_repeated` | STUB | 1008:103d, 1608:012c | has TODO |
| `main_poll_key_or_click` | STUB | 11e0:095f | has TODO |
| `main_load_menu_art` | STUB | 11e0:09d0 | has TODO |
| `font_map_code_to_value` | LIVE | 11e8:0a3c, 11e8:0b40 |  |
| `cursor_set_and_flag` | STUB | 1200:0078, 1208:121a |  |
| `cursor_dirty_image` | LIVE |  |  |

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
| `util_format_float` | LIVE | 1000:01fe, 1000:0221, 1000:0754, 1420:4ad6, 1420:50cb |  |
| `util_str_case_cmp` | LIVE | 1000:36de, 11f8:01af |  |
| `util_format_message` | LIVE | 1000:2928, 1000:2c9c, 1000:36b2, 1088:1dd0, 1190:17d0 |  |
| `util_compare_bounded` | LIVE | 1000:36de, 1000:3744, 1000:377e, 1008:0e12, 1008:0e99, 11f8:014a, 11f8:01af, 11f8:0207, 1420:011b |  |
| `util_str_len` | LIVE | 1000:19f2, 1000:2e85, 1000:3088, 1000:3722, 1100:0000, 1100:00ea, 1608:0220 |  |
| `util_str_rchr` | LIVE | 1000:37b2, 1000:4124 |  |
| `float_ftol` | LIVE | 1000:110d |  |
| `u32_mul32_far` | LIVE | 1000:1368, 1000:16fe |  |
| `s32_div32` | LIVE | 1000:1423, 1420:b4aa |  |
| `u32_shl32` | LIVE | 1000:14d4 |  |
| `util_itoa_dec` | LIVE | 1000:157a, 1000:15fc, 1000:1648 |  |
| `u32_mul32` | LIVE | 1000:1368, 1000:16fe, 1000:1729, 1000:386e, 1000:3ba0 |  |
| `util_to_lower_alias` | LIVE | 1000:194a, 1030:0000, 1420:4baf |  |
| `util_to_upper` | LIVE | 1000:1978, 1100:0b9d, 1190:0e72, 11e8:09f7 |  |
| `util_stpcpy` | LIVE | 1000:1648, 1000:1e9c, 1000:299e, 1000:3722 |  |
| `util_get_env` | LIVE | 1000:28b6, 1000:3ba0 |  |
| `util_long_to_dec` | LIVE | 1000:157a, 1000:2928, 1000:2974 |  |
| `sort_swap_elems` | LIVE | 1000:30e2, 1000:3110 |  |
| `sort_qsort_core` | LIVE | 1000:1423, 1000:3110, 1000:3396 |  |
| `sort_qsort` | LIVE | 1000:3396, 1068:004c, 10c0:023e |  |
| `sort_elem_size_image` | LIVE |  |  |
| `sort_has_cmp_image` | LIVE |  |  |
| `util_str_cmp` | LIVE | 1000:3680, 1000:36de, 1000:3744, 1068:004c, 1178:0022 |  |
| `util_str_str` | LIVE | 1000:37fe |  |
| `util_mem_copy_far_dst_first` | LIVE | 1000:137f, 1000:40aa, 1098:2a85, 10c0:0413 |  |
| `util_mem_set_far_core` | LIVE | 1000:29c4, 1000:40d0 |  |
| `util_mem_set_far` | LIVE | 1000:40f6, 10c0:013d |  |
| `util_swap_bytes16` | LIVE | 10b8:11bc, 1100:0565, 1140:04b5 |  |
| `util_swap_bytes32` | LIVE | 1100:0586 |  |
| `util_str_upper` | LIVE | 1000:1978, 1000:3722, 1068:1dd7, 1100:0b9d |  |
| `util_store_far_pointer` | LIVE | 1140:003b, 1348:0000 |  |
| `util_return_zero_0071` | LIVE | 1140:0071 |  |
| `util_return_zero_0083` | LIVE | 1140:0083 |  |
| `util_return_zero_00d4` | LIVE | 1140:00d4 |  |

### `src/dialog.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `dialog_init` | PARTIAL | 1058:0000, 1058:0440, 1058:049b, 1058:04f6, 1058:0b4d, 1058:0d71, 1058:0e7f, 1098:0157, 1098:124f, 1098:1612, 1098:16e2, 1098:1838, 10b0:0000, 10b0:0461, 1220:0156, 1420:1ea0, 1420:1ec8, 1420:1f40, 1420:4a41, 14b0:0010, 14b0:0092, 14d0:0028 | has TODO |
| `dlg_proc` | LIVE | 1098:124f, 1098:16e2, 1420:1ea0 |  |
| `civ_dialog_proc` | LIVE | 1098:124f, 1098:1838 |  |
| `dialog_shutdown` | NOOP |  |  |
| `dialog_menuz` | PARTIAL | 1220:0000, 1220:0156, 1420:435b, 1420:4a41 | has TODO |
| `dialog_ctrl_register` | PARTIAL | 1058:0000, 1058:0440, 1058:04f6, 1058:0b4d, 1058:0d71, 1058:0e7f | has TODO |
| `dialog_ctrl_free` | NOOP | 1058:0d71 |  |
| `dialog_ctrl_radio_set` | PARTIAL | 1058:0e7f | has TODO |
| `dialog_ctrl_class_count` | LIVE | 1058:0b4d |  |
| `dialog_ctrl_class_valid` | LIVE |  |  |
| `dialog_ctrl_radio_count` | LIVE | 1058:0e7f |  |
| `dialog_ctrl_radio_index` | LIVE | 1058:0e7f |  |
| `dialog_ctrl_paint_button` | LIVE | 1058:0000 |  |
| `dialog_ctrl_paint_static` | LIVE | 1058:04f6 |  |
| `dialog_ctrl_paint_user` | LIVE | 1058:0440 |  |
| `dialog_ctrl_radio_proc` | LIVE | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6 |  |
| `dialog_ctrl_static_proc` | LIVE | 1058:081a |  |
| `dialog_ctrl_user_proc` | LIVE | 1058:0921 |  |
| `dialog_ctrl_region_proc` | LIVE | 1058:09f6 |  |
| `dlg_create_tracked` | PARTIAL | 1000:137f, 1018:0000, 1030:0000, 1070:02b5, 1080:0119, 1090:0376, 1098:096c, 1098:0a5b, 1098:0ae0, 1098:0b67, 1098:1104, 1098:124f, 1098:1612, 1098:16e2, 1098:1838, 1098:201d, 10b0:008a, 10b0:0287, 10b0:0461, 10d0:0854, 10d0:09a2, 10e8:0000, 1100:03ff, 1100:060b, 1100:09a9, 1108:01bc, 1150:00c9, 1180:0297, 11e0:0000, 11e0:0221, 11f8:0000, 1220:00d2, 1420:17aa, 1420:1ea0, 1420:1ec8, 1420:9948, 1420:9960 | has TODO |
| `dlg_destroy_tracked` | LIVE | 1098:1612, 1100:060b, 1420:17aa, 1420:1ea0, 1420:9960 |  |
| `dlg_create_should_center` | LIVE | 1098:124f |  |
| `dlg_create_should_show` | LIVE |  |  |
| `dlg_ctl_is_tracked_init` | LIVE |  |  |
| `dlg_tracked_push_off` | LIVE | 1420:1ea0 |  |
| `dlg_tracked_pop_count` | LIVE | 1098:1612 |  |
| `dlg_move_extent` | LIVE | 1098:0a5b, 1098:0ae0, 1098:0b67 |  |
| `dlg_move_should_use_default` | LIVE | 1098:0b67 |  |
| `anim_pump_if_active` | PARTIAL | 1098:0157, 10a8:0000, 10a8:00b4, 10b0:0000, 10b0:0026, 10b0:008a, 10b0:0287, 10b0:0461, 10b0:151b, 10b0:172d, 1100:02e1, 1150:00c9, 1420:1f40 | has TODO |
| `anim_advance_frames` | PARTIAL | 1098:1104, 10b0:0461, 1100:02e1, 1150:00c9 | has TODO |
| `anim_open_sequence` | PARTIAL | 10b0:008a, 1100:02e1, 1420:1f40 | has TODO |
| `anim_close_sequence` | LIVE | 10a8:00b4, 10b0:0287, 10b0:172d |  |
| `anim_seq_count_frames` | LIVE | 10b0:008a |  |
| `anim_seq_max_end` | LIVE |  |  |
| `anim_seq_is_active` | LIVE | 1098:0157, 10b0:0000, 10b0:0026 |  |
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
| `diplomacy_post_contact` | PARTIAL | 1168:0a07, 1168:0dda, 1168:0e8e, 1168:0f47, 1168:1220, 11c0:0395, 1220:00d2, 1420:62fc | has TODO |
| `diplomacy_eval_bribe` | STUB | 1078:1480, 1078:455c, 11c0:0395 | has TODO |
| `ui_draw_end_turn_prompt` | STUB | 10d0:0854, 10d8:00e0, 1110:0448, 11c0:0ec2, 1210:07ae | has TODO |
| `dialog_menuz_id_to_index` | LIVE | 1220:0156 |  |
| `dialog_menuz_index_to_id` | LIVE | 1220:0156 |  |
| `dialog_menuz_preselect_ok` | LIVE | 1058:0e7f, 1220:0156 |  |
| `text_align_x` | LIVE | 1098:0e0b, 1098:0ed1 |  |
| `text_center_y` | LIVE | 1098:0dda, 1098:0ed1 |  |
| `dialog_dlgunits_to_pixels` | LIVE | 1098:0d43, 1100:03ff |  |
| `dialog_client_rect` | LIVE | 1100:03ff |  |
| `dlg_center_rect` | LIVE | 1068:0958, 1098:0a5b, 1098:0ae0, 1098:0b67, 1170:2623, 11f8:0000, 1210:0e29 |  |
| `text_draw_aligned` | PARTIAL | 1080:0119, 1080:08e9, 1088:1554, 1098:00b5, 1098:01b0, 1098:0e0b, 1098:0ed1, 1098:103f, 1098:20f4, 10b0:0461, 10b0:137a, 10e8:0667, 1110:07c3, 1180:02d3, 1220:0889 | has TODO |
| `dialog_get_item_rect` | PARTIAL | 1098:0d43, 1098:124f, 10e8:0000, 1100:03ff, 1220:0156 | has TODO |
| `dialog_show_message` | PARTIAL | 1088:13d5, 1098:124f, 1098:1612, 1220:00d2, 1220:0156, 1220:0eb8, 1420:172b | has TODO |
| `text_res_list_advance` | LIVE | 1098:1f4f, 1100:0053, 1100:078a |  |
| `main_enqueue_res_string` | LIVE | 1008:0a8d, 1420:4473 |  |
| `dialog_res_list_spec` | LIVE | 1068:0295 |  |
| `main_res_queue_full` | LIVE | 1008:0a8d, 1008:0add, 1098:02d1, 11f0:4ea0 |  |
| `dialog_res_heap_size` | LIVE | 1068:0295, 1098:1f4f |  |
| `dialog_res_list_count` | LIVE | 1068:0295, 1098:1f4f, 1100:078a |  |
| `text_res_list_fits` | LIVE | 1068:0295 |  |
| `dialog_init_res_string_tables` | PARTIAL | 1068:0295, 1098:1f4f, 1100:0053 | has TODO |
| `main_show_res_string` | PARTIAL | 1008:0a8d, 1008:0add, 1100:078a, 12b0:004c, 1420:4473 | has TODO |
| `text_dlg_string_source` | LIVE | 1098:2796, 1100:078a, 1100:0947 |  |
| `text_set_dlg_item_text` | PARTIAL | 1098:2796, 1100:0000, 1100:0053, 1100:0947 | has TODO |
| `text_draw_dlg_item_string` | PARTIAL | 1000:36b2, 1098:2796, 10f0:0066, 1100:0000, 1100:03ff, 1100:078a, 1100:0947 | has TODO |
| `text_fill_res_string_list` | PARTIAL | 1068:0295, 1098:1f4f, 1100:0053, 1100:078a | has TODO |
| `diplo_contact_flags` | LIVE | 1168:0a07, 1168:0dda |  |
| `diplo_is_peace` | LIVE | 1168:0a07 |  |
| `diplo_is_contact` | LIVE |  |  |
| `diplo_has_embassy` | LIVE |  |  |
| `diplo_is_war` | LIVE | 1168:0000, 1168:0dda, 1168:0e8e |  |
| `diplo_is_seen` | LIVE | 1168:0000 |  |
| `diplo_apply_peace` | LIVE | 1168:0dda |  |
| `diplo_apply_war` | LIVE | 1168:0e8e |  |
| `diplo_tribute_amount` | LIVE | 1160:328e, 1168:0000, 11c8:0063 |  |
| `diplo_should_exchange` | LIVE | 1168:1220, 1170:2aaa, 1170:3181 |  |
| `diplo_exchange_grant_count` | LIVE | 1168:1220 |  |
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
| `dialog_draw_frame3d` | PARTIAL | 1018:24c5, 1220:0156, 1220:0d38 | has TODO |
| `text_shadow_offset` | LIVE | 10d8:02df |  |
| `text_centered_x` | LIVE | 10d8:0495, 10d8:0528 |  |
| `text_measure_scaled` | LIVE | 10d8:05b8, 1100:00a1 |  |
| `text_resolve_color` | LIVE | 10d8:002d, 10d8:00e0 |  |
| `text_shadow_pos` | LIVE | 10d8:02df |  |
| `text_pascal_len` | LIVE | 1100:0000 |  |
| `text_font_table` | LIVE | 11e8:0d1d |  |
| `text_font_table_lookup` | LIVE | 11e8:0b40, 11e8:0d1d |  |
| `text_font_slot_for_id` | LIVE | 11e8:0b40, 11e8:0d1d |  |
| `text_font_height_for_id` | LIVE |  |  |
| `text_dispatch_font` | LIVE | 1098:0186, 1098:0194, 11e8:0b40, 11e8:0cb0 |  |
| `text_pack_pascal` | LIVE | 1100:0000, 1100:0053, 11f8:00fc, 11f8:014a |  |
| `text_pascal_payload` | LIVE | 1100:0000, 1100:00a1 |  |
| `text_draw_shadowed` | PARTIAL | 1098:0e0b, 10d8:02df, 11e8:0b40, 1220:0f2f | has TODO |
| `text_draw_at` | PARTIAL | 1098:0e0b, 10d8:01fa, 10d8:02df, 11e8:0b40, 1220:0f2f | has TODO |
| `text_draw_clipped` | PARTIAL | 10d8:00e0, 10d8:01b0, 11e8:0b40, 1220:0f2f | has TODO |
| `text_draw_direct` | PARTIAL | 1000:36b2, 1098:0e0b, 10d8:00e0, 1100:0000, 1100:017b | has TODO |
| `text_draw_centered` | PARTIAL | 10d8:01b0, 10d8:0528, 10d8:05b8, 1100:0000, 1100:00a1 | has TODO |
| `text_draw_header_centered` | PARTIAL | 10d8:046e, 10d8:0495, 10d8:05b8, 11e8:0b40 | has TODO |
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
| `anim_opcode_is_frame_load` | LIVE | 10b0:0461, 10b8:079d, 10b8:0e36 |  |
| `anim_opcode_is_scroll` | LIVE | 10b0:0461 |  |
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
| `anim_clamp_tick` | LIVE | 10b0:0461, 1100:02e1 |  |
| `anim_advance_ret` | LIVE | 10b0:0461, 11e0:0000 |  |
| `anim_slot_should_step` | LIVE | 10a8:00fc, 10a8:018e |  |
| `anim_scroll_sync_tick` | LIVE | 1008:0df0, 10b0:0461 |  |
| `text_sync_font_tail` | NOOP | 1008:09d3, 10b0:0461, 1100:0a4a |  |
| `dlg_center_tracked_window` | LIVE | 1098:0b33, 1098:124f, 11f8:0000 |  |
| `dlg_init_palette_word_writes` | LIVE | 1098:1184, 1098:1196, 1098:124f |  |
| `gfx_get_zero_palette_word` | LIVE | 1098:1184, 1098:11c8 |  |
| `anim_pal_src_for` | LIVE | 10b0:0461 |  |
| `anim_pal_fade_zero` | LIVE | 1090:0376, 10b0:0461 |  |
| `anim_pal_copy` | LIVE | 10b0:0461 |  |
| `anim_slot_phase` | LIVE | 10a8:0000, 10a8:00b4, 10b0:0461 |  |
| `anim_slot_should_free` | LIVE | 10a8:00b4, 10b0:0287, 10b0:0461, 1100:060b |  |
| `anim_caption_phase` | LIVE | 10b0:0461 |  |
| `anim_caption_needs_palette` | LIVE |  |  |
| `anim_caption_is_custom` | LIVE | 1000:36b2, 1100:078a |  |
| `anim_dissolve_phase` | LIVE | 10b0:0461, 10b0:151b, 10b0:172d |  |
| `anim_dissolve_should_free` | LIVE | 10b0:0287, 10b0:0461, 10b0:172d |  |
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
| `text_draw_aligned_list` | PARTIAL | 1068:258f, 1098:0ed1, 1100:00ea, 1220:0156, 1608:01cc | has TODO |
| `text_draw_aligned_list_free` | PARTIAL | 1068:255a | has TODO |
| `text_wrap_is_space` | LIVE | 1068:2072 |  |
| `text_wrap_is_hard_break` | LIVE | 1068:2072 |  |
| `text_wrapped_char_fix` | LIVE | 1068:2072 |  |
| `text_wrapped_overflows` | LIVE | 1068:2072 |  |
| `text_wrapped_trim_trailing` | LIVE | 1068:2072 |  |
| `text_wrapped_skip_leading` | LIVE | 1068:2072 |  |
| `text_wrapped_row_span` | LIVE | 1068:2072 |  |
| `text_wrapped_total_height` | LIVE | 1068:1dd7 |  |
| `text_show_needs_long_desc` | LIVE | 1068:06af, 1068:1dd7 |  |
| `quiz_answer_is_correct` | LIVE | 1178:0022, 12d0:00b6 |  |
| `quiz_should_retry` | LIVE | 1178:0000 |  |
| `quiz_tech_is_valid` | LIVE | 1178:0022 |  |
| `quiz_reward_flags` | LIVE | 1178:0022 |  |
| `quiz_needs_resettle` | LIVE | 1178:0022 |  |
| `quiz_pair_first_mode` | LIVE | 1178:0000 |  |
| `pedia_selector_valid` | LIVE | 1020:0000 |  |
| `pedia_kind_from_selector` | LIVE | 1020:0000 |  |
| `pedia_res_from_selector` | LIVE | 1020:0000, 1068:0295 |  |
| `pedia_dlg_id` | LIVE | 1020:0000 |  |
| `pedia_dlg_flags` | LIVE | 1020:0000 |  |
| `dlg_ctl_is_earth_extra` | LIVE | 1098:124f |  |
| `diplo_ai_contact_gate` | LIVE | 1168:0a07, 1168:0f47 |  |
| `diplo_senate_forces_peace` | LIVE | 1160:0000 |  |
| `ui_end_turn_phase` | LIVE | 10d8:00e0, 11c0:0ec2 |  |
| `text_needs_shadow` | LIVE | 1098:0e0b, 1098:0ed1, 10d8:00e0 |  |
| `dialog_refresh_view_count` | LIVE | 1220:00d2, 1220:0eb8, 12b0:0004 |  |
| `dialog_refresh_msg` | LIVE | 1220:0eb8 |  |
| `anim_opcode_is_text` | LIVE | 1000:36b2, 1098:0ed1, 1098:1104, 10b0:0461, 10d0:0854, 1100:078a |  |
| `anim_opcode_needs_present` | LIVE | 10b0:0461, 10b0:172d, 10d0:0854, 10d0:0956, 10d0:09a2 |  |
| `anim_opcode_needs_animate` | LIVE | 1090:0376, 1098:201d, 10b0:0461 |  |
| `anim_opcode_needs_sound` | LIVE | 10a8:0000, 10a8:00b4, 10a8:00fc, 10b0:0461, 1150:00c9, 11e0:0000, 1420:3114 |  |
| `dlg_preset_w` | LIVE | 1098:124f |  |
| `dlg_preset_h` | LIVE |  |  |
| `anim_seq_resolve_count` | LIVE | 10b0:008a |  |
| `anim_seq_ctx_size` | LIVE | 10b0:008a |  |
| `anim_seq_rec_stride` | LIVE | 10b0:008a |  |
| `anim_seq_op_word` | LIVE | 10b0:008a |  |
| `anim_seq_end_word` | LIVE |  |  |
| `anim_seq_zero_word0` | LIVE |  |  |
| `anim_seq_zero_word1` | LIVE | 10b0:008a, 1100:02e1 |  |
| `anim_seq_port_count` | LIVE | 10b0:008a, 10b0:0287, 10d0:0000 |  |
| `anim_seq_port_off` | LIVE | 10b0:008a, 10d0:0000 |  |
| `anim_tick_is_due` | LIVE | 10b0:0461 |  |
| `diplo_contact_stamp_due` | LIVE | 1168:0a07, 1168:0f47 |  |
| `dialog_menuz_item_disabled` | LIVE | 1220:0156 |  |
| `dialog_menuz_id_is_valid` | LIVE |  |  |
| `text_shadow_passes` | LIVE | 1098:0e0b, 1098:0ed1 |  |
| `dlg_tracked_ctx_size` | LIVE | 1098:124f, 1420:1ea0, 1420:1ec8 |  |
| `dlg_tracked_pal_count` | LIVE | 1098:201d |  |
| `dialog_menuz_timer_ms` | LIVE | 1220:0156, 1420:4a41 |  |
| `dialog_menuz_cancel_code` | LIVE |  |  |
| `help_topic_is_contents` | LIVE |  |  |
| `text_res_list_is_stop` | LIVE | 1098:1f4f |  |
| `diplo_tribute_unit` | LIVE | 1160:0000, 1168:0000 |  |
| `dialog_split_default_w` | LIVE | 1220:0000 |  |
| `dialog_menuz_base_id` | LIVE | 1058:0e7f, 1220:0156 |  |
| `dialog_menuz_last_id` | LIVE |  |  |
| `dialog_get_item_rect_needs_scale` | LIVE | 1098:0d43, 1100:03ff, 1220:0156 |  |
| `dialog_split_title_len` | LIVE | 1100:00ea, 1220:0000 |  |
| `dialog_split_is_title_empty` | LIVE |  |  |
| `dialog_frame3d_base_inset` | LIVE | 1018:24c5, 1220:0156, 1220:0d38 |  |
| `dialog_frame3d_edge_trim` | LIVE |  |  |
| `dialog_frame3d_base` | LIVE |  |  |
| `diplo_has_treaty` | LIVE | 1168:0a07, 1168:0f47, 12d8:3a0a |  |
| `diplomacy_contact_args_ok` | LIVE | 1078:455c, 1168:0a07, 1420:62fc |  |
| `text_align_slot` | LIVE | 1098:0e0b, 1098:0ed1 |  |
| `text_shadowed_needs_shadow` | LIVE | 1098:0ed1, 10d8:00e0, 10d8:01fa, 10d8:02df |  |
| `dlg_preset_flag_value` | LIVE | 1098:124f |  |
| `dlg_tracked_init_count` | LIVE |  |  |
| `dlg_tracked_entry_size` | LIVE |  |  |
| `dlg_ctl_earth_extra_count` | LIVE | 1098:124f |  |
| `dlg_tracked_top_off` | LIVE | 1098:1612 |  |
| `dlg_destroy_clears_top` | LIVE | 1420:17aa, 1420:9960 |  |
| `dlg_intro_rebase_ticks` | LIVE | 10b0:005b, 11e0:0000, 11e0:0221 |  |
| `anim_get_active` | LIVE | 10b0:0026, 10b0:008a, 10b0:0287, 1130:000e |  |
| `anim_reset_time_to_max` | LIVE | 10b0:003d, 11e0:0221 |  |
| `anim_present_active` | STUB | 1090:0119, 1098:1104, 10b0:03b0, 10d0:0854, 10d0:0956, 1608:00b0, 1608:0100 | has TODO |
| `pedia_show_list_dialog` | STUB | 1068:0026, 1098:18f7 | has TODO |
| `pedia_present_art_frame` | STUB | 1068:062f, 10b8:0e00, 10d0:0172, 10d0:0854, 1200:0116 | has TODO |
| `pedia_animate_tech_icon_zoom` | STUB | 1068:06af, 1068:1c13, 1170:21b3 | has TODO |
| `text_dispatch_dlg_char` | STUB | 1068:1f7e, 10e0:03a2, 1220:0156 | has TODO |
| `text_create_wrapped_box` | STUB | 1068:2302 | has TODO |
| `text_get_exclamation_mark` | LIVE | 1068:2716, 1420:1401 |  |
| `pedia_modal_pump_ok_cancel` | STUB | 1068:2728, 1068:282d | has TODO |
| `pedia_modal_subclass_proc` | STUB | 1068:282d | has TODO |
| `text_noop_after_bkcolor` | LIVE | 1098:217f |  |
| `dlg_copy_item_text_to_far` | STUB | 1000:36b2, 1098:2735, 1100:03ff | has TODO |
| `dlg_update_item_enable_show` | STUB | 1098:2829, 1608:00b4 | has TODO |
| `dlg_message_box_prefix_kind` | LIVE | 1098:28e4 |  |
| `dlg_message_box_prefixed` | STUB | 1098:28e4, 1608:0080 | has TODO |
| `dlg_create_dlg_template` | STUB | 1000:40aa, 1098:2a85, 1420:1f02 | has TODO |
| `dlg_append_template_item` | STUB | 1098:2c36 | has TODO |
| `dlg_compact_template` | STUB | 1098:2def | has TODO |
| `dlg_open_indirect` | STUB | 1098:124f, 1098:2e41, 1608:0184 | has TODO |
| `dlg_show_item` | STUB | 1100:08a6, 1100:08d3, 1608:00d0, 1608:0124 | has TODO |
| `dlg_hide_item` | STUB | 1100:08d3 |  |
| `dlg_hook_item_state` | LIVE | 1098:2829, 1100:0900 |  |
| `dlg_send_item_user_msg401` | STUB | 1100:0913, 1220:1832, 1608:0128 | has TODO |
| `dlg_get_window_text255` | STUB | 1098:2735, 1100:0970, 1608:00bc | has TODO |
| `text_next_char_inc` | LIVE | 1098:013d |  |
| `dlg_center_move_window` | STUB | 1098:0a0d, 1098:0aa4, 1098:0b33, 11f8:0000, 1608:00ac | has TODO |
| `dlg_center_move_window_padded` | STUB | 1098:0aa4 |  |
| `dlg_get_desktop_rect` | STUB | 1098:0bac, 1608:01a4 | has TODO |
| `dlg_return_zero_stub` | LIVE | 1008:0acb, 1098:0bcb |  |
| `dlg_return_false` | LIVE | 1068:0026, 1098:18f7 |  |
| `dlg_show_modal_listbox` | STUB | 1068:004c, 1068:0295, 1098:1909 | has TODO |
| `dlg_dispatch_def_dlg_proc` | STUB | 1098:1d56, 1608:01a8 | has TODO |
| `dlg_return_zero_byte` | LIVE | 1098:1f3d |  |
| `dlg_noop_slot_a` | NOOP | 1098:1fe0 |  |
| `dlg_noop_slot_b` | NOOP | 1098:1fee |  |
| `dlg_noop_slot_c` | NOOP | 1098:1ffc |  |
| `text_forward_enum_tail` | NOOP | 1008:09d3, 1098:0dc7 |  |
| `text_fill_bar_range` | STUB | 10d8:0000, 11e8:0504, 11e8:0912 | has TODO |
| `text_draw_colored_at_origin` | STUB | 10d8:002d, 10d8:00e0 | has TODO |
| `text_fill_text_background` | STUB | 10d8:0221 | has TODO |
| `text_noop0604` | STUB | 10d8:0604 | has TODO |
| `text_fill_bar_corners` | STUB | 10d8:0612, 11e8:0504 | has TODO |
| `text_bar_extent` | LIVE |  |  |
| `text_noop_width_hint` | LIVE | 11d0:0000 |  |
| `text_load_buffer_from_parts` | STUB | 11d0:00e4, 12d8:24fa | has TODO |
| `text_append_resource_text` | STUB | 11d0:0115 | has TODO |
| `text_prepare_popup_message` | STUB | 11d0:01f7 | has TODO |
| `text_expand_message_tokens` | LIVE | 11d0:022d, 12d8:24fa, 13c8:0000, 1420:42f6 |  |
| `text_str_to_upper` | LIVE | 1100:0b9d, 11e8:09f7 |  |

### `src/game.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `game_init` | LIVE | 1008:0000, 12d8:24ee, 12d8:3b9a, 12d8:42f6, 12d8:42f8, 12d8:42fa, 12d8:42fe, 12d8:4300, 12d8:44e2 |  |
| `game_new_difficulty_ok` | LIVE | 1028:0000, 10e8:0000, 1108:01bc, 1220:0156, 12d8:42f6, 1420:2a5a |  |
| `game_new_civ_count_ok` | LIVE | 10e8:0000, 11c8:0063, 1220:0156, 12d8:3b9a, 1420:2aa9, 1420:2ac1 |  |
| `game_new_alive_mask` | LIVE | 10e8:0000, 12d8:42f8 |  |
| `game_new_start_year_raw` | LIVE | 10e8:0000, 12d8:42fa |  |
| `game_new_start_year_signed` | LIVE | 1088:1dd0 |  |
| `game_new_player_civ_ok` | LIVE | 10e8:0000, 11f8:0080, 12d8:4300, 1420:2ad2, 1420:2ae5 |  |
| `game_new_map_param` | LIVE | 1028:0000, 1028:1d15, 10b8:079d, 1108:01bc, 11e0:0221, 12d8:44e2 |  |
| `game_new` | PARTIAL | 1008:0304, 1008:0d68, 1028:0000, 1028:0a93, 1028:1d15, 1088:0000, 10b8:079d, 10e8:0000, 1108:01bc, 1150:00c9, 11f8:0080, 1220:0156, 1220:16fd, 12d8:24ee, 12d8:42f6, 12d8:42fe | has TODO |
| `game_tick` | PARTIAL | 1008:0304, 1008:0329, 1008:0d68, 1088:0000, 1098:0157, 1108:0000, 12d8:24ee | has TODO |
| `game_turn_year_step` | LIVE | 1088:0000, 12d8:42fa |  |
| `game_turn_should_score` | LIVE |  |  |
| `game_uprising_due` | LIVE | 1088:0be9, 1110:16c0, 1110:18ce, 11c8:003d |  |
| `game_turn_tick` | PARTIAL | 1008:0000, 1040:068e, 1088:0000, 1088:0b6d, 1088:0be9, 1088:108a, 1088:13d5, 1088:1d99, 1088:1e32, 1110:03e9, 1138:0000, 1168:0000, 1178:0000, 1198:00a3, 11c0:3beb, 11e8:0075, 11f0:0000, 12d8:42f8 | has TODO |
| `game_process_game_end` | STUB | 1040:1137, 1088:0b6d, 1180:0000, 1188:07ad | has TODO |
| `game_spawn_uprisings` | PARTIAL | 1088:0be9, 1110:16c0, 1110:18ce, 1110:1dbd, 11c0:12c8, 11c8:003d | has TODO |
| `game_uprising_rng_scale` | LIVE | 1088:0be9, 11c8:003d |  |
| `city_process_disaster` | PARTIAL | 1030:0000, 1040:068e, 1088:0000, 1110:03ae, 11c8:003d, 1220:00d2, 1420:07f0, 1420:3efe, 1420:54fc, 1420:5500, 1420:5502, 1420:5503 | has TODO |
| `game_calc_power_score` | LIVE | 1088:108a |  |
| `game_update_power_rankings` | LIVE | 1088:108a, 12a8:000c |  |
| `game_update_status_panel` | PARTIAL | 1088:13d5, 1088:1554, 1098:0dda, 1098:0e0b, 10d0:0854, 10d0:0956, 11a8:0cb1, 11f8:0278, 1218:0956, 1218:0a06, 1220:00d2, 1420:172b, 1420:1744 | has TODO |
| `game_accum_pollution` | LIVE | 1028:1afd, 1088:1e32, 12d8:42f0 |  |
| `game_do_pollution_and_wonder_race` | PARTIAL | 1028:1afd, 1088:1e32, 1170:3181, 12d8:42f0 | has TODO |
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
| `sys_drain_input` | PARTIAL | 1038:0c1d, 1108:0000, 11c0:0395, 11f0:0000, 11f0:06d2, 11f0:4ea0, 12b0:004c, 1420:1d44 | has TODO |
| `game_calc_progress_threshold` | LIVE | 1088:1d99 |  |
| `unit_show_info_popup` | PARTIAL | 1098:0dda, 11f0:468c, 11f0:4754, 1218:0801, 1220:0156, 1420:62fc | has TODO |
| `unit_popup_is_end_of_turn` | LIVE | 1000:36b2, 1018:067b, 1030:0000, 1038:0bba, 1038:0c1d, 1080:0119, 1098:0000, 1098:00b5, 1098:0dda, 1098:0e0b, 1098:1104, 10d0:0854, 10d0:09a2, 10d8:00e0, 10e8:0000, 1100:0a33, 1110:0ff6, 1110:16c0, 1110:199b, 1180:0000, 11c8:00c4, 11f0:468c, 11f0:4808, 11f0:4a76, 11f8:0080, 11f8:0278, 1218:0abf, 1420:1a3c, 1420:47dd, 1420:47e9, 1420:47f6, 1420:4803, 1420:480b, 1420:481d, 1420:482f, 1420:483c, 1420:4845 |  |
| `unit_popup_has_veteran` | LIVE | 1420:4803 |  |
| `unit_popup_moves_full` | LIVE | 11c8:0063 |  |
| `unit_popup_moves_frac` | LIVE |  |  |
| `unit_popup_badge_needs_wrap` | LIVE |  |  |
| `combat_resolve_attack` | PARTIAL | 1050:0000, 1078:1480, 10d0:0854, 1110:0448, 1150:00c9, 11a8:0cb1, 11c0:1bc3, 11c0:231d | has TODO |
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
| `demo_show_demographics` | PARTIAL | 1018:0380, 1110:16c0, 1110:16ee, 1188:123a, 1188:220a, 1420:5500 | has TODO |
| `demo_rank_row` | STUB | 1188:220a, 1420:3ced | has TODO |
| `city_show_census_report` | STUB | 1018:0000, 1018:0380, 1018:1d5b, 1018:202a, 1030:0000 | has TODO |
| `census_show_tech_bars` | STUB | 1018:0000, 1018:0380, 1018:03f0, 11c8:008a | has TODO |
| `census_show_unit_list` | STUB | 1018:067b, 11c8:008a | has TODO |
| `census_show_terrain_counts` | STUB | 1018:09cc | has TODO |
| `trade_calc_bonus` | LIVE | 1038:0d09, 1110:1d99, 1170:3181, 11c8:02ec, 11e8:0dcf, 1420:5506 |  |
| `trade_route_value` | LIVE | 1038:0d09, 1420:5513 |  |
| `trade_deliver_caravan_goods` | PARTIAL | 1038:0d09, 1088:13d5, 11c0:16a4, 1220:00d2, 1420:5500 | has TODO |
| `diplomat_revolt_cost` | LIVE | 1168:12c1, 11b8:0000, 1420:5502 |  |
| `diplomat_bribe_cost` | LIVE | 1168:12c1, 11b8:07c2 |  |
| `diplomat_city_action` | PARTIAL | 11b8:0000, 11c8:003d, 12d8:3a0a | has TODO |
| `diplomat_bribe_unit` | PARTIAL | 1088:13d5, 1110:03ae, 11b8:07c2, 11c0:12c8, 11c0:16a4, 11c0:1bc3 | has TODO |
| `city_do_revolt` | PARTIAL | 1038:0000, 1110:171f, 11b8:0f26, 11c0:12c8, 1220:00d2, 1420:4168 | has TODO |
| `pic_play_shared` | PARTIAL | 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1030:79ce, 1030:7c28, 1030:7e57, 1038:0000, 1038:0c1d, 1038:0d09, 1040:068e, 1040:1137, 1080:099b, 1098:124f, 1170:0000, 1170:05af, 1170:05ca, 1188:0db8, 1188:123a, 11b8:0f26, 11e0:0000, 11f0:4ea0, 1420:5502 | has TODO |
| `pic_shared_city_ok` | LIVE | 1030:0000, 1420:5500, 1420:5502 |  |
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
| `civ_unit_turn_step` | PARTIAL | 1038:0000, 1038:0c1d, 1050:0000, 1078:4f0f, 1110:03ae, 1110:1461, 1128:0000, 11c0:0395, 11c0:0ec2, 11c0:12c8, 11c0:231d, 11c0:2c32, 11f0:0000, 11f0:44d4, 1220:00d2, 1420:62fc | has TODO |
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
| `sys_drain_peek_key_first` | LIVE | 1038:0c1d, 1108:0000, 11e8:02fb, 11f0:0000, 11f0:4ea0, 1420:1d44 |  |
| `sys_drain_peek_key_last` | LIVE |  |  |
| `sys_drain_peek_mouse_first` | LIVE |  |  |
| `sys_drain_peek_mouse_last` | LIVE |  |  |
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
| `city_found_free_slot` | LIVE | 1040:0000 |  |
| `city_found_initial_prod` | LIVE | 1040:0000 |  |
| `city_found_is_first` | LIVE | 1040:0000 |  |
| `city_found_trim_len` | LIVE | 1040:0000 |  |
| `tech_calc_cost` | LIVE | 1170:31ec |  |
| `tech_research_eligible` | LIVE | 1170:21b3 |  |
| `tech_research_roll_is_better` | LIVE | 1170:21b3, 12d0:0062 |  |
| `tech_grant_is_future` | LIVE | 1170:2aaa, 12a8:0022 |  |
| `tech_bit_word` | LIVE | 1170:3181 |  |
| `tech_bit_mask` | LIVE | 1170:3181 |  |
| `hut_found_city_site_ok` | LIVE | 11c0:2677 |  |
| `hut_tech_shuffle_ok` | LIVE | 11c0:2677 |  |
| `hut_tech_eligible` | LIVE | 11c0:2677 |  |
| `hut_gold_bonus` | LIVE | 11c0:2677 |  |
| `hut_barbs_ok` | LIVE | 11c0:2677 |  |
| `hut_barb_step` | LIVE | 11c0:2677, 11c8:0063 |  |
| `hut_barb_unit_kind` | LIVE | 11c0:2677 |  |
| `hut_friendly_unit_kind` | LIVE | 11c0:2677 |  |
| `tech_grant_stores_bit` | LIVE | 1170:2aaa |  |
| `tech_grant_is_library_source` | LIVE | 1170:2aaa |  |
| `tech_grant_library_spread_ok` | LIVE | 1170:2aaa |  |
| `tech_grant_barracks_obsolete_clear` | LIVE | 1170:2aaa |  |
| `tech_grant_from_other` | LIVE | 1170:2aaa |  |
| `tech_cost_roll_bound` | LIVE | 1170:21b3 |  |
| `game_eliminate_needed` | LIVE | 10e8:1b97 |  |
| `game_eliminate_alive_clear` | LIVE | 10e8:1b97 |  |
| `game_eliminate_seen_clear` | LIVE | 10e8:1b97 |  |
| `pedia_entry_kind_valid` | LIVE | 1068:06af |  |
| `pedia_res_for_selector` | LIVE | 1020:0000 |  |
| `pedia_build_variant` | LIVE | 1068:06af |  |
| `pedia_terrain_find_index` | LIVE | 1020:011b |  |
| `gov_announce_needed` | LIVE | 10e8:32c1 |  |
| `tech_conquest_gate` | LIVE | 1038:09bc |  |
| `tech_conquest_list_cap` | LIVE | 1038:09bc |  |
| `tech_conquest_eligible` | LIVE | 1038:09bc |  |
| `tech_conquest_cost_is_better` | LIVE | 1038:09bc |  |
| `tech_conquest_initial_best` | LIVE | 1038:09bc |  |
| `tech_conquest_has_any` | LIVE | 1038:09bc |  |
| `tech_conquest_player_picks` | LIVE | 1038:09bc |  |
| `tech_conquest_player_loses` | LIVE | 1038:09bc |  |
| `game_elimflag_low_mask` | LIVE | 1038:083d |  |
| `game_elimflag_high_mask` | LIVE | 1038:083d |  |
| `game_elimflag_msg_needed` | LIVE | 1038:083d |  |
| `game_elimflag_msg_id` | LIVE | 1038:083d, 1420:0746 |  |
| `game_elimflag_clear` | LIVE | 1038:083d |  |
| `city_plunder_value` | LIVE | 1038:0000, 1038:0901 |  |
| `game_end_needs_replay` | LIVE | 1088:0b6d, 1180:0000, 12d8:24ee |  |
| `game_end_shows_ceremony` | LIVE | 1040:1137, 1088:0b6d |  |
| `game_uprising_tile_ok` | LIVE | 1088:0be9, 1110:16c0, 1110:18ce, 1110:1dbd, 11c8:003d |  |
| `game_uprising_leader` | LIVE | 1088:0be9, 11c0:12c8, 11c8:0063 |  |
| `game_uprising_wave_due` | LIVE | 1088:0be9 |  |
| `game_pollution_spread_due` | LIVE | 1028:1afd, 1088:1e32 |  |
| `game_pollution_alarm_due` | LIVE | 1138:0000, 1420:1744 |  |
| `game_status_pollution_icon` | LIVE | 1088:13d5, 11a8:0cb1 |  |
| `game_status_pollution_count` | LIVE | 11c8:0063 |  |
| `game_status_progress_index` | LIVE | 1088:13d5, 1088:1554, 11a8:0cb1, 11c8:0063, 12d8:42fa |  |
| `game_turn_disaster_due` | LIVE | 1040:068e, 1088:0000, 11c8:003d, 12d8:42fe |  |
| `game_turn_treasury_warn` | LIVE | 1088:0000, 1220:00d2 |  |
| `status_tech_count_ok` | LIVE | 1088:1a82, 11c8:0063 |  |
| `status_tech_height_ok` | LIVE | 1088:1a82 |  |
| `demo_military_rate` | LIVE | 1188:123a |  |
| `demo_productivity` | LIVE | 1188:123a |  |
| `demo_family_raw` | LIVE | 1188:123a |  |
| `demo_life_clamp` | LIVE | 1188:123a, 11c8:0063 |  |
| `city_disaster_city_ok` | LIVE | 1030:0000, 1040:068e, 1088:0000, 11c8:003d, 1220:00d2, 1420:07f0, 1420:3efe, 1420:5502, 1420:5503 |  |
| `city_disaster_has_building` | LIVE | 1030:0000, 1110:03ae, 11c8:003d, 1220:00d2, 1420:07f0, 1420:54fc |  |
| `game_status_milestone_due` | LIVE | 1088:13d5, 1220:00d2, 1420:172b |  |
| `status_lcg_pick` | LIVE | 1088:1a82 |  |
| `gfx_tech_icon_valid` | LIVE | 1088:1a82, 11a8:0cb1 |  |
| `unit_found_join_ok` | LIVE | 11c0:16a4, 11e8:0dcf, 1420:5500, 1420:62ff |  |
| `city_spot_valid` | LIVE | 1110:16ee, 1110:180e, 11c0:0000, 11c0:0395, 11c8:00c4, 1420:5500, 1420:5502 |  |
| `unit_popup_has_pollution` | LIVE | 1110:199b, 11f0:468c, 1420:4845 |  |
| `unit_wait_blink_phase` | LIVE | 10d0:0854, 10d8:00e0, 1110:0448, 1110:0dba, 11c0:0ec2, 11e8:02fb, 11f0:0ec2, 1210:07ae, 1328:0012 |  |
| `unit_should_center_view` | LIVE | 1110:1461, 11f0:030c, 11f0:44d4 |  |
| `unit_popup_has_railroad` | LIVE | 1110:199b, 11f0:468c, 1420:481d, 1420:4828, 1420:482f, 1420:483c |  |
| `unit_popup_has_river` | LIVE | 1420:4828 |  |
| `unit_popup_has_irrigation` | LIVE | 1420:482f |  |
| `unit_popup_has_mine` | LIVE | 1420:483c |  |
| `diplomat_steal_blocked` | LIVE | 11b8:0000, 11c8:003d, 11f8:0080, 1420:5502 |  |
| `city_revolt_score` | LIVE | 1110:171f, 11b8:0f26, 11c0:12c8, 1220:00d2, 1420:4168 |  |
| `demo_pollution_index` | LIVE | 1188:123a, 1188:220a, 11c8:0334, 1420:5500 |  |
| `unit_reveal_seen_or` | LIVE | 11b8:0f26, 11c0:0395 |  |
| `game_turn_fix_year` | LIVE | 1088:0000, 12d8:42fa |  |
| `game_turn_quiz_due` | LIVE | 1110:03e9, 1178:0000 |  |
| `game_turn_save_due` | LIVE | 1198:00a3, 11c0:3beb |  |
| `game_turn_retirement_due` | LIVE | 1000:363e, 1000:36b2, 1088:0000, 1088:08e2, 1138:0000 |  |
| `game_turn_chronicle_due` | LIVE | 1088:0000, 1088:0994, 1088:0b6d, 1138:0000, 1150:00c9 |  |
| `combat_attacker_wins` | LIVE | 1050:0000, 11c0:1bc3, 11c8:003d |  |
| `diplomat_revolt_half_due` | LIVE | 11b8:0000, 1420:5502 |  |
| `diplomat_bribe_half_due` | LIVE | 11b8:07c2 |  |
| `unit_popup_moves_needs_frac` | LIVE | 11f0:468c |  |
| `unit_popup_build_tail_due` | LIVE | 1420:1a3c |  |
| `city_revolt_best_foreign` | LIVE | 11b8:0f26 |  |
| `city_tile_yield_combine` | LIVE | 1030:7c28, 1110:168f, 1110:16c0, 1110:199b, 1110:1c72, 1420:17d6 |  |
| `city_choose_production_pick` | LIVE | 1170:05ca, 1170:1f84, 1170:3181 |  |
| `game_spawn_uprisings_core` | PARTIAL | 1088:0be9, 1110:16c0, 1110:18ce, 1110:1dbd, 11c0:12c8, 11c8:003d, 1220:00d2 |  |
| `city_disaster_pick` | PARTIAL | 1030:0000, 1040:068e, 1088:0000, 1110:03ae, 11c8:003d, 1220:00d2, 1420:07f0, 1420:3efe, 1420:54fc, 1420:5502, 1420:5503 |  |
| `trade_apply_bonus` | LIVE | 1038:0d09, 1088:13d5, 1110:1d99, 11c0:16a4, 1220:00d2, 1420:5513 |  |
| `diplomat_affords_cost` | LIVE | 1110:03ae, 1168:12c1, 11b8:0000, 11b8:07c2, 1420:5502 |  |
| `map_set_irrigation_flag` | LIVE | 1030:7f1e, 1110:199b, 1110:1a2f, 1110:1a8f |  |
| `city_count_specialist_type` | LIVE | 1030:7fc1, 1030:7ffe |  |
| `city_normalize_labor_counts` | LIVE | 1030:802c, 11c8:0063, 1260:00de, 1420:5503 |  |
| `city_draw_labor_row_raw` | STUB | 1030:817e | has TODO |
| `city_draw_labor_row_icons` | STUB | 1018:1e9b, 1030:830c, 1030:8c02, 1080:0c5a, 11a8:0cb1 | has TODO |
| `city_draw_discovery_icon` | STUB | 1030:8529, 1420:98f4 | has TODO |
| `map_draw_minimap_dots` | STUB | 1030:8586 | has TODO |
| `map_plot_minimap_dot` | STUB | 1030:8847, 1128:177e | has TODO |
| `pic_show_info_message` | STUB | 1030:892a, 1220:00d2, 1220:0156, 12d8:0671 | has TODO |
| `pic_draw_framed_panel` | STUB | 1030:895c | has TODO |
| `city_show_find_and_center_dialog` | STUB | 1068:004c, 1110:03ae | has TODO |
| `ship_update_connection_bits` | LIVE | 1070:1cfa |  |
| `pic_draw_sunken_panel` | STUB | 1030:895c, 1030:8a60, 10d0:0956 | has TODO |
| `pic_draw_labeled_bar` | STUB | 1030:8c11, 10d8:0528, 11e8:0504 | has TODO |
| `tech_build_discovery_list` | STUB | 1030:8d33 | has TODO |
| `tech_find_discovery_pair` | LIVE | 1030:9061 |  |
| `city_blit_tile_slot` | STUB | 1030:79ce, 1030:90a8, 10d0:03fa, 1110:0448, 1110:1461 | has TODO |
| `pic_present_production_view` | STUB | 1030:9203, 1030:94d2, 10d0:0854, 1170:05ca | has TODO |
| `pic_blit_masked_icon` | STUB | 1030:925b, 11a8:0d6e | has TODO |
| `pic_create_info_window` | STUB | 1030:944c, 1608:00cc | has TODO |
| `pic_destroy_info_window` | STUB | 1030:94ba, 1070:0327, 1608:00dc |  |
| `pic_get_shared_text_buf` | LIVE | 1030:9588, 1030:9598 |  |
| `pic_noop_tail` | LIVE | 1030:9598 |  |
| `pic_show_window_restored` | STUB | 1030:95a8, 1608:00d0 | has TODO |
| `pic_arrow_enable_slot` | LIVE | 1030:9915 |  |
| `pic_set_arrow_enable` | LIVE | 1030:9915 |  |
| `pic_draw_bevel_edge` | STUB | 1030:96ad, 1030:982f, 1030:994b | has TODO |
| `pic_bevel_inner_h` | LIVE | 1030:994b |  |
| `pic_bevel_is_raised` | LIVE |  |  |
| `unit_unlink_stack` | LIVE | 1050:1105, 1050:11ad |  |
| `combat_blast_reveal` | STUB | 1030:74d5, 1050:13af, 1050:158a, 10d0:0956 | has TODO |
| `combat_blast_count` | LIVE | 1050:158a |  |
| `combat_should_irrigate` | LIVE |  |  |
| `combat_halve_pop` | LIVE |  |  |
| `census_show_foreign_affairs` | STUB | 1018:0000, 1018:1221, 10d0:0854, 10d8:01fa, 1170:2aaa, 11f8:0080 | has TODO |
| `census_foreign_is_known` | LIVE |  |  |
| `census_foreign_key_to_idx` | LIVE |  |  |
| `census_foreign_y_to_idx` | LIVE |  |  |
| `census_show_civ_detail` | STUB | 1018:1739, 1160:20c0, 11b8:0000 | has TODO |
| `census_show_civ_detail_overlay` | STUB | 1000:137f, 1018:176c | has TODO |
| `census_draw_civ_detail` | STUB | 1018:17db, 1038:0bba | has TODO |
| `census_detail_find_capital` | LIVE |  |  |
| `census_detail_tech_y` | LIVE |  |  |
| `ship_can_place_at` | LIVE | 1070:11f2, 1070:1e77, 1420:12b0 |  |
| `ship_validate_placement` | LIVE | 1070:1cfa, 1070:1dfb |  |
| `ship_poll_launch_confirm` | STUB | 1010:00d6, 1420:13D2 | has TODO |
| `ai_assess_civ_position` | STUB | 1078:0000, 1088:0000 | has TODO |
| `unit_queue_restore_orders` | LIVE | 1078:448f |  |
| `unit_queue_init_orders` | LIVE | 1078:48f8, 10e8:12b5 |  |
| `unit_queue_insert_order` | LIVE | 1078:1480, 1078:4937 |  |
| `unit_queue_make_gap` | LIVE | 1078:4a4c |  |
| `unit_queue_find_nearest` | LIVE | 1078:4b0d, 1110:1d99, 11c8:02ec |  |
| `unit_queue_cancel_near` | STUB | 1038:0000, 1040:0000, 1078:4c1a, 11c8:02ec | has TODO |
| `city_reselect_production` | STUB | 1078:4ca5, 1078:4d28, 1170:05ca, 1420:3f5c | has TODO |
| `ai_choose_coastal_dir` | LIVE | 1078:4dd2, 1110:16c0, 1110:18ce, 11e8:0dcf |  |
| `ai_retarget_sea_units` | STUB | 1050:0000, 1078:5b4e | has TODO |
| `city_reset_census_state` | LIVE | 1018:0380, 1100:0be5, 1420:1d53 |  |
| `game_fill_scaled_rect` | STUB | 1088:2335, 11e8:0d81, 1220:0f2f | has TODO |
| `game_calc_triangular_level` | LIVE | 1088:23bb, 1188:11ab |  |
| `ship_frame_end_hook` | LIVE | 1008:102d, 1070:12b5 |  |
| `city_find_by_tile_checked` | LIVE | 1110:16ee, 1110:1bab, 11c8:00c4, 1220:00d2 |  |
| `diplo_reset_contact_state` | STUB | 1160:207a, 1358:0008, 1358:000a, 1378:0066 | has TODO |
| `diplo_build_greeting_text` | STUB | 1160:2310, 1160:233c, 12d8:24fa | has TODO |
| `diplo_prompt_treaty_menu` | STUB | 1160:257a, 1160:25fa, 1378:006a | has TODO |
| `diplo_apply_peace_treaty` | STUB | 1160:25fa, 1168:0dda | has TODO |
| `diplo_tick_contact_anim` | STUB | 1160:26ef, 1378:0012 | has TODO |
| `diplo_setup_contact_portrait` | STUB | 1160:225c, 1160:2a38, 1160:2e03, 1160:2ee0, 1160:33fc, 11b8:0c3d | has TODO |
| `diplo_close_contact_gfx` | STUB | 1160:2e03 | has TODO |
| `diplo_draw_contact_frame` | STUB | 1160:2ee0 | has TODO |
| `diplo_draw_leader_clip` | STUB | 1160:33fc | has TODO |
| `diplo_calc_leader_frame` | LIVE | 1160:3487, 12d8:3b8a |  |
| `city_process_civ_turn` | STUB | 1088:010d, 1170:0000 | has TODO |
| `tech_choose_dialog_proc` | LIVE | 1170:204f |  |
| `status_open_result_port` | STUB | 1048:0334, 1048:035c, 1188:0000, 1188:013f, 1188:07ad, 1188:0b3e, 1188:0db8 | has TODO |
| `status_draw_world_map` | STUB | 1188:013f | has TODO |
| `score_draw_power_graph` | STUB | 1188:07ad | has TODO |
| `score_show_wonders` | STUB | 1188:0b3e | has TODO |
| `score_show_history_rank` | STUB | 1188:0db8 | has TODO |
| `diplo_play_city_effect` | STUB | 11b8:0c3d | has TODO |
| `city_show_view` | STUB | 11b0:0000, 11b8:14e5 | has TODO |
| `city_draw_water_tile` | STUB | 11b0:115c, 11b0:11bf, 11b0:1227, 11b0:1316, 11b0:1653, 11b0:1a5f | has TODO |
| `city_draw_terrain_tile` | STUB | 11b0:11bf | has TODO |
| `city_draw_cell_icon` | STUB | 11b0:1227 | has TODO |
| `city_draw_production_row` | STUB | 11b0:1316 | has TODO |
| `city_preload_view_icons` | STUB | 11b0:1653 | has TODO |
| `city_free_view_icons` | STUB | 11b0:1a5f |  |
| `unit_play_scroll_anim` | STUB | 11b0:1b63, 11b0:1fac, 11b0:237c | has TODO |
| `popup_play_forward_scroll` | STUB | 11b0:1fac, 1420:3fe6 | has TODO |
| `popup_play_reverse_scroll` | STUB | 11b0:237c | has TODO |
| `city_absorb_home_units` | LIVE | 11c0:0159, 13b8:0000, 13f0:000a |  |
| `city_absorb_saved_image` | LIVE |  |  |
| `city_absorb_staged_image` | LIVE |  |  |
| `unit_pick_stack_defender` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1 |  |
| `unit_count_type_in_stack` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1 |  |
| `unit_count_type_if_match` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1, 13b8:0004 |  |
| `unit_count_by_unitdef_field` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1, 13b8:0004 |  |
| `unit_sum_stack_combat_value` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1 |  |
| `unit_sum_stack_combat_callback` | LIVE | 11c0:1947, 11c0:1a81, 11c0:1cec, 11c0:1d32, 11c0:1db6, 11c0:1e03, 11c0:1e3c, 11c0:1ee1 |  |
| `unit_filter_stack_visibility` | LIVE | 11c0:0395, 11c0:1f42 |  |
| `unit_reset_stack_moves_if_flag` | LIVE | 11c0:1fdd |  |
| `unit_reset_air_stack_filter` | PARTIAL | 11c0:2105, 11c0:2151 | has TODO |
| `unit_reset_stack_and_redraw` | PARTIAL | 1110:0dba, 11c0:2105, 11c0:2151 | has TODO |
| `unit_found_city_if_land` | PARTIAL | 11c0:223c, 11c0:2287, 11c0:22d2 | has TODO |
| `unit_found_city_if_not_land` | PARTIAL | 11c0:2287 |  |
| `unit_found_city_if_sea` | PARTIAL | 11c0:22d2 |  |
| `unit_has_adjacent_unit` | LIVE | 11c0:2603 |  |
| `unit_pick_stack_dialog` | STUB | 11c0:3023, 11c0:3407, 11c0:3644, 11c0:3702 | has TODO |
| `unit_draw_stack_list` | STUB | 11c0:3023, 11c0:3407, 11c0:3644, 11c0:3702 | has TODO |
| `unit_pump_stack_dialog` | STUB | 11c0:3644 | has TODO |
| `unit_stack_dlg_proc` | LIVE | 11c0:3702 |  |
| `unit_reveal_if_visible` | STUB | 11f0:44d4 | has TODO |
| `unit_check_irrigate` | LIVE | 11f0:455f |  |
| `unit_ring_find_nearest` | LIVE | 11f0:4f38 |  |
| `ship_show_arrival_cinematic` | STUB | 11d8:0000, 11d8:087f | has TODO |
| `ship_show_defeat_cinematic` | STUB | 11d8:087f | has TODO |
| `ship_wait_pump_delay` | STUB | 11d8:0bd2, 11d8:0c38 | has TODO |
| `ship_noop_stub` | LIVE | 11d8:0c38 |  |

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
| `gfx_godpal_range_len` | LIVE | 1090:015e, 1090:02d2, 1090:0376, 1090:0670, 1090:06fd, 1200:00a8 |  |
| `gfx_godpal_flag_for_mono` | LIVE | 1090:0376, 1090:0488, 1090:0670, 1090:06fd |  |
| `gfx_godpal_range_end` | LIVE | 1090:0376 |  |

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
| `hof_rating_for` | LIVE | 1080:0435, 1080:0595, 1080:05b0, 1420:1589 |  |
| `hof_year_is_bc` | LIVE | 1080:0119, 1420:1522 |  |
| `hof_format_slot` | LIVE | 1080:0119, 1080:0435, 1080:05b0, 1098:0ed1, 1098:20f4, 1100:03ff, 11c8:03ac, 1420:1502, 1420:153e, 1420:1589 |  |
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
| `main_max_ticks` | LIVE |  |  |
| `main` | LIVE |  |  |

### `src/mainmenu.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `mainmenu_run` | PARTIAL | 1000:36b2, 1048:01cd, 1098:0b33, 1098:124f, 1098:1612, 10b0:005b, 11e0:0221, 1220:0156, 1420:435b | has TODO |
| `mainmenu_choice_is_valid` | LIVE | 1008:0329, 1130:000e, 11e0:0221, 1220:0000, 1420:435b |  |
| `mainmenu_item_count` | LIVE | 1000:36b2, 11e0:0221, 1220:0000, 1220:0156, 1420:435b |  |
| `mainmenu_item_at` | LIVE | 1000:36b2, 1220:0156, 1420:435b |  |
| `mainmenu_dispatch` | PARTIAL | 1008:0329, 1080:0023, 1080:0119, 10e8:0000, 1108:0243, 1130:000e, 11e0:0221, 1420:435b | has TODO |
| `menu_dispatch_count` | LIVE | 1048:01e4 |  |
| `menu_dispatch_find` | LIVE | 1048:01cd |  |
| `menu_dispatch_id_at` | LIVE |  |  |
| `menu_dispatch_target_at` | LIVE |  |  |
| `menu_command_group` | LIVE |  |  |
| `menu_dispatch_command` | PARTIAL | 1048:01cd | has TODO |
| `mainmenu_dialog_y_offset` | LIVE | 10b0:0461, 11e0:0221, 1220:0156 |  |
| `mainmenu_should_loop` | LIVE | 1000:36b2, 11e0:0000, 11e0:0221, 1220:0156, 12d8:44e2 |  |
| `menu_toggle_show_cmd` | LIVE | 1210:1134, 1218:077d |  |
| `menu_toggle_view_window` | LIVE | 1048:02af, 1208:0af1, 1210:025b, 1218:0386, 12b0:0004, 12b0:001c, 12b0:0034 |  |
| `mainmenu_idle_rebase_ticks` | LIVE | 10b0:005b, 11e0:0884, 11e0:090d |  |
| `mainmenu_resync_rebase_ticks` | LIVE | 10b0:005b, 11e0:090d |  |
| `mainmenu_procedural_rows` | LIVE | 1220:0000, 1220:0156, 1420:435b |  |
| `mainmenu_widget_row_y` | LIVE | 1190:0a29, 1220:0156 |  |
| `mainmenu_needs_rsc_frame` | LIVE | 1190:0a29, 1190:0fda |  |
| `mainmenu_host_palette_ok` | LIVE | 1098:124f, 1098:201d, 10b0:0461, 11e0:0000 |  |
| `mainmenu_event_is_dismiss` | LIVE | 11e0:0000, 1220:0156 |  |
| `mainmenu_modal_is_done` | LIVE | 11e0:0000, 11e0:0221 |  |
| `mainmenu_pump_events` | LIVE | 1098:124f, 1100:02e1, 1220:0156, 1220:0202 |  |
| `mainmenu_tick_is_due` | LIVE | 1100:02e1, 1220:0156, 1220:0202, 1220:0cfd, 1420:435b, 1420:4a41 |  |
| `mainmenu_menuz_timer_ok` | LIVE | 1100:02e1, 1220:0156, 1220:0202, 1220:0cfd, 1420:4a41 |  |
| `mainmenu_menuz_tail_ok` | LIVE | 11e0:0221, 1220:0156 |  |
| `menu_show_help_contents` | STUB | 1000:116a, 1000:1184, 1048:01cd, 1048:04e8, 1228:0002, 12b0:0004, 1608:016c | has TODO |
| `menu_dispatch_or_enable_range` | STUB | 1098:0bdd, 1098:0c97, 1608:0160, 1608:0164 | has TODO |
| `menu_palace_improve` | STUB | 1088:087f, 1158:0000, 1420:3176 | has TODO |
| `menu_palace_rebuild` | STUB | 1158:0000, 1158:08e7 | has TODO |
| `menu_palace_close` | STUB | 1158:123b, 11d8:0000 | has TODO |
| `menu_palace_calc_icon` | LIVE | 1158:1277 |  |
| `menu_palace_draw_icon` | STUB | 1158:140c | has TODO |
| `menu_palace_icon_px` | LIVE |  |  |
| `menu_palace_icon_py` | LIVE |  |  |
| `menu_run_radio_dialog` | STUB | 11e0:0a2a, 12b0:009c | has TODO |
| `menu_radio_dlg_proc` | LIVE | 11e0:0b73 |  |

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
| `map_init` | STUB | 10d0:0000, 1208:0af1, 1210:025b, 1218:0386, 1420:b394 | has TODO |
| `map_wndproc_main` | LIVE | 1008:0329, 1008:0d68, 1098:06f8, 10d0:0854, 1110:03e9, 1208:0054, 1208:0af1 |  |
| `map_wndproc_small` | LIVE | 1110:03ae, 1110:03e9, 1210:0000, 1210:025b, 1210:09b4 |  |
| `map_wndproc_status` | LIVE | 1088:13d5, 11f8:0278, 1218:0000, 1218:0386, 1218:0801, 1218:0956 |  |
| `map_timer_proc` | STUB | 1008:0000, 1008:0329, 1008:0d68, 1088:0000, 1208:0054, 1208:0af1 | has TODO |
| `map_timer_interval_ms` | LIVE | 1008:0000, 1008:0329, 1208:0054, 1208:0af1, 1208:0e7b |  |
| `map_shutdown` | NOOP |  |  |
| `wdwstatus_calc_upper_rect` | LIVE | 1000:137f, 1088:1479, 1218:0956 |  |
| `wdwstatus_calc_lower_rect` | LIVE | 1088:1536, 1218:0a06 |  |
| `wdwstatus_is_dirty` | LIVE | 1218:063e, 1218:0801 |  |
| `wdwstatus_frame_metrics` | LIVE | 1218:0956 |  |
| `path_find_corridor` | LIVE | 1128:0000, 1128:10de, 1420:62fc |  |
| `path_is_passable` | LIVE | 1110:168f, 1128:0000, 1128:081c, 1128:0ea3 |  |
| `map_is_sea_land` | LIVE | 1110:16c0, 1128:0000 |  |
| `map_is_occupied` | LIVE | 1110:18ce, 1128:0000, 1420:62fc |  |
| `map_is_city_blocked` | LIVE | 1110:18ce, 1110:199b, 1128:0000, 1420:5500 |  |
| `map_ocean_clear_depth` | LIVE | 1110:1dbd, 1128:0000 |  |
| `map_resolve_direct` | LIVE | 1110:1c4b, 1128:0000 |  |
| `map_reveal_center` | STUB | 1110:1461, 11f0:44d4 | has TODO |
| `map_spot_city` | STUB | 1110:16ee, 1110:180e, 11c0:0000, 11c0:0395, 11c8:00c4, 1420:5500 | has TODO |
| `text_draw_status_row` | PARTIAL | 1088:13d5, 1088:1479, 1088:1536, 1098:00b5, 11f8:0278, 1218:0956 | has TODO |
| `text_draw_with_font` | LIVE | 1008:0e99, 11f8:0278, 1218:0956 |  |
| `status_format_main_row` | PARTIAL | 1088:1dd0, 1098:124f, 1138:0000, 11d0:00e4 | has TODO |
| `status_format_second_row` | PARTIAL | 1038:0bba, 1088:1dd0, 11b0:0000, 12d8:24fa | has TODO |
| `status_format_gold_row` | PARTIAL | 1088:1dd0, 10d8:03fa, 11a8:0cb1, 11b8:14e5, 11e8:02fb | has TODO |
| `map_open_main` | STUB | 1208:0af1 | has TODO |
| `map_open_small` | STUB | 1210:025b | has TODO |
| `map_open_status` | STUB | 1088:13d5, 1218:0386, 1218:0801 | has TODO |
| `map_refresh_status` | STUB | 1088:13d5, 10d0:0854, 11f0:468c, 1218:0801 | has TODO |
| `map_center_on_tile` | STUB | 1110:03ae, 1110:03e9 | has TODO |
| `map_refresh_all_views` | STUB | 1088:13d5, 1110:03e9, 1208:17da, 1210:09b4, 1408:00cc | has TODO |
| `mainport_apply_palette` | STUB | 1090:0376, 1200:00a8 | has TODO |
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
| `map_repaint_needed` | LIVE | 1208:17da, 1208:1866 |  |
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
| `map_get_ocean_or_cont_val` | LIVE | 1028:0a93, 1110:16c0, 1110:1d99, 1110:1dbd, 12d8:4334, 12d8:43b4 |  |
| `path_link_block` | LIVE | 1128:0ea3 |  |
| `path_link_table_idx` | LIVE | 1028:12c6, 1128:0ea3 |  |
| `path_link_has_link` | LIVE | 1128:0ea3 |  |
| `path_link_is_better` | LIVE | 1128:0ea3, 11c8:025d |  |
| `path_link_corner_ok` | LIVE | 1110:16c0, 1128:0ea3, 1128:16d0 |  |
| `path_link_entry_budget` | LIVE | 1028:12c6, 1128:0ea3, 1128:10de, 1128:16d0 |  |
| `map_ocean_is_navigable` | LIVE | 1110:1dbd, 1128:0651 |  |
| `map_ocean_is_spawnable` | LIVE | 1088:0be9, 1110:1dbd |  |
| `map_ocean_is_coastal` | LIVE | 1040:03f6, 1110:1dbd |  |
| `map_city_coastal_should_set` | LIVE | 1040:03f6 |  |
| `map_cont_size_class` | LIVE | 10e8:212a, 1110:1dbd |  |
| `map_cont_size_remainder` | LIVE | 10e8:212a |  |
| `path_route_cost_is_better` | LIVE | 1128:081c, 11c8:02ec |  |
| `path_route_domain_mismatch` | LIVE | 1128:081c |  |
| `path_route_needs_fallback` | LIVE | 1128:081c |  |
| `status_headline_needs_short` | LIVE | 1038:0bba, 1068:0958, 1138:0000 |  |
| `status_main_needs_city_scan` | LIVE | 1138:0000 |  |
| `status_main_is_cabinet` | LIVE | 1018:24c5, 1138:0185, 1138:01a3, 1420:2f96 |  |
| `status_headline_needs_prefix` | LIVE | 1038:0bba, 1138:03c0, 1138:03fe |  |
| `status_flash_is_flash` | LIVE | 1138:049e, 1138:04a7, 1138:04a9, 11c8:003d, 1420:2fd8 |  |
| `status_main_city_pick_ok` | LIVE | 1138:00df, 1138:00e9 |  |
| `status_gold_needs_city_view` | LIVE | 11b0:0000, 11b8:14e5, 11e8:02fb |  |
| `status_second_name_checksum` | LIVE | 1000:1716, 11b0:0000, 12d8:24fa |  |
| `path_corridor_visited_idx` | LIVE | 1128:10de |  |
| `path_corridor_iter_limit` | LIVE | 1128:10de |  |
| `map_dir_to_dxdy` | LIVE | 1110:1c4b |  |
| `map_find_unit_at` | LIVE | 1110:180e, 12d8:4304, 1420:62fc |  |
| `map_occupancy_has_road` | LIVE | 1110:197a, 1110:199b, 1128:10de |  |
| `map_occupancy_is_city` | LIVE | 1110:199b, 11c0:23fd, 11c0:2437 |  |
| `map_center_origin_for_tile` | LIVE | 1110:03ae, 1110:03e9 |  |
| `map_tile_is_seen` | LIVE | 1208:1534, 1408:00cc |  |
| `text_status_row_needs_clip` | LIVE | 11f8:0278, 1218:0956 |  |
| `map_set_view_scale_and_origin` | LIVE | 1098:296f |  |
| `map_scale_rect_in_place` | LIVE | 1098:29e1 |  |
| `map_remove_unit_from_tile` | PARTIAL | 1050:11ad, 1078:1480, 1110:1767, 1110:17bc, 11c0:16a4 | has TODO |
| `map_set_unit_present_flag` | LIVE | 1110:1906, 1110:1941, 11b8:0f26 |  |
| `map_clear_unit_present_flag` | LIVE | 1110:1941 |  |
| `map_has_road` | LIVE | 1030:7c28, 1110:197a, 1128:10de |  |
| `map_get_mirrored_occupancy_flags` | LIVE | 1110:04d0, 1110:19e0 |  |
| `map_set_occupancy_bits` | LIVE | 1030:7f1e, 1040:0000, 1110:1a8f, 1170:0241 |  |
| `map_is_city_site_valid` | LIVE | 10e8:1814, 1110:1c72, 1110:1cd9 |  |
| `map_get_mirrored_continent_id` | LIVE | 1110:1d71, 1110:1d99 |  |
| `map_build_shade_pattern` | STUB | 10d0:0cfa, 1110:1e17, 1330:0008 |  |
| `map_compose_tile_icon` | STUB | 1110:04d0, 1118:0000, 1118:04b4, 11a8:0328 | has TODO |
| `map_get_tile_cache_key` | STUB | 1110:055d, 1118:04b4, 1420:087a | has TODO |
| `map_cache_civ_icons` | STUB | 1158:041a, 1158:09a5, 1158:14b7 | has TODO |
| `map_free_civ_icons` | STUB | 08e7:0ff2, 1158:07d6, 1158:17ad | has TODO |
| `map_wrap_step_dist_scaled` | LIVE | 11c8:025d, 11c8:02a4, 11c8:02ec |  |
| `map_reveal_if_visible` | STUB | 1110:1461, 11f0:447f, 12d8:4300, 12d8:4302 | has TODO |
| `tile_can_irrigate` | LIVE | 1120:02dc, 11f0:45db |  |
| `tile_is_on_screen` | STUB | 11f0:4e26, 1208:2061, 1330:4010, 1408:00c6 | has TODO |
| `tile_screen_px` | LIVE | 1128:177e, 11f0:4e26, 1208:2061 |  |
| `tile_screen_py` | LIVE |  |  |

### `src/newgame.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `newgame_pick_difficulty` | STUB | 10e8:0000, 1220:0156, 12d8:42f6, 1420:2a5a | has TODO |
| `newgame_pick_competition` | STUB | 10e8:0000, 1220:0156, 12d8:3b9a, 12d8:42f8, 1420:2aa9, 1420:2ac1, 1420:2ac3 | has TODO |
| `newgame_civ_count_from_index` | LIVE | 12d8:3b9a |  |
| `newgame_alive_mask` | LIVE | 12d8:42f8 |  |
| `newgame_pick_tribe` | STUB | 10e8:0000, 11f8:0080, 1220:0156, 12d8:4300, 1420:2ad2, 1420:2ae5 | has TODO |
| `tribe_name_count` | LIVE | 11f8:0080 |  |
| `tribe_leader_count` | LIVE |  |  |
| `tribe_adj_count` | LIVE |  |  |
| `tribe_get_name_list` | LIVE | 10e8:0000, 1100:078a, 11f8:0080, 1420:2ad2, 1420:2ae5 |  |
| `tribe_get_leader_name` | LIVE | 10e8:0000, 11f8:0080 |  |
| `tribe_get_adjective` | LIVE | 10e8:0000, 11f8:0080 |  |
| `newgame_unit_font_fixed_count` | LIVE | 10e8:2d46 |  |
| `newgame_unit_font_alt_count` | LIVE | 10e8:2d46 |  |
| `newgame_unit_font_fixed_at` | LIVE | 10e8:2d46 |  |
| `newgame_unit_font_alt_at` | LIVE | 10e8:2d46 |  |
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
| `newgame_start_dist_score` | LIVE | 10e8:1d48 |  |
| `newgame_start_terrain_bonus` | LIVE | 10e8:1d48 |  |
| `newgame_start_continent_bonus` | LIVE | 10e8:212a |  |
| `newgame_start_needs_extra_settler` | LIVE | 10e8:1d48 |  |
| `newgame_start_should_pick` | LIVE | 10e8:1d48 |  |
| `newgame_civ_note_early_res` | LIVE | 10e0:077f |  |
| `newgame_civ_note_player_res` | LIVE | 10e0:07e8 |  |
| `newgame_civ_note_title` | LIVE | 1420:29cd, 1420:29e9 |  |
| `newgame_maybe_show_settler_hint` | STUB | 10e0:005b, 11f0:08ff, 1420:2998 | has TODO |
| `newgame_patch_command_to_control` | LIVE | 10e0:03a2, 10e0:06fd |  |
| `newgame_build_icon_tables` | STUB | 10e8:231f | has TODO |
| `newgame_refresh_civ_icons` | STUB | 10e8:31aa, 1420:0128, 1420:0130, 1420:2a52 | has TODO |
| `newgame_extract_icon_at` | STUB | 10e8:33b9, 11a8:05c7 | has TODO |

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
| `rsc_decode_gdat` | STUB | 1420:17d6 | has TODO |
| `rsc_decode_kdat` | STUB |  | has TODO |
| `rsc_decode_cvpc` | STUB |  | has TODO |
| `rsc_strlist_count` | LIVE | 1190:1953 |  |
| `rsc_strlist_get` | LIVE |  |  |
| `rsc_text_find` | LIVE | 1420:2a5a, 1420:435b |  |
| `rsc_gdat_detail` | STUB | 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 | has TODO |
| `rsc_kdat_detail` | STUB |  | has TODO |
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
| `rsc_gdat_stride` | STUB | 1420:12b8, 1420:12bc, 1420:17d6, 1420:17d8 | has TODO |
| `rsc_kdat_stride` | STUB |  | has TODO |
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
| `res_table_is_known` | LIVE |  |  |
| `res_known_cstr` | LIVE |  |  |
| `res_get_string_copy_from_blob` | LIVE | 1100:0053, 1100:078a |  |
| `res_get_string_raw_from_blob` | LIVE | 1100:0053, 1100:078a |  |
| `res_copy_static_cstr` | LIVE |  |  |
| `res_raw_static_pascal` | LIVE |  |  |
| `res_get_string_copy` | PARTIAL | 1000:299e, 1100:078a, 1190:09ed, 1190:0aea, 11f8:0080, 13f8:0000 | has TODO |
| `res_get_string_raw` | PARTIAL | 1100:078a, 1190:0aea, 11f8:00c5, 13f8:0000 | has TODO |
| `res_blob_entry_kind` | LIVE | 1100:078a |  |
| `res_free_resource_loop` | STUB | 1100:0263, 1608:0038 | has TODO |
| `res_find_and_load` | STUB | 1100:028a, 1420:54fa, 1608:0030, 1608:0034 | has TODO |

### `src/save.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `save_prompt_open` | STUB | 1130:0075 | has TODO |
| `save_prompt_save` | STUB | 1130:0194 | has TODO |
| `save_init_filespec` | LIVE | 1000:36b2, 1000:3722, 1080:0023, 1080:0092, 1140:0095, 1420:14e8, 1420:14f5 |  |
| `save_load_dialog` | STUB | 10b0:0000, 10f8:0000, 1130:000e, 1130:0075, 11e0:0221 | has TODO |
| `save_save_dialog` | STUB | 1098:26d4, 1130:0194, 1180:0000, 1198:00a3 | has TODO |
| `save_save_game` | LIVE | 1000:2928, 1000:363e, 1098:26d4, 10f8:02bd, 1130:0194, 1198:00a3 |  |
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
| `save_pack` | LIVE | 10f8:02bd, 10f8:05da, 10f8:090c |  |
| `save_unpack` | LIVE | 10f8:0000, 10f8:040a, 1140:019d |  |
| `save_pack_buffer` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
| `save_unpack_buffer` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
| `save_build_filename` | LIVE | 1000:2928, 1000:363e, 1000:36b2, 1080:0119, 1198:0000, 1198:00a3, 1420:173c |  |
| `save_rll_encode` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
| `save_rll_decode` | LIVE | 10f8:0000, 10f8:02bd, 10f8:040a, 10f8:05da |  |
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
| `save_try_open_file` | LIVE | 1000:2ad9, 1140:00e6, 1140:0139, 1180:00f1 |  |
| `save_allow_file_op` | LIVE | 1140:0126 |  |
| `save_file_seek` | LIVE | 1000:161c, 1140:0139, 1140:030f, 1140:037c |  |
| `save_file_tell_nonzero` | LIVE | 1000:192e, 1140:034a |  |

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
| `file_write_buffered` | LIVE | 1000:2c9c, 1000:2cba, 1000:2e85, 1000:3fc4, 1420:4cd0 |  |
| `file_put_char_buffered` | LIVE | 1000:2152, 1000:2cd8, 1000:3fc4 |  |
| `file_emit_char` | LIVE | 1000:2cba, 1000:2cd8, 1000:2e85 |  |
| `file_buffered_is_direct` | LIVE | 1000:2152, 1000:2cd8, 1000:2e85, 1420:4cd0 |  |
| `file_is_text_mode` | LIVE | 1000:1750, 1000:33c0, 1000:3e70, 1000:3fc4 |  |
| `dos_fd_alloc` | LIVE |  |  |
| `dos_fd_get` | LIVE |  |  |
| `dos_os_fd` | LIVE |  |  |
| `dos_errno_e0010` | LIVE |  |  |
| `dos_errno_e4e7a` | LIVE |  |  |
| `dos_map_error` | LIVE | 1000:151a, 1420:4e7c |  |
| `dos_map_error_lookup` | LIVE | 1000:151a, 1420:4e7c |  |
| `dos_is_char_device` | LIVE | 1000:1566, 1000:1750, 1000:3fc4, 1000:4252, 1420:4fc4, 1420:4fc8 |  |
| `dos_is_char_device_bit` | LIVE | 1000:1566, 1000:1750, 1000:3fc4, 1420:4fc4 |  |
| `dos_error_from_errno` | LIVE |  |  |
| `dos_seek` | LIVE | 1000:151a, 1000:161c |  |
| `dos_seek_origin_ok` | LIVE | 1000:151a, 1000:161c |  |
| `dos_read` | LIVE | 1000:151a, 1000:1566, 1000:1750, 1000:33c0, 1140:019d, 1420:4fc4 |  |
| `dos_read_is_blocked` | LIVE | 1000:151a, 1000:1566, 1000:1750, 1420:4fc4 |  |
| `dos_unlink` | LIVE | 1000:151a, 1000:19a6 |  |
| `dos_file_attr` | LIVE | 1000:151a, 1000:1f5c, 1000:2ad9 |  |
| `dos_file_attr_func_ok` | LIVE | 1000:1f5c, 1000:2ad9 |  |
| `dos_close` | LIVE | 1000:151a, 1000:1f7c, 1000:1fa6 |  |
| `dos_is_eof` | LIVE | 1000:151a, 1000:201e |  |
| `dos_eof_at_end` | LIVE | 1000:201e |  |
| `dos_create` | LIVE | 1000:1f5c, 1000:2aac, 1000:2ad9, 1000:2c4a, 1140:0139 |  |
| `dos_truncate` | LIVE | 1000:2ac7, 1000:2ad9 |  |
| `dos_open` | LIVE | 1000:151a, 1000:2ad9, 1000:2c4a |  |
| `dos_open_mode_map` | LIVE | 1000:2c4a |  |
| `dos_open_table_word` | LIVE | 1000:2c4a |  |
| `dos_write` | LIVE | 1000:151a, 1000:1566, 1000:3e70, 1000:3fc4, 1140:0245, 1420:4fc8 |  |
| `dos_write_is_blocked` | LIVE | 1000:151a, 1000:1566, 1000:3fc4, 1420:4fc8 |  |
| `dos_ioctl` | PARTIAL | 1000:151a, 1000:4252 |  |
| `dos_ioctl_is_direct` | LIVE | 1000:151a, 1000:4252 |  |
| `dos_get_current_drive` | LIVE | 1000:1252 |  |
| `dos_get_current_drive_index` | LIVE | 1000:13e6 |  |
| `dos_get_current_dir` | LIVE | 1000:139c |  |
| `sys_tick_scaled_from` | LIVE | 10b0:005b, 10b0:008a, 10b0:0461, 1100:02e1 |  |
| `sys_get_tick_scaled` | LIVE | 1100:02e1 |  |
| `sys_build_current_dir_path` | LIVE | 1000:2826 |  |
| `sys_init_paths` | LIVE | 1228:0002, 1228:0004 |  |
| `sys_current_drive` | LIVE | 1228:0002 |  |
| `sys_base_path` | LIVE | 1228:0004 |  |
| `sys_yield` | LIVE | 1098:0157, 10b0:0000 |  |
| `sys_delay_ms` | LIVE | 1008:0d68, 1098:0157, 10b0:0000, 1100:02e1, 1420:1f40 |  |
| `sys_wait_next_tick` | LIVE | 1090:0376, 1098:0157, 10b0:0000, 10b0:0461, 10d0:0854, 10d0:0956, 1100:02e1, 1150:00c9, 1420:1f40 |  |
| `sys_pump_messages` | LIVE | 10b0:0000, 1108:0000, 11c0:0ec2, 11e8:02fb, 1328:0012 |  |
| `sys_event_requests_quit` | LIVE |  |  |
| `sys_poll_quit` | LIVE | 1008:0304, 1088:0000, 1098:062f, 1220:0156, 12d8:24ee |  |
| `sys_wait_for_input` | PARTIAL | 1008:0d68, 1018:0380, 1018:067b, 1018:0be0, 1018:1d5b, 1018:208f, 1030:0000, 1038:0c1d, 1080:0119, 1080:06ff, 1098:0157, 10e8:0000, 1180:0000, 11f0:468c, 11f0:4ea0 | has TODO |
| `main_pump_message` | PARTIAL | 1008:0329, 1008:0a8d, 1098:0157, 1098:02d1, 1108:0000, 11e8:02fb, 1208:0000, 12b0:0004, 1608:0134, 1608:013c, 1608:0140 | has TODO |
| `sys_wait_sdl_is_key` | LIVE | 1038:0c1d |  |
| `sys_wait_sdl_is_lbutton` | LIVE | 1038:0c1d |  |
| `sys_drain_sdl_should_drop` | LIVE | 11f0:4ea0 |  |
| `main_key_to_res_char` | LIVE | 1098:02d1, 1208:0000, 1420:1400, 1420:142c |  |
| `sys_get_client_rect` | PARTIAL | 1000:137f, 1218:0956, 1218:0a06, 1608:000c, 1608:0010, 1608:0100, 1608:010c | has TODO |
| `sys_client_rect_is_valid` | LIVE | 1218:0956, 1218:0a06 |  |
| `sys_rect_span` | LIVE | 1000:137f, 1218:0956 |  |
| `sys_set_rect` | LIVE | 1218:0956, 1218:0a06, 1608:0100 |  |
| `sys_inflate_rect` | LIVE | 1218:0956, 1608:010c |  |
| `status_measure_row` | PARTIAL | 1088:1460, 1098:0dda, 1138:0269, 11f0:4754, 1218:0956, 1220:0b88 | has TODO |
| `status_row_stride` | LIVE | 1138:0269 |  |
| `sys_check_runtime_memory` | LIVE | 11e8:0075, 1420:43b4, 1420:4473, 1420:44ff |  |
| `sys_check_free_memory` | LIVE | 11e8:0000, 1420:43b8 |  |
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
| `sys_fatal_abort` | STUB | 1000:37b2, 1000:4124 | has TODO |
| `sys_fatal_basename` | LIVE | 1000:37b2, 1000:4124 |  |
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
| `mem_dispose_ptr` | LIVE | 1008:0329, 1028:0000, 10f8:02bd, 1100:060b |  |
| `mem_dispose_handle` | LIVE | 1100:0652 |  |
| `mem_realloc_handle` | LIVE | 1100:030f |  |
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
| `file_temp_next_num` | LIVE | 1000:1210, 1000:16a5 |  |
| `sys_pump_mode_select` | LIVE | 1108:0000 |  |
| `sys_pump_scale_coord` | LIVE | 1038:0c1d, 1108:0000 |  |
| `sys_runtime_warn_style_first` | LIVE | 11e8:0075, 1420:44f7 |  |
| `sys_runtime_warn_style_final` | LIVE | 1420:4578 |  |
| `sys_runtime_warn_title_first` | LIVE | 1420:44f7, 1420:4578 |  |
| `sys_runtime_warn_body_first` | LIVE | 1420:4473 |  |
| `sys_runtime_warn_title_final` | LIVE | 1420:4578 |  |
| `sys_runtime_warn_body_final` | LIVE | 1420:44ff |  |
| `sys_runtime_check_order` | LIVE | 1088:0000 |  |
| `sys_font_face_index_ok` | LIVE | 1008:0e99 |  |
| `sys_font_slot_ok` | LIVE | 1008:062f, 11e8:0d1d |  |
| `sys_font_install_slot` | STUB | 1008:062f, 1008:0e12, 1420:00a4 | has TODO |
| `dos_get_disk_free` | STUB | 1000:11d6, 10b8:04df, 10b8:0c1c | has TODO |
| `sys_noop_default` | NOOP | 1000:127e, 1000:127f |  |
| `file_get_position` | STUB | 1000:1316, 1000:161c, 1000:2578, 1190:0501 | has TODO |
| `file_set_position` | STUB | 1000:1346, 1000:161c, 1000:2152, 1000:24f6 | has TODO |
| `dos_get_date` | STUB | 1000:13bc, 1000:18d4 | has TODO |
| `dos_get_time` | STUB | 1000:13d1, 1000:18d4 | has TODO |
| `sys_stack_push_copy` | PARTIAL | 1000:14f8, 10b0:008a |  |
| `dos_propagate_error` | LIVE | 1000:1210, 1000:151a, 1000:1553 |  |
| `dos_tell` | STUB | 1000:161c, 1000:192e | has TODO |
| `file_flush_all_entries` | STUB | 1000:2152, 1000:2222 | has TODO |
| `file_parse_mode` | LIVE | 1000:2266, 1000:2322, 1000:4030 |  |
| `file_open_entry` | STUB | 1000:1566, 1000:2322, 1000:243d, 1000:2ad9, 1000:3496 | has TODO |
| `file_alloc_entry` | LIVE | 1000:23f3 |  |
| `file_fopen` | STUB | 1000:2322, 1000:243d, 1190:0501 | has TODO |
| `file_text_newline_adjust` | LIVE | 1000:2474, 1000:24f6, 1000:2578 |  |
| `file_seek` | STUB | 1000:1346, 1000:161c, 1000:24f6 | has TODO |
| `file_tell` | STUB | 1000:1316, 1000:2578 | has TODO |
| `file_flush_buffered_outputs` | STUB | 1000:2152, 1000:265a, 1000:2698, 1000:272a | has TODO |
| `file_fill_buffer` | STUB | 1000:2698, 1000:272a, 1000:33c0 | has TODO |
| `heap_free_current_hook` | STUB | 1000:2098, 1000:3496, 1000:4239, 1000:4a52 | has TODO |
| `heap_get_anchor_off` | LIVE | 1000:441e, 1000:468c |  |
| `heap_load_anchor` | LIVE | 1000:468c, 1000:46f1, 1000:4870 |  |
| `heap_alloc_far` | STUB | 1000:46f1, 1000:498c | has TODO |
| `heap_try_alloc_block` | STUB | 1000:47f2 | has TODO |
| `heap_free_far` | STUB | 1000:4870, 1000:4a52 | has TODO |
| `heap_grow_chunk` | STUB | 1000:4931 | has TODO |
| `mem_release_handle` | STUB | 1000:4961 | has TODO |
| `mem_alloc_with_mode` | STUB | 1000:41e4, 1000:46f1, 1000:498c | has TODO |
| `mem_free_with_mode` | STUB | 1000:4239, 1000:4870, 1000:4a52 | has TODO |
| `rng_next15` | LIVE | 1000:1729, 10b0:151b, 1100:03ea |  |
| `mem_get_global_size` | STUB | 1068:1dd7, 1100:0212, 1100:0ab7, 1608:0014 | has TODO |
| `mem_get_global_size_alt` | STUB | 1100:0ab7 |  |
| `sys_poll_lbutton_down` | STUB | 1100:0a5d | has TODO |
| `sys_reserved_stub` | LIVE | 1100:0a20 |  |
| `sys_wait_date_delay` | STUB | 1000:18d4, 1008:0fcb | has TODO |
| `sys_trace_begin` | NOOP | 1098:016a, 1110:0448 |  |
| `sys_trace_end` | NOOP | 1098:0178, 1420:2e79, 1420:4957 |  |
| `sys_trace_mark` | NOOP | 1098:01a2, 1420:48b2 |  |
| `sys_pump_mouse` | STUB | 1098:03e3 | has TODO |
| `mem_alloc_zeroed` | LIVE | 1140:03ae, 1140:03e0, 1608:0000, 1608:0008 |  |
| `mem_free_handle` | LIVE | 1140:03e0 |  |
| `sys_show_memory_stats` | STUB | 11e8:0000, 11e8:0075, 11e8:012a | has TODO |

### `src/title.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `title_play_intro` | PARTIAL | 1098:1104, 1098:124f, 10b0:005b, 10b0:008a, 10b0:0287, 10b0:0461, 1150:00c9, 11e0:0000, 1220:0156, 1420:4a41 | has TODO |
| `title_play_anim_loop` | PARTIAL | 1030:0000, 1098:1104, 1098:124f, 1098:1612, 10b0:005b, 10b0:008a, 10b0:0287, 10b0:0461, 11e0:0000 | has TODO |
| `title_decode_frame` | STUB | 1018:015d, 1088:1dd0, 1138:0000, 1190:0a29, 11b0:0000, 11b8:14e5 | has TODO |
| `pic_decompress_chunk` | LIVE | 10f8:040a, 10f8:05da, 1420:23ce |  |
| `pic_read_header` | STUB | 10d0:0956, 1140:019d, 1420:2300, 1420:2372 | has TODO |
| `title_load_opening_frame` | STUB | 1030:0000, 10d0:0956, 1190:0000, 11e0:0000 | has TODO |
| `title_event_is_skip` | LIVE | 1030:0000, 10b0:0461, 11e0:0000 |  |
| `title_poll_skip` | LIVE | 10b0:0461, 11e0:0000 |  |
| `title_resolve_asset` | LIVE |  |  |
| `title_max_ms` | LIVE |  |  |
| `title_play_intro_sdl` | LIVE | 1030:0000, 10b0:008a, 10b0:0287, 10b0:0461, 11e0:0000 |  |
| `title_mplogo_show` | PARTIAL |  |  |
| `title_dialog_is_intro` | LIVE | 1098:124f, 11e0:0000 |  |
| `title_seq_is_earth` | LIVE | 10b0:008a |  |
| `title_intro_should_continue` | LIVE | 10b0:0461 |  |

### `src/video.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `video_init` | LIVE | 1008:059a, 1080:0119, 1080:06ff, 1090:0376, 1090:0488, 10b0:0461, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0956, 10d0:0cfa, 1200:00a8, 1420:0af4 |  |
| `video_blit_indexed` | LIVE | 10d0:0854 |  |
| `video_present` | LIVE | 10d0:0854 |  |
| `video_shutdown` | NOOP |  |  |
| `palette_animate_range` | LIVE | 1090:0119, 1090:0376, 10b0:0461, 1200:00a8 |  |
| `video_check_driver_caps` | LIVE | 1008:059a, 1420:0af4 |  |
| `palette_init` | LIVE | 1090:0488 |  |
| `gfx_brush_is_solid` | LIVE | 1098:00b5 |  |
| `gfx_color_table_kind` | LIVE | 1098:01b0 |  |
| `gfx_calc_color_dist_sq` | LIVE | 10e8:2cc1, 10e8:2d46 |  |
| `video_driver_is_mono` | LIVE | 1008:059a |  |
| `video_driver_needs_convert` | LIVE | 10b8:0000 |  |
| `mainport_palette_handle` | LIVE | 1098:1120, 1098:114a |  |
| `gfx_stored_dc_uses_stored` | LIVE | 1030:0000, 1098:114a, 1098:2687, 10b0:008a, 10b0:0461 |  |
| `mem_alloc_with_hook` | LIVE | 1000:2826, 1000:3496, 1000:41e4, 1000:46f1 |  |
| `gfx_scroll_copy_w` | LIVE | 10d0:03fa |  |
| `gfx_scroll_copy_h` | LIVE |  |  |
| `palette_snapshot_size` | LIVE | 1090:0119, 1098:25b9 |  |
| `palette_animate_opcode_ok` | LIVE | 1090:0119, 1090:0376, 1098:201d, 1200:00a8 |  |
| `palette_snapshot_headless_skips` | LIVE | 1098:25b9 |  |
| `palette_animate_locked_range` | PARTIAL | 1090:0119, 1090:0376, 1098:25b9 | has TODO |
| `palette_snapshot_and_animate` | PARTIAL | 1090:0119, 1098:25b9 | has TODO |
| `palette_animate_for_opcode` | LIVE | 1090:0119, 1090:0376 |  |
| `gfx_port_create` | LIVE | 10b0:008a, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0cfa, 1208:0af1, 1210:025b, 1218:0386 |  |
| `gfx_port_destroy` | LIVE | 10d0:0000, 10d0:0172 |  |
| `gfx_port_alloc_bitmap` | LIVE | 10d0:0000, 10d0:08f1, 10d0:0cfa |  |
| `gfx_port_set_active` | LIVE | 10b0:008a, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0956, 10d0:0cfa, 1208:0af1, 1210:025b, 1218:0386, 12d8:44ff |  |
| `gfx_port_present_rect` | LIVE | 1080:0119, 1080:06ff, 10d0:0854, 10d0:08f1, 10d0:0cfa |  |
| `gfx_port_blit` | LIVE | 10b0:008a, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0cfa, 1208:0af1, 1210:025b, 1218:0386 |  |
| `mainport_get_or_create` | PARTIAL | 1090:0376, 1098:114a, 10b0:008a, 10d0:0000, 10d0:0956, 1200:00a8, 1208:0af1, 1210:025b, 1218:0386 | has TODO |
| `gfx_port_fill_or_copy` | STUB | 1098:00b5, 10b0:0461, 10b8:079d, 10d0:0854, 10d0:08f1 | has TODO |
| `gfx_port_load_rsc_frame` | STUB | 1090:0376, 1090:0488, 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0cfa | has TODO |
| `util_draw_progress_bar` | STUB | 1000:29ea, 10d0:08f1 | has TODO |
| `gfx_port_change_color` | STUB | 10d0:0000, 10d0:0854, 10d0:08f1, 10d0:0956, 10d0:09ee, 10d0:0cfa | has TODO |
| `gfx_port_get_dc` | LIVE | 1098:114a, 10d0:0000 |  |
| `gfx_port_release_dc` | LIVE | 1098:114a, 10d0:0000 |  |
| `gfx_port_create_compatible` | LIVE | 10d0:0000, 10d0:0cfa |  |
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
| `art_load_frame` | PARTIAL | 10b8:079d, 10b8:0aa8, 10b8:0e00, 10b8:0e36, 10b8:11bc, 10b8:158c, 10d0:0000, 10f8:040a, 11e0:0000, 1420:2372 | has TODO |
| `art_frame_id_ok` | LIVE | 10b8:079d, 10b8:0aa8, 10b8:11bc, 11e0:0000 |  |
| `gfx_startup_port_size_ok` | LIVE | 1098:124f, 10b8:008a, 10d0:0000, 11e0:0000 |  |
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
| `gfx_store_dc` | PARTIAL | 1030:0000, 1098:1104, 1098:124f, 10b0:0461, 10d0:0854, 10d0:09a2, 1100:09a9, 1208:0af1, 1210:025b, 1220:0156 | has TODO |
| `gfx_dc_push` | PARTIAL | 1030:0000, 1098:1104, 1098:124f, 10b0:0461, 1220:0156 | has TODO |
| `gfx_dc_pop` | PARTIAL | 1098:1104, 10d0:0854, 10d0:09a2 | has TODO |
| `gfx_store_tick_needs_save` | LIVE | 10b0:0461 |  |
| `gfx_store_modal_needs_save` | LIVE | 1030:0000, 1098:124f, 1208:0af1, 1210:025b, 1220:0156 |  |
| `gfx_store_order_is_save_first` | LIVE | 1098:00b5, 10d0:0854 |  |
| `palette_dispose` | NOOP | 1090:0488, 1090:0596 |  |
| `palette_handle_is_live` | LIVE | 1090:0596 |  |
| `palette_animate_clamp` | LIVE | 1090:015e, 1090:02d2, 1090:0376, 10b0:0461, 1200:00a8 |  |
| `palette_flag_for_mono` | LIVE | 1090:0488, 1090:0670, 1090:06fd |  |
| `art_frame_needs_convert` | LIVE | 10b8:079d, 10b8:0858, 10b8:0863, 10b8:086d, 10b8:0c1c, 10c8:0000 |  |
| `art_frame_palette_copy_count` | LIVE | 10b8:0915 |  |
| `art_frame_palette_fill_from` | LIVE | 10b8:0929 |  |
| `art_frame_palette_fill_end` | LIVE | 10b8:0965 |  |
| `art_frame_saves_converted` | LIVE | 10b8:08a1, 10b8:0c1c, 10f8:05da |  |
| `art_port_alloc_size` | LIVE | 10b8:0e36, 10b8:0ed2, 10b8:0edb, 10b8:0f18 |  |
| `art_port_path_kind` | LIVE | 10b8:0e6a, 10b8:0e78, 10b8:0ec7, 10b8:10e1, 10d0:0cfa |  |
| `art_port_frees_bits` | LIVE | 10b8:116d |  |
| `art_load_frame_data` | PARTIAL | 1098:1104, 10b8:079d, 10b8:0aa8, 10b8:0c1c, 10b8:11bc, 10b8:158c, 10d0:0000, 10f8:040a | has TODO |
| `art_port_data_to_bitmap` | PARTIAL | 10b8:0e36, 10d0:0cfa | has TODO |
| `palette_find_color_index` | LIVE | 1070:127A, 1098:01B0, 10b0:168a |  |
| `gfxport_get_indexed_pixel` | STUB | 10b0:1749, 1608:0214 | has TODO |
| `gfxport_set_indexed_pixel` | STUB | 10b0:17a0, 1608:01C8 | has TODO |
| `gfx_build_dib_handle` | LIVE | 10a0:0000, 10d0:0cfa |  |
| `gfx_dib_handle_to_bitmap` | LIVE | 10a0:037c |  |
| `palette_noop_poll` | LIVE | 1098:1184, 1098:11c8 |  |
| `palette_noop_set_entry` | LIVE | 1098:11f0 |  |
| `pal_free_quant_tables` | STUB | 10c0:0000 | has TODO |
| `pal_init_quant_tables` | PARTIAL | 10c0:0044, 10c8:0012, 12f0:0902 | has TODO |
| `pal_expand_palette` | LIVE | 10c0:00e0, 10c8:0075, 12f8:FA22 |  |
| `pal_quant_init` | STUB | 10c0:013d, 10c8:002f | has TODO |
| `pal_kd_build` | STUB | 10c0:023e, 1420:2420 | has TODO |
| `pal_find_nearest` | LIVE | 10c0:0413, 10c8:063c |  |
| `pal_kd_range_search` | NOOP | 10c0:07c3 |  |
| `pal_cmp_channel` | LIVE |  |  |
| `pal_cmp_r` | LIVE | 10c0:0a71, 10c0:0ad9, 10c0:0b45, 1420:2420 |  |
| `pal_cmp_g` | LIVE | 10c0:0ad9 |  |
| `pal_cmp_b` | LIVE | 10c0:0b45 |  |
| `gfx_return_one` | LIVE | 1098:200a |  |
| `palette_noop_after_copy` | LIVE | 1098:20e6 |  |
| `gfx_store_word_triple` | LIVE | 1098:218d |  |
| `gfx_draw_dialog_frame` | STUB | 1020:0000, 1080:06ff, 1098:1104, 1098:21b8, 1098:2260, 10d0:0000, 10d0:0172, 10d0:0956, 10d0:09a2 | has TODO |
| `gfx_draw_border_tiles` | STUB | 1000:137f, 1098:2260, 10d0:0854, 11a8:0cb1, 1608:0100, 1608:0108 | has TODO |
| `gfx_border_tile_count` | LIVE |  |  |
| `gfx_border_tile_pos` | LIVE |  |  |
| `gfx_noop_border_gap_a` | LIVE | 1098:258f |  |
| `gfx_noop_border_gap_b` | LIVE | 1098:259d |  |
| `palette_noop_before_snapshot` | LIVE | 1098:25ab |  |
| `gfx_noop_after_stored_dc` | LIVE | 1098:26b3 |  |
| `pal_convert_frame` | STUB | 10b8:079d, 10c8:0000, 12f8:0056 | has TODO |
| `pal_dither_convert` | STUB | 10b8:06a0, 10c8:01c2 | has TODO |
| `pal_shift_queue` | LIVE | 1000:299e, 10c8:0947 |  |
| `pal_alloc_scale_tables` | STUB | 10c8:09a1, 12f8:0068 | has TODO |
| `pal_free_scale_tables` | STUB | 10c8:0aa1 | has TODO |
| `pal_cmp_int` | LIVE | 10c0:0bb1 |  |
| `pal_cmp_byte` | LIVE | 10c0:0be9 |  |
| `gfx_return_one_alt` | LIVE | 1098:200a, 1098:26c1 |  |
| `gfx_return_true_byte` | LIVE | 1098:2715 |  |
| `gfx_noop_gap_2727` | LIVE | 1098:2727 |  |
| `gfxport_dc_slot` | LIVE | 10d0:0262, 10d0:028c, 10d0:0956, 1100:09a9 |  |
| `gfxport_fill_color` | STUB | 10d0:02ba, 10d0:0cfa, 1420:28dc | has TODO |
| `gfxport_fill_clip_w` | LIVE |  |  |
| `gfxport_fill_clip_h` | LIVE |  |  |
| `gfxport_copy_port_to_port` | STUB | 10d0:06c0, 10d0:08f1, 1420:28fd | has TODO |
| `gfxport_fill_tiled_icon` | STUB | 10d0:0bee, 1218:0891 | has TODO |
| `gfxport_tile_cols` | LIVE |  |  |
| `gfxport_tile_rows` | LIVE |  |  |
| `art_phase_yield_stub` | LIVE | 1098:0157, 11e8:0296, 11e8:08c8 |  |
| `art_reserved_stub` | LIVE | 11e8:08c8 |  |
| `gfx_draw_icon_forward` | STUB | 11e8:0674, 11e8:08d6 | has TODO |
| `gfx_return_zero` | LIVE | 11e8:0900, 11e8:0912 |  |
| `gfx_extract_icon_slice` | STUB | 11e8:097c | has TODO |
| `gfx_slice_left` | LIVE |  |  |
| `gfx_slice_top` | LIVE |  |  |
| `gfx_slice_right` | LIVE |  |  |
| `gfx_slice_bottom` | LIVE |  |  |
| `gfxport_set_shared_ptr` | LIVE | 1200:0000 |  |
| `gfxport_shared_ptr_image` | LIVE |  |  |
| `gfxport_noop_slot_a` | NOOP | 1200:0016 |  |
| `gfxport_noop_slot_b` | NOOP | 1200:0024 |  |
| `gfxport_noop_slot_c` | NOOP | 1200:0032 |  |
| `gfxport_noop_slot_d` | NOOP | 1200:0040 |  |
| `gfxport_noop_slot_e` | NOOP | 1200:004e |  |
| `gfxport_noop_slot_f` | NOOP | 1200:005c |  |
| `gfxport_noop_slot_g` | NOOP | 1200:006a |  |
| `gfx_fill_stack_rect` | STUB | 1000:14f8, 1100:0876, 1608:0110 | has TODO |
| `gfx_pre_store_stub` | LIVE | 10e8:012e, 1100:0996, 1100:0a0d, 1108:02cf, 1420:B3C2 |  |
| `gfx_post_set_dc_stub` | LIVE | 1100:0a0d |  |
| `gfx_validate_stored_rect` | STUB | 1030:5732, 1100:09a9, 1100:09e7, 1608:014c | has TODO |
| `gfx_stroke_rect` | STUB | 1100:0ae5, 1420:9960 | has TODO |
| `palette_push_palette` | STUB | 1068:06af, 1090:015e, 1098:28e4 | has TODO |
| `palette_pop_palette` | STUB | 1068:1bc6, 1090:02d2, 1090:0376 | has TODO |
| `palette_zero_god_palette` | STUB | 1090:05db | has TODO |
| `palette_clear_range_flags` | STUB | 1090:0670, 1090:06fd | has TODO |
| `palette_set_range_flags` | STUB | 1090:06fd | has TODO |

### `src/win16.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `win16_mem_init` | LIVE | 1100:030f, 11e8:0000, 11e8:0337 |  |
| `win16_mem_shutdown` | LIVE |  |  |
| `win16_arena_alloc` | LIVE |  |  |
| `win16_arena_used` | LIVE |  |  |
| `win16_arena_size` | LIVE |  |  |
| `win16_global_alloc` | LIVE | 1100:030f |  |
| `win16_global_free` | LIVE |  |  |
| `win16_get_free_space` | LIVE | 11e8:0000 |  |
| `win16_get_free_resources` | LIVE | 11e8:0000 |  |
| `win16_check_free_memory` | LIVE | 11e8:0000 |  |
| `win16_check_runtime_memory` | LIVE | 11e8:0075 |  |
| `win16_dos_get_drive` | LIVE | 1000:1252 |  |
| `win16_dos_get_cwd` | LIVE | 1000:139c |  |
| `win16_dos_chdir` | LIVE | 1000:116a |  |
| `win16_fd_alloc` | LIVE |  |  |
| `win16_fd_get` | LIVE |  |  |
| `win16_lopen` | LIVE | 1000:2ad9 |  |
| `win16_lread` | LIVE | 1000:1750 |  |
| `win16_lwrite` | LIVE | 1000:3fc4 |  |
| `win16_llseek` | LIVE | 1000:161c |  |
| `win16_lclose` | LIVE | 1000:1fa6 |  |
| `win16_register_classes` | LIVE | 1008:00cd, 1058:0b4d, 1098:06f8, 1208:0e7b |  |
| `win16_get_system_metrics` | LIVE |  |  |
| `win16_message_box` | LIVE | 1420:43b8 |  |
| `win16_set_timer` | LIVE | 1008:00cd, 1208:0e7b |  |
| `win16_kill_timer` | LIVE |  |  |
| `win16_timer_interval_ms` | LIVE |  |  |
| `win16_create_palette` | LIVE | 1090:0488 |  |
| `win16_animate_palette` | LIVE | 1090:0376 |  |
| `win16_dispose_palette` | LIVE |  |  |
| `win16_delete_object` | LIVE |  |  |
| `win16_asset_cb` | LIVE |  |  |
| `win16_find_asset` | LIVE |  |  |
| `win16_probe_resources` | LIVE |  |  |
| `win16_probe_fonts` | LIVE |  |  |

### `src/worldgen.c`

| Function | State | Orig addr | Note |
|---|---|---|---|
| `worldgen_dir_dx_at` | LIVE |  |  |
| `worldgen_dir_dy_at` | LIVE |  |  |
| `worldgen_generate_random` | LIVE | 1028:0000 |  |
| `worldgen_load_preset` | LIVE | 1028:0000, 1028:0a93, 1028:1d15, 10b8:079d |  |
| `worldgen_load_earth` | PARTIAL | 1028:0a93, 1028:1d15, 10b8:079d, 10b8:0e00, 10e0:0000, 1108:0243, 11e0:0221, 1420:2e28, 1420:2e34 | has TODO |
| `worldgen_dialog_id_to_cell` | LIVE | 10e0:0000, 1108:0243, 1420:2e28, 1420:2e34 |  |
| `worldgen_custom_apply` | LIVE | 10e0:0000, 1100:03ff, 1108:0243, 1420:2e28, 1420:2e34 |  |
| `worldgen_prepare_earth` | PARTIAL | 10b8:079d, 10e8:0000, 1108:01bc, 1108:0243, 1150:00c9, 11e0:0000, 12d8:44e2 | has TODO |
| `worldgen_prepare_mode` | LIVE | 1028:0000, 1108:01bc, 12d8:44e2 |  |
| `worldgen_is_preset` | LIVE | 1028:0000 |  |
| `worldgen_earth_frame` | LIVE | 1028:0000, 10b8:0aa8, 10b8:11bc |  |
| `worldgen_earth_preset_bytes` | LIVE | 10b8:079d |  |
| `worldgen_copy_preset` | LIVE | 1028:0000, 10b8:0000, 10b8:079d, 1420:20a9 |  |
| `worldgen_customize` | PARTIAL | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6, 1058:0b4d, 1098:124f, 10e8:0000, 1220:0156, 1220:16fd | has TODO |
| `worldgen_customize_dialog` | PARTIAL | 1058:06ab, 1058:081a, 1058:0921, 1058:09f6, 1058:0b4d, 1098:124f, 1098:1612 | has TODO |
| `worldgen_seed_one_island` | LIVE | 1028:0945 |  |
| `worldgen_paint_blob` | LIVE | 1028:09ef |  |
| `worldgen_label_continents` | LIVE | 1028:0a93, 1028:12c6, 1028:1ca7, 1028:1d15 |  |
| `worldgen_gen_rivers` | LIVE | 1028:066a, 1028:06c1, 1028:1dfb, 11c8:003d |  |
| `worldgen_spread_pollution` | LIVE | 1028:1afd, 1110:1b36, 11c8:003d, 11e8:0e19 |  |
| `worldgen_score_city_sites` | LIVE | 1028:0f0c, 1028:0fb4, 1420:17d6 |  |
| `worldgen_build_continent_links` | LIVE | 1000:29ea, 1028:12c6, 1098:0157, 10b0:0000, 1128:16d0 |  |
| `worldgen_build_ocean_links` | LIVE | 1000:29ea, 1028:0f01, 1028:162c, 10f8:01e9 |  |
| `worldgen_copy_rect` | LIVE | 1028:06c1, 1028:1d15, 1028:1dbd, 1028:1dfb |  |
| `path_check_continent_link` | LIVE | 1028:12c6, 1128:10de, 1128:16d0 |  |
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
| `worldgen_probe_custom_font` | PARTIAL | 1060:0000, 1060:01c1 | has TODO |
| `worldgen_scatter_count` | LIVE | 1028:0000 |  |
| `worldgen_scatter_is_resample` | LIVE |  |  |
| `worldgen_scatter_tile_action` | LIVE | 1028:05e7, 11e8:0dcf |  |
| `worldgen_seed_origin_x` | LIVE | 1028:0945 |  |
| `worldgen_seed_origin_y` | LIVE |  |  |
| `worldgen_video_caps_query_count` | LIVE | 1060:01c1 |  |
| `worldgen_video_caps_query_at` | LIVE |  |  |
| `worldgen_video_caps_needs_raster_ext` | LIVE |  |  |
| `worldgen_show_video_caps_dialog` | PARTIAL | 1038:0c1d, 1060:0000, 1060:005b, 1060:00d1, 1060:01c1, 1098:124f, 1098:1612 | has TODO |
| `map_is_link_grid_in_bounds` | LIVE | 1028:12c6, 1028:162c, 1028:1acf, 1248:0004 |  |
| `worldgen_return_second` | LIVE | 1030:0000, 1038:0000, 1108:01a9, 1108:01bc |  |
