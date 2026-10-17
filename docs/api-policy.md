# API-POLICY — co robić z funkcją dotykającą API (CivLin)

Reguły stałe (2026-10-17: port 1:1 wszystkiego, LIVE tylko z fasadą).
Wszystko portujemy 1:1 do `src/`, ŁĄCZNIE z funkcjami wołającymi USER/GDI/
KERNEL/Win16 — nie pomijamy funkcji dlatego, że dotyka Win16. Trzy wyjścia,
w tej kolejności próbowania:
**shim/fasada (LIVE) → wierny port + STUB/PARTIAL z TODO → mały refactor
wewnątrz funkcji.**
Wybór dokumentuje się w komentarzu przy funkcji (znaczniki §5).
Nadrzędna zasada (Fala H): `CIV_OK` tylko gdy efekt osiągnięty albo krok
dowodliwie próżniowy na SDL; inaczej uczciwy `CIV_ERR_UNSUPPORTED`.
Żadnego fake-OK. LIVE = tylko implementacja bazująca na SDL (albo inna
fasada: mem/arena, tick/event, file, rysowanie tekstur — patrz §1).

## 1. Shim / fasada → LIVE (tylko z fasadą SDL/host)

Kiedy: efekt oryginału mapuje się 1:1 na SDL3/stdio/host I implementacja
bazująca na SDL istnieje (albo inna fasada: mem/arena, `SDL_GetTicks`/
`PumpEvents`/`PollEvent`, `SDL_IO`, `SDL_RenderTexture`/`SetRenderTarget`/
`SetPaletteColors`, `SDL_OpenAudioDeviceStream`). Dopiero wtedy wolno
opisać funkcję jako LIVE. Sygnatura funkcji gry NIE zmienia się; zmienia
się tylko backend.

- Kontrakty int-fd / liczników / capów bez zmian (`-1`/`-1L` on fail,
  short-count preserved, `len==0 → 0`, `NULL → NOENT`, OOR → `RANGE`).
- Błędy SDL nie mają `errno`: staging `DAT_1420_0010/4e7a` przez
  `dos_map_error` + `dos_error_from_errno` (`ENOENT→NOENT`, `ENOMEM→NOMEM`,
  reszta `IO`, short → `TRUNCATED`); `SDL_GetError()` tylko do logu.
- Tag `[PORT-SDL]` + `Replaces: orig-call -> SDL-call` w komentarzu.
- Dwie tablice int-fd → `SDL_IOStream*` (`win16_l*` i dos-owa) zostają
  ROZDZIELONE (dualizm `_L*` vs INT 21h, nigdy unifikacji w fali plików).
- Idempotentne API SDL wymaga bramki istnienia, gdy orig failuje na braku
  (precedens: `dos_unlink` — `SDL_RemovePath` zwraca true dla brakującej
  ścieżki, więc gate przez `SDL_GetPathInfo`, inaczej drugi unlink
  sfingowałby `CIV_OK`).

Przykłady LIVE: `win16_lopen/lread/lwrite/llseek/lclose` → `SDL_IO*`;
`win16_find_asset` → `SDL_GetPathInfo` + `SDL_EnumerateDirectory`;
`gfx_port_get_dc` → borrow `port->ren`; `gfx_port_release_dc` → walidowany
no-op (`NULL → NOENT`, obcy DC → `BAD_DATA`).

## 2. Wierny port + STUB / PARTIAL z TODO (gdy brak fasady lub brak danych)

Kiedy: brak implementacji bazującej na SDL (USER/GDI/KERNEL plumbing,
selektory, handlery okien, timery jako `SDL_AddTimer`, MCI, pełny commit
palety, blity CPU, pomiar fontów), ALBO efekt wymaga danych, których nie
mamy (dump niepotwierdzony, dekoder-zgad, runtime-only). Funkcję I TAK
portujemy 1:1 (parametry, gałęzie, matematyka, dyskryminatory, kształt
wołań Win16 jako wołania naszych `win16.*`/`video.*`/`dialog.*`), a brakującą
fasadę zostawiamy jako `TODO: Orig ...` z `CIV_ERR_UNSUPPORTED`. Nie
pomijamy funkcji dlatego, że dotyka Win16.

- `TODO: Orig ADDR opis brakującego mostka` w ciele; PARTIAL gdy połowa
  OS-free jest LIVE 1:1 (strażniki, matematyka, dyskryminatory), STUB gdy nic.
- Czego NIE robić: nie zgadywać dekoderów (RLE/tagged-frame, godpal),
  trzymać `*out untouched`; nie odtwarzać POSIX-errno 1:1 pod SDL
  (staging sprowadzony do `EIO/ENOENT/ENOMEM`, `EINTR`-retry znika).
- `WONTFIX-Win16` (uczciwy no-op `CIV_OK`) tylko dla kroków dowodliwie
  próżniowych na SDL, z komentarzem i adresem Orig.

Precedensy: `gfx_godpal_get` — paleta `0xec` budowana w runtime
(`DAT_1420_9966` to BSS-handle, brak statycznego LOGPALETTE w binarce),
więc STUB aż po runtime capture, nie `dd`; `mainport_get_or_create` —
PARTIAL-probe, port NIE zwracany dopóki commit palety stoi;
`dos_ioctl` — PARTIAL (whitelist direct-funcs LIVE, reszta UNSUPPORTED).

## 3. Mały refactor wewnątrz funkcji / zmiana sygnatury (dozwolone lokalnie)

Kiedy: ukryty stan GDI/DC utrudnia port (np. `SetColor(x)` + `PutPixel(x,y)`,
`SELECTOBJECT`/`GETDC`, globalny brush/DC). Wolno zamienić na jawny
parametr/kontekst (`putPixel(x,y,color)`, `port`/`ren`/`rect`/`pal`), o ile
zmiana NIE wychodzi poza funkcję. Kształt orig NIE WYRAŻALNY w SDL3
(przykład niżej) to nadal zmiana sygnatury — udowodniona spikiem, nie
uznana, nigdy po cichu.

- Dowód w komentarzu (np. dokładny komunikat SDL + macierz format/access).
  Precedens: `SDL_CreateTexture(INDEX8, TARGET)` →
  `Palettized textures can't be render targets` (ban na poziomie SDL,
  wszystkie backendy) — cała architektura "port = INDEX8 TARGET" do
  przeprojektowania (propozycja: RGBA8888 TARGET + CPU-kopia indeksów +
  paleta na porcie, upload przez `UpdateTexture`).
- Zasady zmiany/refactoru: nazwa + adres Orig ZOSTAJĄ; `Replaces`
  opisuje mapowanie orig-call → SDL-call (np. `SetColor+PutPixel →
  putPixel(x,y,color)`, `GETDC → borrow port->ren`, `SELECTOBJECT →
  jawny port/rect/pal`); wołający bez zmian poza podaniem już ustalonego
  jawnego kontekstu (port/ren); aktualizacja nagłówka + regen STATUS.
  Refactor NIE wychodzi poza funkcję — bez przeprojektowania architektury
  po cichu; brak wołających = zmiana tania (stan dziś dla `fill_or_copy`).
- Granica z §2: brak parametru (fill bez koloru — orig szedł przez
  `Gfx_MakeBrush @ 1098:00b5`) to NIE powód do zgadywania koloru
  (`RANGE`, nie fake-fill); to powód do jawnego parametru koloru / stanu
  fill-color / draw color w ramach tej samej funkcji — decyzja lokalna,
  nie architektoniczna.

## 4. Operacje bez twina SDL (POSIX-FALLBACK)

`isatty`/`fstat`, `stat`/`chmod`, `getcwd`, `ftruncate`, `remove`-fallback:
zostają W TEJ SAMEJ funkcji (nie osobny twin), każda linia z markerem
`/* POSIX-FALLBACK (no SDL twin) */` — gate grep to sprawdza per-linia.
Najpierw sprawdzić nagłówki SDL 3.5.0 z `/usr/local`; czego nie ma
(np. `SDL_TruncateIO`) — fallback, nie polifill.

## 5. Znaczniki (obowiązkowe)

- `[PORT-SDL]` — delegat z efektem; `[PORT-SDL + POSIX-FALLBACK]` — mieszany.
- `[HOST]` — sonda hosta bez orig-efektu (asset-probe, `getcwd`-fallback).
- `WONTFIX-Win16` — no-op `CIV_OK`, tylko próżniowy na SDL.
- `TODO: Orig seg:off ...` — tylko w STUB/PARTIAL; znika z chwilą LIVE.
- `Replaces:` — para orig-call → SDL-call w komentarzu funkcji; przy
  małym refactorze także mapowanie stanu ukrytego → jawnego
  (np. `SetColor+PutPixel → putPixel(x,y,color)`).
- Nowy kod tylko w plikach fali; zero `SDL_*` w
  `game/dialog/worldgen/newgame/map/save/rsc_loader/civ_common`
  (poza dozwolonym `sys_tick`) — funkcje gry wołają Win16-kształt przez
  nasze `win16.*`/`video.*`/`dialog.*`, nigdy SDL wprost.

## 6. Gate fali (niezmienny)

`make clean && make CC=clang-24` zero warnings → `nm -g civlin` pokazuje
symbole → headless `SDL_VIDEODRIVER=dummy CIVLIN_MAX_TICKS=5 ./civlin`
(uczciwy stop na frontierze, nigdy crash) → roundtrip danych fali
(`civ.prf`: missing→defaults, save→`0x1a`, determinizm, short→TRUNCATED;
case-warianty assetów) → grep: brak nowych `fopen/fread/...` poza liniami
`POSIX-FALLBACK` (followers z planu #8 wyłączeni) → `gen_status.py`:
funkcje opisane jako LIVE mają fasadę SDL i są bez `has TODO`; wiernie
portowane funkcje bez fasady mają `TODO: Orig ...` + `UNSUPPORTED`
(oczekiwane, nie failure gate).

## 7. Antywzorce

Fake-OK (CIV_OK bez efektu) · opisywanie jako LIVE bez fasady SDL ·
pomijanie funkcji dlatego, że dotyka Win16 · zgadywanie dekoderów/palet ·
cicha zmiana sygnatury / refactor wychodzący poza funkcję · scalanie tabel win16/dos ·
`SDL_AddTimer` zamiast store+poll (`win16_set/kill_timer` +
`sys_get_tick_scaled`/`sys_wait_next_tick`; `TIMERPROC @ 1008:0d68` tylko
tikuje countdown) · `SDL_GetError()` jako kontrakt · pisanie po
`civ1win/`/`unpacked/` (read-only, lokalne, nigdy w repo).
