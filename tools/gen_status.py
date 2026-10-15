#!/usr/bin/env python3
"""Generate the per-function STATUS.md table from src/*.c.

Heuristic only (see LEGEND in STATUS.md); hand-fix rows where the
dispatcher status differs from the literal scan:
  - STUB     body ends in CIV_ERR_/RSC_ERR_UNSUPPORTED, no real work
  - PARTIAL  real work done, tail still UNSUPPORTED (or mixed returns)
  - LIVE     real work, success path returns CIV_OK/RSC_OK (or the value
             contract is fulfilled: pure OS-free ports, SDL impl, validators)
  - NOOP     intentionally empty (void shutdowns, cursor_set, placeholders)

Usage:  python3 tools/gen_status.py   # prints the full-table markdown
"""
import glob
import re

ADDR = re.compile(r"\b[0-9A-Fa-f]{4}:[0-9A-Fa-f]{4}\b")
FUNC = re.compile(r"^(?:static\s+)?(?:[\w\*]+\s+)+(\w+)\s*\([^;]*?\)\s*\{\s*$", re.M)


def body_of(src, start):
    depth = 0
    j = start
    while j < len(src):
        if src[j] == "{":
            depth += 1
        elif src[j] == "}":
            depth -= 1
            if depth == 0:
                return src[start : j + 1]
        j += 1
    return src[start:]


def strip_comments(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    s = re.sub(r"//.*", "", s)
    return s


def classify(name, body):
    code = strip_comments(body)
    stmts = [ln.strip() for ln in code.splitlines() if ln.strip() and ln.strip() not in ("{", "}")]
    # body starts at '{' (no signature line inside); drop pure (void)x; casts
    meat = [ln for ln in stmts if not re.fullmatch(r"\(void\)\w+\s*;", ln)]
    has_unsup = "UNSUPPORTED" in code
    has_ok = re.search(r"return\s+(CIV_OK|RSC_OK)\b", code) is not None
    if has_unsup and not has_ok:
        # pure arg-guard + UNSUPPORTED, or real work with UNSUPPORTED tail?
        if len(meat) > 6:
            return "PARTIAL"
        return "STUB"
    if has_unsup and has_ok:
        return "PARTIAL"
    if not meat:
        return "NOOP"
    if len(meat) == 1 and re.fullmatch(r"return\s+\w+\s*\(.*\)\s*;", meat[0]):
        return "LIVE"  # thin delegate to the real owner
    return "LIVE"


# Hand overrides where the literal scan misleads (dispatchers etc.).
OVERRIDES = {
    "boot_run": "PARTIAL",  # dispatches 10 steps; blocked by first STUB step
    "main": "LIVE",  # SDL boot + intro + exit works (best-effort boot)
    "title_play_intro_sdl": "LIVE",  # SDL intro player (parallel to RE-map stub)
    "audio_play_id": "LIVE",
    "sound_play_file": "LIVE",
    "newgame_wizard_run": "PARTIAL",  # real dispatcher, all steps still STUB
    "anim_advance_frames": "STUB",  # constant 1 = "done" placeholder
}

HEADER = """# CivLin — stan implementacji (per-function)

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
  funkcją i w jej ciele — to wskazówki RE, nie approved mapping.
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
"""

print(HEADER)
for path in sorted(glob.glob("src/*.c")):
    if path.endswith("rsc_dump.c"):
        continue
    src = open(path).read()
    print(f"\n### `{path}`\n")
    print("| Function | State | Orig addr | Note |")
    print("|---|---|---|---|")
    for m in FUNC.finditer(src):
        name = m.group(1)
        if name in ("if", "for", "while", "switch", "return"):
            continue
        start = m.end() - 1
        body = body_of(src, start)
        # immediately preceding comment block only (after last */)
        pend = src.rfind("*/", 0, m.start())
        cstart = src.rfind("/*", pend if pend != -1 else 0, m.start())
        pre = src[cstart : m.start()] if cstart != -1 else ""
        if re.search(r"\w+\s*\(.*\)\s*;", strip_comments(pre)):
            pre = ""  # code between comment and function: not its header
        addrs = sorted(set(ADDR.findall(pre + body[:400])))
        state = OVERRIDES.get(name, classify(name, body))
        todo = "TODO" in body
        note = "has TODO" if (todo and state in ("STUB", "PARTIAL")) else ""
        print(f"| `{name}` | {state} | {', '.join(addrs)} | {note} |")
