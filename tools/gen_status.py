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
SIG = re.compile(r"^(?:static\s+)?[ \t\w\*]*?\b(\w+)\s*\(", re.M)
KEYWORDS = ("if", "for", "while", "switch", "return", "sizeof")


def sig_end(text, paren_idx, bound=2000):
    """From index of '(' find matching ')' then '{' (def) or ';' (decl).

    Params may span lines; only whitespace may sit between ')' and the
    terminator. Returns (kind, idx) or None. Single-line prefix (SIG)
    keeps comment words from matching across lines.
    """
    depth = 0
    j = paren_idx
    end = min(len(text), paren_idx + bound)
    while j < end:
        ch = text[j]
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                k = j + 1
                while k < len(text) and text[k] in " \t\r\n":
                    k += 1
                if k < len(text) and text[k] == "{":
                    return ("def", k)
                if k < len(text) and text[k] == ";":
                    return ("decl", k)
                return None
        elif ch == "{" and depth > 0:
            return None
        j += 1
    return None


def iter_sigs(text, kind):
    """Yield (name, match_start, term_idx) for definitions/decls."""
    for m in SIG.finditer(text):
        name = m.group(1)
        if name in KEYWORDS:
            continue
        r = sig_end(text, m.end() - 1)
        if r is None or r[0] != kind:
            continue
        yield name, m.start(), r[1]


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
    if len(meat) == 1 and re.fullmatch(r"return\s+NULL\s*;", meat[0]):
        return "STUB"  # pointer-stub with no UNSUPPORTED sentinel yet
    if (any(re.fullmatch(r"return\s+NULL\s*;", ln) for ln in meat)
            and all(re.fullmatch(r"return\s+NULL\s*;", ln)
                    or ln.startswith("if") or ln in ("else", "else {")
                    for ln in meat)):
        return "STUB"  # guard + NULL returns only: unimplemented stub
    return "LIVE"


# Hand overrides where the literal scan misleads (dispatchers etc.).
OVERRIDES = {
    "main": "LIVE",  # SDL boot + menu/game tails + exit works (boot real, rest best-effort)
    "title_play_intro_sdl": "LIVE",  # SDL intro player (parallel to RE-map stub)
    "audio_play_id": "LIVE",
    "sound_play_file": "LIVE",
    "newgame_wizard_run": "PARTIAL",  # real dispatcher, all steps still STUB
    "anim_advance_frames": "PARTIAL",  # OS-free opcode 1..0xe/tick/ret/slot/caption/dissolve probes LIVE in dialog.c:416, timed GfxPort/frame tail STUB
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
  funkcją, w jej ciele oraz w nagłówku `src/*.h` (mapa RE żyje w nagłówkach)
  — to wskazówki RE, nie approved mapping.
- „has TODO" = w ciele jest znacznik TODO z adresem Orig i opisem braków.

## Ścieżka krytyczna WinMain @ 1008:0000 (stan na 2026-10-16)

`main.c`: SDL_Init + okno/renderer + arena 16 MB (`win16_mem_init`) ->
prawdziwy `boot_run` (**CIV_OK**, zweryfikowane uruchomieniem `boot: ok`) ->
`mainmenu_run` (STUB, pierwszy strict-stop) -> `game_init` (LIVE) ->
`game_new` (STUB) -> jeden `game_tick` (STUB) -> shutdown, exit 0.
Reszta za bootem leci best-effort (log, nigdy fatal).
Warstwa Win16/DOS żyje w `src/win16.{h,c}` (SDL3/stdio, arena zamiast
GLOBALALLOC/GETFREESPACE; WONTFIX-y jako no-op `CIV_OK` z adresem Orig).

| Krok WinMain | Funkcja | Stan | Komentarz |
|---|---|---|---|
| Sys_CheckFreeMemory | `sys_check_free_memory` | LIVE | delegat do `win16_check_free_memory` (SDL_GetSystemRAM vs 2,5 MB + 0x23) |
| paths (drive/cwd) | `sys_init_paths` | LIVE | |
| App_RegisterWindowClasses | `app_register_window_classes` | LIVE | WONTFIX: brak klas okien na SDL — no-op `CIV_OK` via `win16_register_classes` |
| civfonts.fon | `font_enum_families_init` | LIVE | sonda plików via `win16_probe_fonts`; parse na żądanie (`font_fon/fnt_*` LIVE), install GDI WONTFIX |
| ResMgr_LoadRsc x5 | `boot_load_resources` | LIVE | sonda 5 plików via `win16_probe_resources` (case-insensitive); `rsc_load_rsc` LIVE, właściciel mgr TBD |
| Video_CheckDriverCaps | `boot_check_video` | LIVE | WONTFIX-checklist: no-op w tym kroku; żywy check renderera gdzie renderer istnieje |
| Art_MaybeConvertArtwork | `art_maybe_convert_artwork` | LIVE | na Linuksie zbędne — no-op CIV_OK |
| Config_LoadPrefs | `boot_load_prefs` | LIVE | domyślne (sounds on); I/O `civ.prf` zostaje w `config_load_prefs` (STUB) |
| Palette_Init | `palette_init` | LIVE | no-op (paleta per-tekstura) |
| Game_InitGlobalTables | `boot_init_tables` | LIVE | rng + arena 16 MB; WONTFIX: Win16-lock selektora (bufor mapy w `map.c` `.bss`) |
| Main_RunStartupSequence (intro) | `main_run_startup_sequence` | STUB | **krytyczna ścieżka**: Anim_OpenSequence @ 10b0:008a + pętla AdvanceFrames @ 10b0:0461 nad dialogiem 0x94 (P0); menu dopiero po intrze |
| Init_UnitFontTables | `boot_init_unit_font_tables` | STUB | potrzebne dopiero wizardowi (P1) |
| Sound_PlayId(STOP) | `audio_play_id` | LIVE | |
| MainPort/GfxPort | `mainport_get_or_create`, `gfx_port_create` | STUB | mostek na SDL_Texture — średnia praca (P3) |
| WdwMap/SmMap/Status_Open | `map_open_*` | STUB | `map_init` LIVE (no-op); widoki SDL TBD |
| Menu_InitMainBar | `menu_init_mainbar` | LIVE | WONTFIX: brak USER-menu na SDL — no-op (stringi w `menu_strings` LIVE) |
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
"""

def header_addrs(hpath):
    """Map function name -> Orig seg:off addrs from its header comment.

    Workflow keeps `Orig FUN_seg_off` + address in every src/*.h comment
    so the RE map survives; merge those into the STATUS row (the .c body
    alone may not repeat them).
    """
    try:
        hsrc = open(hpath).read()
    except OSError:
        return {}
    out = {}
    for name, mstart, _ in iter_sigs(hsrc, "decl"):
        pend = hsrc.rfind("*/", 0, mstart)
        if pend == -1:
            continue
        cstart = hsrc.rfind("/*", 0, pend)
        if cstart == -1:
            continue
        pre = hsrc[cstart:mstart]
        if re.search(r"\w+\s*\(.*\)\s*;", strip_comments(pre)):
            continue  # code between comment and decl: not its header
        addrs = sorted(set(ADDR.findall(pre)))
        if addrs:
            out.setdefault(name, set()).update(addrs)
    return out


print(HEADER)
for path in sorted(glob.glob("src/*.c")):
    if path.endswith("rsc_dump.c"):
        continue
    src = open(path).read()
    hmap = header_addrs(path[:-2] + ".h")
    print(f"\n### `{path}`\n")
    print("| Function | State | Orig addr | Note |")
    print("|---|---|---|---|")
    for name, mstart, brace_idx in iter_sigs(src, "def"):
        body = body_of(src, brace_idx)
        # immediately preceding comment block only (after last */)
        pend = src.rfind("*/", 0, mstart)
        cstart = src.rfind("/*", pend if pend != -1 else 0, mstart)
        pre = src[cstart:mstart] if cstart != -1 else ""
        if re.search(r"\w+\s*\(.*\)\s*;", strip_comments(pre)):
            pre = ""  # code between comment and function: not its header
        addrs = sorted(set(ADDR.findall(pre + body[:400])) | hmap.get(name, set()))
        state = OVERRIDES.get(name, classify(name, body))
        todo = "TODO" in body
        note = "has TODO" if (todo and state in ("STUB", "PARTIAL")) else ""
        print(f"| `{name}` | {state} | {', '.join(addrs)} | {note} |")
