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

Two dumps have been inspected: EZ2DJ The 1st Tracks Special Edition and 3rd Trax. Most of what static analysis can settle is now settled, and what remains needs a run.

The detailed evidence lives in the [HDD layout analysis](analysis/ez2dj-hdd-layout.md), the [executable structures analysis](analysis/ez2dj-exe-structures.md), and the [import surface analysis](analysis/ez2dj-import-surface.md). The structures document owns per-executable PE structure, protection anatomy, and data inventories, and gains a section whenever a new executable is identified. Only conclusions are kept here.

---

## 2. Confirmed items

### 2.1 Executable identification — confirmed

| Item | Value |
| --- | --- |
| 1st SE game executable | **`ez2dj.exe`** — named by the `shell=` entry in `System.ini` (protected) |
| 1st SE bring-up build | `ez2dj1.exe` (not protected). Not what the cabinet ran |
| 3rd game executable | `EZ2DJ.EXE` (protected) |
| PE magic | PE32 (`0x10B`) throughout |
| Machine | i386 (`0x014C`) throughout |
| Image base | `0x00400000` throughout |
| Subsystem | Windows GUI (2) throughout |
| `.reloc` | Section present, but `ez2dj1.exe` has an empty base-relocation data directory and is fixed to its preferred base |
| Build timestamps | `ez2dj1.exe` 1999-12-24, `ez2dj.exe` 2000-01-01, `EZ2DJ.EXE` 2001-09-24 |
| Protection | Only `ez2dj1.exe` is unprotected; the others hold their entry point in a `.gtide` or `.protect` section |

**`ez2dj1.exe` is the bring-up target for Stages 2 and 3** — the one build that reaches real game code without executing a protection layer first.

### 2.2 Import list — confirmed

**7 DLLs and 144 functions** for `ez2dj1.exe`. The full list and priority order are in the [import surface analysis](analysis/ez2dj-import-surface.md).

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

The 3rd build additionally uses `DINPUT.dll`, `AVIFIL32.dll`, and `WS2_32.dll`, so per-version HLE profiles are required.

### 2.3 Assets and runtime paths — partly confirmed

| Item | State |
| --- | --- |
| HDD directory structure | **Confirmed** — 1st SE and 3rd differ from each other |
| Asset organisation | **Confirmed** — 1st SE has `Songs/` (68 entries) and a per-screen `System/` |
| Configuration files | **Confirmed** — `ez2dj.ini`, `System.ini` |
| Score storage | **Confirmed** — `rank_0.dat` through `rank_2.dat`, 400 bytes each |
| Guest working directory | **Confirmed (1st SE)** — `\ez2dj`, from `shell=d:\ez2dj\ez2dj.exe` in `System.ini`. `SetCurrentDirectoryA` is imported, so it may still change during a run |
| Drive letter | **Confirmed (1st SE)** — `D:`, same evidence |
| 3rd guest path | **Unresolved** — the 3rd dump has no `System.ini` |
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
