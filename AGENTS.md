# CivLin — faithful Civ1 (Windows) clone for Linux

Goal: recompile-equivalent of original `CIV.EXE` on Linux from Ghidra
decompilation + original assets. Not a remake: behavior and data stay
faithful to the Windows release.

## Layout

- `civ1win/` — original Windows assets, READ-ONLY reference. Never edit,
  never copy modified files back here. Contains packed `CIV.EXE`
  (EDI LZSS2, 832 KB), `CIVDATA*.RSC`, `CIVFONTS.FON`, WAV/HLP/BMP.
- `src/` — reimplementation on SDL3. Modules mirror the Ghidra RE map
  (all Orig FUN_* names + addresses live in each header):
  `boot` (WinMain @ 1008:0000 order), `title` (intro/OPENING.wav),
  `mainmenu` (mode select @ 11e0:0221), `newgame` (wizard @ 10e8:0000),
  `game` (turn loop @ 1088:0000), `halloffame` (segment 1080),
  `worldgen` (map gen @ 1028:0000), `dialog` (MenuZ @ 1220:0156 + 1058
  controls), `audio` (sound IDs @ 1150:019e), `map` (map windows),
  `save` (.sav), `video`, `rsc_loader`, `gfx_indexed`, `civ_common`.
- `unpacked/` — unpacked `CIV.EXE` goes here (currently empty). Ghidra
  imports from here, never from packed `civ1win/CIV.EXE` directly.
- `CivLin.gpr` / `CivLin.rep/` — Ghidra project, LOCAL ONLY, never
  committed (see Git below). Open with Ghidra 12.1.2 at
  `~/ghidra_12.1.2_PUBLIC/ghidraRun`.
- `null.mgc` — custom `file(1)` magic helper, leave as is.
- `tools/` — helper scripts (e.g. `unpack_edi.py`). Tracked, keep portable.

## Toolchain (verified 2026-10-15)

- Language: C (stays close to Ghidra decompiler output). 
- Compiler: `clang-24` per project decision  
- Graphics/platform layer: SDL3 only (`pkg-config --modversion sdl3` →
  `3.5.0`, installed from source in `/usr/local`, flags via
  `pkg-config --cflags --libs sdl3`). No SDL2, no other windowing/audio API.

## Working rules

- Build with `make` (`CC=clang-24`, flags via `pkg-config sdl3`):
  `make` builds `./civlin` + `./rsc_dump`; `make run` / `make run-dump`.
  Boot steps are UNSUPPORTED stubs until each module is implemented.
- Decompilation flow: unpack EDI LZSS2 → `unpacked/` → import into
  `CivLin.gpr` → reimplement in `src/` on top of SDL3. Do not check in
  Ghidra lock files (`CivLin.lock`, `*.lock~`).

## RE workflow (Ghidra → src)

- Start from known points: already-named functions, string literals
  (`civfonts.fon`, `CivData*.rsc`, `DO NOT delete any *.rsc files`),
  Win16 imports (`_LOPEN`, `_LREAD`, `LOADRESOURCE`, `FINDRESOURCE`),
  or callers from the previous frontier. Follow calls AND xrefs/data-refs
  outward (callees + callers), batch-decompile each new `FUN_*`.
- Name in Ghidra as soon as behavior is clear (`Module_VerbNoun`,
  e.g. `Rsc_OpenContainer`, `Map_FillRect`); keep `Orig FUN_seg_off`
  + address in every `src/*.h` comment so the RE map survives.
- Triage per function (2026-10-16 update: odblokowanie STUB):
  - WSZYSTKO dekodujemy i przenosimy 1:1 do `src/`, staying close to
    decompiler output. Dotyczy to także stanu gry: globale DGROUP
    (`DAT_1420_*`, `DAT_12d8_*`, `DAT_13f8_0000` shared-static,
    stos dialogów, bufory map/save, dane palety, tablice).
    Stan trzymamy na jawnych kontekstach (`struct Game_State`,
    `Dialog_Ctx`, `Rsc_Mgr`, parametry `out/cap`) albo — gdy oryginał
    używał proces-global/static o semantyce nadpisywania przy następnym
    wywołaniu (np. `Res_GetStringCopy/Raw @ 11f8:0080/00c5`) — na
    module-static z tą samą semantyką + adnotacją Orig adresu.
    NIE zostawiamy STUB tylko dlatego, że oryginał użył globala,
    `__far` pointera albo segmentu. Segmenty/`__far`/DOS-owe layouty
    mapujemy na jawne offsety/rozmiary z komentarzem Orig.
  - Tylko mechanika OS zostaje STUB/PARTIAL: USER/GDI/KERNEL plumbing
    (`REGISTERCLASS`, `CREATEDIALOG` + pompa `PEEKMESSAGE/ISDIALOGMESSAGE`,
    `SELECTPALETTE`/DC/pędzle, selektory, DOS inty, handle okien,
    timery, `MESSAGEBOX`/`WINHELP`/MCI, bezpośrednie bufory video).
    Te zwracają `CIV_ERR_UNSUPPORTED` / `RSC_ERR_UNSUPPORTED`
    z `TODO: Orig ...` opisującym brakujący mostek SDL3/stdio
    (`win16.*`, `video.*`, `dialog.*` modal). Wrappery efektu, których
    efekt jest próżniowy na SDL, mogą być uczciwym no-op `CIV_OK`
    z komentarzem `WONTFIX-Win16`.
  - Zasada H (Fala H) bez zmian: `CIV_OK` tylko gdy efekt osiągnięty
    albo krok dowodliwie próżniowy na SDL, inaczej `UNSUPPORTED`
    (no fake-OK); nie zgadujemy dekoderów (1198 RLE/tagged-frame,
    godpal) — dopóki dump/dekompilacja niepotwierdzona, trzymamy
    `*out untouched` + TODO z porównaniem.
- Close the loop: implement → `make clean && make CC=clang-24` must pass
  with zero warnings → `nm -g civlin` shows the new symbols.

## Git (one repo at root, `git init` done 2026-10-15)

- Tracked: `src/`, `tools/`, `AGENTS.md`, `.gitignore`, future build/docs.
  Nothing else.
- Never commit: `civ1win/` + `unpacked/` (copyrighted originals/derivatives,
  ~5 MB, local only — game must load them from a local copy), Ghidra state
  (`*.gpr`, `CivLin.rep/`, `*.lock*`), build output, `.kilo/`.
- So: NIE trzymamy całości w repo — tylko kod. Assety każdy trzyma
  lokalnie obok checkoutu.
- Ask the user before installing/adding any new tool beyond
  clang + SDL3 + Ghidra (user request).
- Original asset formats (RSC/FON/HLP) are reverse-engineer targets —
  write loaders in `src/`, never convert assets in place.
