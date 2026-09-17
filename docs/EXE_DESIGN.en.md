# Original Executable Analysis (English)

This document **accumulates** the structure and design confirmed in the original EZ2DJ executable. The Korean edition is [EXE_DESIGN.ko.md](EXE_DESIGN.ko.md), and both carry the same facts.

## Notation

| Marker | Meaning |
| --- | --- |
| **Confirmed** | Verified against the real binary or a run. The verification method is recorded with it. |
| **Inferred** | Reasoned from evidence but not yet verified. The evidence is recorded with it. |
| **Unresolved** | Not known yet. How to find out is recorded with it. |

Nothing goes in without evidence. When an item is confirmed, its marker changes and the verification method is recorded.

---

## 1. Current state

Inspected so far: EZ2DJ The 1st Tracks Special Edition, 2nd Trax, 3rd Trax, 4th Trax, 5th and 6th, plus EZ2Dancer 2nd MOVE (`ez2d2m`). Inputs come in two shapes — directory dumps and MAME CHD images. Most of what static analysis can settle is now settled; protection-layer responses and what needs a run remain.

The detailed evidence lives in the [HDD layout analysis](analysis/ez2dj-hdd-layout.md), the [executable structures analysis](analysis/ez2dj-exe-structures.md), and the [import surface analysis](analysis/ez2dj-import-surface.md). The structures document owns per-executable PE structure, protection anatomy, and data inventories, and gains a section whenever a new executable is identified. Only conclusions are kept here.

---

## 2. Confirmed items

### 2.1 Executable identification — confirmed

| Item | Value |
| --- | --- |
| 1st SE game executable | **`ez2dj.exe`** — named by the `shell=` entry in `System.ini` (protected) |
| 1st Tracks representative executable | **`Ez2DJ.exe`** — user-designated representative (`.protect`, protected) |
| 2nd Tracks representative executable | **`EZ2DJ.exe`** — user-provided representative (entry point is in `.text`; protection status unresolved) |
| 3rd game executable | `EZ2DJ.EXE` (protected, `.protect`) |
| 4th game executable | `EZ2DJ.exe` (protected, `.protect`) |
| EZ2Dancer 2nd MOVE executable | `EZ2Dancer.exe` (protected, `.protect`) |
| PE magic | PE32 (`0x10B`) throughout |
| Machine | i386 (`0x014C`) throughout |
| Image base | `0x00400000` throughout |
| Subsystem | Windows GUI (2) throughout |
| Base relocation | 1st SE `ez2dj.exe` has a `.reloc` section but an empty data directory, so it is fixed to its preferred base. 3rd and 4th carry a real relocation directory inside `.protect` |
| Build timestamps (PE TimeDateStamp) | `ez2dj.exe` 1999-12-24, 2nd `EZ2DJ.exe` 2004-07-18, 3rd `EZ2DJ.EXE` 2001-09-24 (directory) / 2001-10-15 (CHD), 4th `EZ2DJ.exe` 2002-07-18, `EZ2Dancer.exe` 2001-01-12 |
| Protection | 1st SE, 3rd, 4th and `ez2d2m` place their entry points in `.gtide` / `.protect` and are protected; 2nd's entry is in `.text`, so its status is unresolved |

**The canonical executables are loaded directly.** No separate bring-up build is used to sidestep the protection layer; the packer of the build the cabinet actually ran is carried through as-is.

### 2.2 Import list — confirmed

The surface the canonical `ez2dj.exe` presents to the loader is **7 DLLs and 161 functions**, 17 of which belong to the protection layer. For 3rd and 4th the original `.idata` holds 10 DLLs / 159 and 161. The full counts and priority order are in the [import surface analysis](analysis/ez2dj-import-surface.md).

| Item | Value |
| --- | --- |
| Graphics | **DirectDraw plus Direct3D Immediate Mode**. The runtime obtains Direct3D through `QueryInterface(IID_IDirect3D3)`, creates 121 XYZ/NORMAL/TEX1 vertices, null-size-locks the buffer, and fills an 11×11 grid at stride 32. Original call site `0x0042069d` invokes `DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST, vb, indices, 600, 0)` through global device `[0x01eb7cc0]` at vtable offset `+0x8c`. Confirmed draw state uses stage-zero texture/diffuse modulation, linear filtering, an RGB565 source color key, alpha testing, and ZERO/SRCALPHA versus ONE/ZERO blending. Lazy `%s.bmp` loading creates RGB565 `DDSCAPS_OFFSCREENPLAIN` surfaces, performs a GDI copy, and composes them through source-key `BltFast`/`Blt`. Calls through global `[0x01eb7cc0]` match the `IDirect3DDevice3` vtable. |
| Audio | **DirectSound** (ordinal `#1`) plus `winmm` mixer volume. It creates static buffers and a 360,448-byte looping ring, whose streaming path cyclically updates 45,056-byte PCM chunks inside whole-buffer locks. `GAMEASSIGNMENTS/DemoVolume` index 0..3 selects the DirectSound volume table `[-10000, -2222, -1111, 0]`. It also uses DuplicateSoundBuffer. |
| Input | **A single `GetAsyncKeyState`. No DirectInput** |
| Configuration | `GetPrivateProfile*` and `WritePrivateProfileStringA` — INI files |
| Registry | `RegFlushKey` only |
| Character encoding | Every API is the ANSI (`...A`) variant |
| Threads | `CreateThread`, events, critical sections, TLS — **multithreaded** |
| Ordinal imports | **Used** (`DSOUND.dll #1`) |
| Delay imports | Not used |

The 3rd and 4th builds additionally use `DINPUT.dll`, `AVIFIL32.dll`, and `WS2_32.dll`, grow the `USER32` surface from 21 to 32, and enter graphics through `DirectDrawCreateEx` rather than `DirectDrawCreate`, so per-version HLE profiles are required.

### 2.3 Assets and runtime paths — partly confirmed

| Item | State |
| --- | --- |
| HDD directory structure | **Confirmed** — 1st SE and 3rd differ from each other |
| Asset organisation | **Confirmed** — 1st SE has `Songs/` (68 entries) and a per-screen `System/` |
| Configuration files | **Confirmed** — `ez2dj.ini`, `System.ini` |
| Score storage | **Confirmed** — `rank_0.dat` through `rank_2.dat`, 400 bytes each |
| Guest working directory | **Confirmed (1st SE)** — `\ez2dj`, from `shell=d:\ez2dj\ez2dj.exe` in `System.ini`. `SetCurrentDirectoryA` is imported, so it may still change during a run |
| Drive letter | **Confirmed (1st SE)** — `D:`, same evidence |
| 1st Tracks guest path | **Unresolved** — the input has no `System.ini` |
| 3rd guest path | **Unresolved** — the 3rd dump has no `System.ini` |
| 2nd guest path | **Unresolved** — the 2nd dump has no `System.ini` |
| Asset file formats | **Unresolved** — the file structures under `Songs/` have not been opened yet |

### 2.4 Hardware boundary — unresolved

| Item | State |
| --- | --- |
| Arcade I/O board | **Partly confirmed** — the 3rd `EZ2DJ.INI` carries `"UseIOCard" = 1`, so an I/O card is definitely used. The protected 1st SE executable confirms its byte `IN`/`OUT` port range and active-low banks. Button, turntable, coin, and light meanings cross-checked against an independent public implementation remain marked **inferred** in the [I/O port map](analysis/ez2dj-io-map.md). `OUT 0x106` remains unresolved |
| Dongle or protection device | **Partially confirmed** — the protection stub opens `\\.\LPTDI1` and sends two IOCTLs shaped 4→8 bytes and 24→104 bytes. A zero first output DWORD is the advance condition at both stages, and the first stage retries up to three times. Applying the transform at 0x01ed4141 twice to the second input DWORD produces an eight-byte mask that is XORed with response offsets 4 through 11 to form the `.data` restoration state. Its first DWORD seeds `0x01ed7296`, advances once per byte with the same transform, and its low byte is subtracted from protected `.data`. Minimal target state `0900000000000000` repeatedly restores the normal initializer. This is a binary-restoration value, not confirmation of a physical-dongle key or vendor protocol. The first shape resembles HASP4 HaspCode, but the published classic-HASP path and packet differ from the full LPTDI interface, so the vendor remains unresolved. The 3rd executable uses a separate `\\.\FEnteDev` boundary with a `0x9c402468→450→44c→458` contract. `0x450` is a six-byte in-place packet; when the word at offset 2 matches marker `0xFAFA`, the helper returns the word at offset 4. Replaying historical synthetic bytes `0100fafa0010` confirms reachability of a Function-0 `0x44c` call. A nonzero byte at Function-0 descriptor offset `0xfe` is causal for retaining the handle and reaching Function-6 `0x44c`/Function-`0x0e` `0x458`; experimental value `0x0001` is not a physical-driver response, and the Function-`0x0e` output remains unresolved. Detailed evidence is in the [3rd Hardlock analysis](analysis/ez2dj3rd-hardlock-function-0e.md) |
| Timer source | **Confirmed** — `timeGetTime` |

---

## 4th Music Select Composition Observation (2026-09-05)

**Confirmed:** Frame 1000 of user run `20260905-174233-086` draws the background, discs, then ONE/ONE header artwork. Bottom/right UI pairs use destination-multiplication masks and additive artwork. The original also requests SRCBLEND=9 / DESTBLEND=6, which the previous HLE rejects. **Unresolved:** Attribution of those rejected draws to the missing header mask requires another run because the old failure-log budget was exhausted. [Detailed analysis](analysis/ez2dj4th-music-select-disc-state.md).

**Confirmed:** Follow-up run `20260905-185621-933` successfully processes the SRCBLEND=9 / DESTBLEND=6 draws for center mask texture 250 and header mask texture 280, and the user confirms the header occlusion now matches the original. The previous missing destination-color blend support dropped the entire mask draw and caused the visual difference.

## 3. Update rules

* When a new fact is confirmed, update this document and [EXE_DESIGN.ko.md](EXE_DESIGN.ko.md) in the same task.
* Keep detailed per-topic evidence under `docs/analysis/` and leave only conclusions and links here.
* Do not transcribe raw byte dumps. Record structures, offsets, and observed behavior.

## 2026-09-06 2nd execution-boundary correction

2nd execution logs confirmed the absent `GetPrivateProfileIntA` import, legacy-I/O helper RVAs `0x000782d7` and `0x0007832b`, and the `DirectDrawCreateEx` HLE connection. The 2nd profile therefore does not inject demo-volume, and the D3D IAT exception is limited to 4th, where packer data must be preserved. Detailed evidence is recorded in the [executable structures analysis](analysis/ez2dj-exe-structures.md) and [I/O port map](analysis/ez2dj-io-map.md).

## 2026-09-10 The EZ2Dancer 2nd MOVE executable (`ez2d2m`)

**Confirmed:** this is the first non-EZ2DJ product covered here. `ez2dancer/EZ2Dancer.exe` is PE32/i386 with image base `0x00400000`, entry point RVA `0x00401240`, SizeOfImage `0x0043b000`, timestamp `0x3a5f074c`, and four sections: `.text`, `.rdata`, `.data` and `.protect`. Its entry point lies inside a writable, executable `.protect` section, making it the same self-modifying packer family as EZ2DJ 1st, 1st SE, 3rd, 4th and 5th. Its strings carry `\.\FEnteDev`, `\.\HARDLOCK.VXD`, `HLW32Proc`, `API_1LNM.DLL` and `WTSQuerySessionInformationA`, so the protection envelope is the same as well.

**Confirmed:** the packed import directory holds one stub per DLL, and its graphics entry point is `DirectDrawCreateEx` rather than `DirectDrawCreate`. `GetCommandLineA`, `GetWindowsDirectoryA` and `GetPrivateProfileIntA` are absent, so the `ez2d2m` profile leaves those three boundaries off and takes the DirectDraw 7 path.

**Inferred:** its cabinet I/O is 16-bit-wide over ports `0x300` to `0x30c` rather than EZ2DJ's byte-wide `0x100` to `0x106`. The evidence is a single public implementation and was not verified against the original. **Unresolved:** this executable's raw-I/O helper RVAs, its Hardlock response and descriptor, and whether it runs at all.

The detailed evidence is in [ez2d2m CHD filesystem and executable observations](analysis/ez2d2m-chd-filesystem.md) and [the EZ2Dancer I/O port map](analysis/ez2dancer-io-map.md).

## 2026-09-11 The two kinds of `ez2d2m` Hardlock transform

**Confirmed:** the protection sends two kinds of transform — eleven one-block `function=0x000e` requests and one seven-block `function=0x0011` request. The first eleven carry identical values on two machines. In the `0x0011` request only block 5 is identical in every capture; blocks 0, 1 and 6 move with the re2DJ runtime's load address, and blocks 2 to 4 differ between machines.

**Inferred:** by 2EZConfig-V2's high-level contract, consulted for facts only, `0x000e` is `API_CRYPT` and `0x0011` is `API_CODE`. `API_CODE` computes once from the second-to-last block — block 5 — and writes several places in the payload, so its answer is a function of the whole request. The response table gained request rows to match ([design 247](design/20260911-247-hardlock-payload-response-rows.md)). **Unresolved:** a valid `0x0011` answer.

## 2026-09-15 Unanalyzed `roms/` executables added (task 287)

**Confirmed:** every game executable in the user-supplied `roms/` input that had no structural analysis was measured and added to the [executable structures analysis](analysis/ez2dj-exe-structures.md) as sections 6, 7 and 8. Size, MD5, SHA-1 and SHA-256 are now recorded alongside each executable identification.

**Confirmed — 5th.** `roms/ez2dj5th/ez2dj/EZ2DJ.exe` is 1,388,544 bytes with PE TimeDateStamp `0x3f53377b` (2003-09-01), a protected build whose entry point lies in `.protect`. Its original `.idata` holds 10 DLLs / 161 functions and its **DLL list and function-name set are exactly those of 4th**, so 5th demands no Win32 API beyond what 4th already needs from the HLE. Its packed table has the same 36-entry shape as 4th's, differing only in six per-DLL representative stubs.

**Confirmed — 6th.** 6th has three executables and none of them carries a protection section. The `EZ2DJ.EXE` the cabinet runs (126,976 bytes) is a launcher; the real game is its `EZ2DJ6th.EXE` child (585,728 bytes). The bootstrap's plaintext strings hold two child paths — `.\EZ2DJ6TH.EXE` and also `.\EZ2DJ1ST\EZ2DJ.EXE`. The game body imports 7 DLLs / 137 functions, dropping `ADVAPI32`, `AVIFIL32` and `WS2_32` entirely against 4th and 5th, so 6th requires neither an AVI-playback nor a Winsock boundary.

**Confirmed — the 6th's bundled 1st Tracks.** `EZ2DJ/Ez2Dj1st/` inside `6th.chd` holds a complete 1st Tracks layout whose `Ez2DJ.exe` (360,448 bytes, `0x411bbf5c`, 2004-08-12) is an **unprotected rebuild**: its 6 DLLs / 138 functions read directly without unpacking. Together with `EZ2DJ6th.EXE` it is the only input whose game surface can be read statically without defeating protection.

**Confirmed — 1st Tracks.** The original `.idata` of `roms/ez2dj1st/ez2dj/Ez2DJ.exe` (577,536 bytes, `0x3862fd9d`) holds 7 DLLs / 141 functions, a **strict subset** of the 144 of 1st SE; 1st SE adds only `GDI32!BitBlt`, `GDI32!SetBkColor` and `KERNEL32!GetWindowsDirectoryA`. The executables in the `ez2dj` and `ez2dj1` directories are byte-identical.

**Confirmed — 1st SE is two builds.** The directory dump (`.gtide` packer, 561,152 bytes) and the CHD copy (`.protect` packer, 634,880 bytes) share a PE TimeDateStamp but are different files. Their `.text`, `.rdata`, `.data` and `.reloc` share layout but differ in content, while `.idata` alone is byte-identical: the packer transforms only the body and leaves the original import-table section untouched.

**Unresolved:** whether the 5th directory layout came from `ez2dj5.chd`; when the 6th bootstrap selects each of its two children; why the 6th `EZ2DJ.INI` is not plaintext and how it is consumed; and the Hardlock response contracts of the three newly covered products.

## 2026-09-17 Protected-build decryption and 3rd game state (tasks 291-297)

Analysis: [runtime decryption in protected builds](analysis/protected-build-runtime-decryption.md), [I/O port map](analysis/ez2dj-io-map.md), [3rd frame pacing](analysis/ez2dj3rd-frame-pacing.md), [3rd demo play and the settings registry](analysis/ez2dj3rd-demo-play.md)

**Confirmed — a `.protect` build's entry breakpoint precedes decryption.** In all six builds (1st Tracks, 1st SE CHD, 3rd, 4th, 5th, `ez2d2m`) a dump taken right after the entry is restored holds none of the original strings or port helpers, which appear in a dump taken once the guest has run for a few seconds; for 3rd the two dumps differ across 36.3% of the image. Static analysis uses the resumed dump (`--image-dump`'s `resumed`).

**Confirmed — each build's legacy-I/O helper RVAs are its own.** Searching resumed dumps by the byte signatures of the compiler runtime's `inp`/`outp` block finds `inportb` and `outportb` exactly at the profile values in all five byte-width builds. The block's output-side gap differs between 1st/1st SE and 3rd/4th/5th. `ez2d2m` inlines its port I/O into game code and is outside the signatures.

**Confirmed — 3rd is an MSVC debug configuration**, with `0xcccccccc` local fills, stack checks and incremental-link thunks, so function boundaries and call targets read out statically.

**Confirmed — 3rd parses its own INI, and its settings are a key-to-address registry.** The format is sectionless with quoted keys, and 24 of the INI's 25 keys are bound to the settings object `[0x004edbc8]`, the remaining `UseIOCard` being unreferenced by code. There is no `AutoPlay` key. (Corrected in task 300 from "exactly the INI's 25 keys".)

**Confirmed — 3rd's demo and autoplay are separate flags.** The demo start routine `0x0048aa31` runs the game scene synchronously and sets, as a pair, the demo flag `[0x00a2946c]` (overlay, unbound input, muting, ending on input) and the autoplay flag `[0x00a29508]`. Writing only the autoplay flag to 1 makes the game hit notes without demo side effects, and the value is **latched at song start**. The input manager flips it every frame slot `0x1b` is pressed; that slot's physical binding is **unresolved**.

**Confirmed — 3rd's frame loop is a `target - elapsed` sleep feedback.** The target period is **inferred** at about 17 ms with the original constant **unresolved**, and today's frame rate depends on statically linked SDL3's default `timeBeginPeriod(1)`.

**Confirmed — 3rd hides the mouse cursor**, importing `ShowCursor` and `SetCursor`; re2DJ restores it at the window boundary.

## 2026-09-18 4th demo and autoplay flags (task 300)

Analysis: [4th demo play and the autoplay flag](analysis/ez2dj4th-demo-play.md)

**Confirmed — 4th shares 3rd's structure.** In the TimeDateStamp `0x3d369bfd` build the settings registry `[0x005111e0]` (24 keys, no `AutoPlay`), the demo flag `[0x00ac290c]`, the demo start routine `0x004a4430` and the autoplay flag `[0x00ac29b0]` (setter `0x00437600`, getter `0x004375f0`) connect exactly as in 3rd. Both flags are 1 together only during demos in attract, and the user confirmed that turning autoplay on in the OSD makes a song play itself. The note-judgement getter sites come in two sets, **inferred** to be two game modes.

## 2026-09-18 5th demo and autoplay flags (task 301)

Analysis: [5th demo play and the autoplay flag](analysis/ez2dj5th-demo-play.md)

**Confirmed — 5th shares the 3rd and 4th structure.** In the TimeDateStamp `0x3f53377b` build the settings registry `[0x00516350]` (26 keys, no `AutoPlay`), the demo flag `[0x00aee194]`, the demo start routine `0x004aabf7` and the autoplay flag `[0x00aee238]` (setter `0x00437790`, getter `0x00437780`) connect the same way. Both flags are 1 together only during demos in attract, and the user confirmed that turning autoplay on in the OSD makes a song play itself.

**Inferred — 5th adds partial-auto settings.** `AutoScratch` and `AutoPedal`, absent in 4th, are bound in the registry, and the first judgement set's note-data field moved 12 bytes later (`+0x1d8`). Their actual behavior is unresolved.

## 2026-09-18 1st SE scene engine and autoplay flag (task 302)

Analysis: [1st SE self-playing scenes and the autoplay flag](analysis/ez2dj1stse-demo-play.md)

**Confirmed — the 1st SE CHD build is a scene engine.** The TimeDateStamp `0x3862df27` build registers 60 scenes through `0x00423670`; the demo is the dedicated scenes `DemoGame` and `ClubMixDemoGame`, and the overlay is the `ShowDemoPlay` scene, unlike 3rd-5th's demo flag over a normal game scene.

**Confirmed — the autoplay flag is `[0x01c3f3a4]`.** The three self-playing scenes (`DemoGame`, `ClubMixDemoGame`, `HowToPlayGame`) write it to 1 in their init callback and 0 in their destroy callback, the player scenes read it for their auto/manual branch and input handling, and it is passed into the chart player when a song starts. A read-only attract poll and the user's OSD check confirmed it.

**Confirmed — sound-effect buffers are created with `0x000140e2`** (`DSBCAPS_STATIC | GETCURRENTPOSITION2` and others). That the DirectSound HLE currently classifies them as streaming is a re2DJ problem, not the original's structure.

## 2026-09-18 1st Tracks dedicated demo player scenes (task 304)

Analysis: [1st Tracks self-playing scenes](analysis/ez2dj1st-demo-play.md)

**Confirmed — 1st Tracks is also a scene engine, but with dedicated demo player scenes.** The TimeDateStamp `0x3862fd9d` build registers 53 scenes through `0x00424280` and keeps `DemoPlayer` and `ClubMixDemoPlayer` apart from the normal `player`. Only the demo scenes pass a constant 1 to the chart player `0x0041af60`; the value it stores, `[0x0055bc4c]`, is read only by the chart module, and writing 1 mid-song changed nothing.

**Inferred — 1st Tracks has no switchable autoplay variable.** Automatic play is taken to live in the demo player scenes' duplicated code, and it is not expressed as `game_controls`.

## 2026-09-18 EZ2Dancer 2nd MOVE demo and autoplay flag (task 305)

Analysis: [EZ2Dancer 2nd MOVE demo and the autoplay flag](analysis/ez2d2m-demo-play.md)

**Confirmed — `ez2d2m` is built from paired classes.** The TimeDateStamp `0x3a5f074c` build has no scene registration function; each class such as `NormalGame` and `DemoGame` carries its own `OnCreateGame`, `OnDestroyGame` and Director, and each function passes its own name string to the logging function.

**Confirmed — the autoplay flag is `[0x007fa424]`.** `NormalGame::OnCreateGame` puts channels 3-`0x12` in automatic mode only when it is 1, where the demo writes the same values as constants, and a toggle built into the game flips it from input slot `0xc`. Because the demo does not use the flag, attract polling cannot confirm it; the OSD toggle did.
