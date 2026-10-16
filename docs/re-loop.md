# RE-LOOP — pętla dekompilacji do 100% pokrycia (CivLin)

Powtarzalny workflow: scalanie drzew worktree → regeneracja STATUS →
planowanie następnej fali niezależnych sesji → repeat aż 100%.
Szczegółowy plan pierwszej fali jest w historii sesji; tu tylko reguły stałe.

## 0. Definicja celu (100%)

- `python3 tools/gen_status.py` → **0 wierszy STUB** i **0 wierszy PARTIAL z `has TODO`**
  poza allowlistą WONTFIX (patrz §5).
- Każda funkcja Ghidra (`civ.exe`, 1303 fun, `x86:LE:16:Protected Mode`) ma albo:
  port z adnotacją `Orig seg:off + FUN_seg_off` w komentarzu `src/*.h` (i w miarę
  możliwości w `.c`), albo wpis WONTFIX/NOOP z uzasadnieniem w `STATUS.md`.
- `make clean && make CC=clang-24` przechodzi z **zerem warningów**.
- `nm -g civlin` pokazuje nowe symbole po każdej fali.
- `STATUS.md` regenerowany wyłącznie przez `python3 tools/gen_status.py > STATUS.md`;
  ręczna rzeźba tylko w `OVERRIDES` w `tools/gen_status.py` i w sekcji
  „Ścieżka krytyczna" w nagłówku generatora.
- Miara uzupełniająca (do dobudowania jako `tools/coverage_ghidra.py`):
  `% funkcji Ghidra nie-`FUN_*` (nazwanych `Module_VerbNoun`) oraz
  `% adresów Ghidra pokrytych przez `Orig` w `src/*.h` — `gen_status` liczy
  tylko `src/*.c`, więc sam nie dowodzi pokrycia 1303 funkcji.

## 1. Faza M — zamrożenie bazy (master)

1. Snapshot metryki: `python3 tools/gen_status.py > /tmp/status_base.md`
   + zapisz liczby LIVE/STUB/PARTIAL.
2. Build-check mastera: `make clean && make CC=clang-24` (zero warnings).
3. Commit brudnego mastera jako `wip/re-base-<YYYYMMDD>` (razem z nowymi
   plikami typu `src/win16.*`). Bez czystej bazy patche z worktrees fałszywie
   się konfliktują.
4. W każdym brudnym worktree: build-check, potem `git add` **tylko `src/*`**
   + commit na jego branchu `re/*`. Puste worktrees (diff 0): nic nie commitować,
   oznaczyć klaster do retry w następnej fali.

## 2. Faza S — scalanie sekwencyjne (JEDEN merger, nigdy równolegle)

1. Kolejność: **od najmniejszego diffa do największego**; pliki `dialog.*`
   (edytowane zwykle przez kilka klastrów naraz) schodzą pojedynczo, hunk po hunku.
2. Po każdym mergu: `make clean && make CC=clang-24`. Przy fail — stop, fix,
   dopiero następny merge.
3. Konflikty w `src/*.h`: zachować **wszystkie** adresy `Orig` z obu stron
   (mapa RE żyje w nagłówkach). Nie gubić `TODO: Orig ...` z ciał STUBów.
4. Po ostatnim mergu: `python3 tools/gen_status.py > STATUS.md`,
   commit `merge(re): fala <nazwa> <data>`, push branchy `re/*` jako backup.
5. Worktrees scalone i puste: sprzątnąć wg §6 (stop sesji → remove worktree →
   prune → ewentualnie delete brancha). Żadnego worktree nie zostawia się
   „na potem" po zakończeniu fali.

Zakazane w fazie S: edycje `civ1win/` i `unpacked/` (read-only oryginały),
commity stanu Ghidra (`*.gpr`, `CivLin.rep/`, `*.lock*`), output buildu,
instalacje nowych tooli (dozwolone tylko clang-24 + SDL3 + Ghidra),
konwersje assetów w miejscu (loadery piszemy w `src/`).

## 3. Faza P — planowanie następnej fali

1. Delta: porównaj LIVE/STUB/PARTIAL przed/po + listę `has TODO` z nowego STATUS.
2. Frontier (priorytet): (a) pierwszy STUB na krytycznej ścieżce
   WinMain @ 1008:0000 w dół, (b) resztki P0→P4, (c) HoF 1080 / save / dziury
   audio / map-windows. Puste klastry z poprzedniej fali wracają na początek
   kolejki (zwykle: shared-player `1030:*`, newgame/worldgen `10e8/1028/11f8`).
3. Klastry muszą mieć **rozłączne zbiory adresów `seg:off`** i rozłączne główne
   pliki docelowe. Reguła: max **1 pisarz** na `dialog.*` w danej fali, reszta
   tylko czyta. Zapisy nazw w Ghidrze (`Module_VerbNoun`, np. `Anim_OpenSequence`,
   `Dlg_CreateTracked`, `Map_GenerateRandomOrLoad`) tylko we własnym klastrze.
4. Każdy task dostaje: listę adresów, pliki docelowe, zakaz dotykania cudzych
   adresów, triage z AGENTS.md (logika OS-free → port 1:1 blisko decompilera;
   segmenty/`__far`/DOS-int/`SELECTPALETTE`/uchwyty okien/bufory mapowane →
   stub `CIV_ERR_UNSUPPORTED`/`RSC_ERR_UNSUPPORTED` + `TODO: Orig ...`),
   gate `make clean && make CC=clang-24` zero warnings + `nm -g`.
5. Tryb: jedno worktree na klaster, branch `re/<fala>-<klaster>`.
   Nie startować więcej klastrów niż rozłącznych frontów (zwykle 5–7).

## 4. Faza L — pętla i stop

Powtarzaj M → S → P aż spełniona jest definicja z §0 albo zostaje wyłącznie
allowlista WONTFIX. Przerwanie fali (do diagnozy, nie do siłowego mergu):
czerwony build, nierozwiązywalny konflikt w `src/*.h` w jednym przebiegu,
albo 2 puste sesje z rzędu w tym samym klastrze (wtedy klaster dzielimy drobniej
zamiast retry 1:1).

## 5. Allowlista WONTFIX (nie dumpować, jawny no-op `CIV_OK` z adresem Orig)

Semantyka GLOBALALLOC-lock, GDI-install fontów, bajty checklisty GETDEVICECAPS,
chunki drivera w `civ.prf`, wnętrze MCI. Wszystko inne ma dążyć do LIVE.

## 6. Sprzątanie worktrees (po KAŻDEJ fali, żeby nie zaśmiecić repo i dysku)

1. Moment: dopiero gdy branch `re/*` jest scommitowany **i** wypchnięty jako backup
   (albo jawnie oznaczony jako pusty/porzucony) **i** jego treść jest już w masterze
   po fazie S. Nigdy przed mergem.
2. Kolejność na jedno worktree (z katalogu mastera):
   `Agent Manager → stop sesji` (to wyrejestrowuje ją z `.kilo/agent-manager.json`),
   potem `git worktree remove --force .kilo/worktrees/<nazwa>`,
   na końcu `git worktree prune`. Nie `rm -rf` na piechotę — ręczne kasowanie
   katalogu zostawia wpis w `git worktree list` i rozjeżdża stan Agenta.
   `.kilo/agent-manager.json` nigdy nie edytujemy ręcznie.
3. Branche: lokalny `re/*` kasujemy (`git branch -d`) tylko gdy jest w pełni
   scalony do mastera; zdalny backup zostawiamy do końca całej pętli RE-LOOP.
   Porzucone/puste klastry: branch zostaje do retry w fazie P, samo worktree
   i tak usuwamy (pusty checkout i tak nic nie trzyma).
4. Limit higieny: przed startem nowej fali `git worktree list` ma pokazywać
   wyłącznie master (plus ewentualnie celowo wstrzymane). Jeśli coś wisi —
   najpierw dokończ §6, potem odpalaj nowe sesje. Nowa fala = maks. tyle klastrów,
   ile rozłącznych frontów, żeby za falę nie sprzątać dwa razy.
5. Drobiazgi w worktree przed `remove`: `make clean`, brak plików
   `*.lock*`/`CivLin.rep`/`civlin`/`rsc_dump` w diffie (build output i stan Ghidra
   nigdy nie wchodzą do commita). Pool `~/.local/share/kilo/worktree-pool`
   (detached HEAD-y) zostawiamy Agent Managerowi — nie czyścimy go ręcznie
   w środku fali.
