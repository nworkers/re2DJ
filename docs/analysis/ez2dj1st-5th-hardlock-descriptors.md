# ez2dj1st·ez2dj5th Hardlock descriptor 관측

## 한국어

### PE 배치

두 실행 파일 모두 1st SE의 CHD 빌드, 3rd, 4th와 같은 `.protect` 계열입니다. import directory가 `.protect` 안에 있고 원본 `.idata`는 파일에 남아 있으나 header가 가리키지 않습니다.

| 항목 | `ez2dj1st` | `ez2dj5th` |
| --- | --- | --- |
| 실행 파일 | `ez2dj/Ez2DJ.exe` | `EZ2DJ/EZ2DJ.exe` |
| section | 6개 | 6개 |
| timestamp | `0x3862fd9d` | `0x3f53377b` |
| image base | `0x00400000` | `0x00400000` |
| entry RVA | `0x0199b240` | `0x0070d240` |
| size of image | `0x019b6000` | `0x00746000` |
| `.protect` | `0x0199b000` | `0x0070d000` |
| import directory | `0x019b5620` | `0x007458e0` |
| 원본 `.idata` | `0x01985000` | `0x006fe000` |

**확인됨.** `ez2dj1st`의 fingerprint(`Ez2DJ.exe`, entry `0x0199b240`, size of image `0x019b6000`)는 built-in profile에 이미 기록된 값과 일치합니다.

### descriptor에 도달하기까지

[추출 절차](../guides/hardlock-descriptor-extraction.md)의 기본 옵션만으로는 두 실행 모두 descriptor에 도달하지 못합니다. 두 단계가 더 필요합니다.

**1. 동적 해석.** 기본 옵션만으로 실행하면 `CreateFileA`가 `route=win32`로 해석됩니다. 게스트가 호스트의 실제 장치를 열려 하고 곧 `ExitProcess`합니다.

```
dynamic-resolver:name=CreateFileA:route=win32:address=0x763de9f0:caller=0x01d9e349
...
exit-process:route=exit_process:code=8
exit-process-hardlock:total=0:initialize=0:handshake=0:descriptor=0:transform=0
```

`--hle-dynamic-vfs`를 켜면 `\\.\NTICE` 실패(error 123) 뒤 `\\.\FEnteDev`가 열리고 `0x9c402468` initialize와 `0x9c402450` handshake까지 갑니다.

**2. handshake 응답.** 응답을 판정해 통과하지 못하면 게스트는 `0x9c40244c` descriptor를 요청하지 않고 종료합니다. 응답 없이 실행하면 initialize 2회, handshake 3회에서 멈춥니다.

[작업 109](../work-logs/20260901-109-ez2dj3rd-hardlock-450-response.md)가 3rd에서 확인한 helper 동작을 씁니다. marker word가 `0xFAFA`가 아니면 0을 반환하고, 같으면 세 번째 word를 반환합니다. 그때의 synthetic oracle을 `--device-mock-hardlock-450-response`로 재생하면 descriptor에 도달합니다.

### descriptor 헤더

| 항목 | `ez2dj1st` | `ez2dj5th` |
| --- | --- | --- |
| `header_valid` | 1 | 1 |
| `module_id` | `0x0000` | `0x0000` |
| `module_address` | `0x15e5` | `0x4c5c` |
| `data_address` | `0x00000000` | `0x00000000` |
| `block_count` | 0 | 0 |
| `function` | `0x0000` | `0x0000` |
| `status` | `0x0000` | `0x0000` |
| `remote` | `0x0001` | `0x0001` |
| `port` | `0x0378` | `0x0378` |
| descriptor 요청 | 2회 | 2회 |

`id_ref`와 `id_verify`는 두 프로파일 모두 non-zero이고 모든 요청에서 같은 값입니다. **원문은 저장소에 기록하지 않고 Git이 무시하는 `cfg/hardlock-id.ini`의 해당 section에만 두었습니다.** 1st SE와 6th에서 쓴 것과 같은 정책입니다.

`module_address`는 계열을 그대로 드러냅니다.

| 제품 | `module_address` |
| --- | --- |
| `ez2dj1stse` | `0x15e1` |
| `ez2dj1st` | `0x15e5` |
| `ez2dj6th` | `0x4c51` |
| `ez2dj5th` | `0x4c5c` |

**확인됨.** 1st는 1st SE와 같은 `0x15xx` 대역, 5th는 6th와 같은 `0x4cxx` 대역입니다.

### 장치 경계

두 실행 모두 `\\.\NTICE`를 먼저 열려다 error 123으로 실패한 뒤 `\\.\FEnteDev`를 엽니다. `\\.\LPTDI`는 열지 않습니다. 1st SE의 CHD 빌드와 같습니다.

| 항목 | `ez2dj1st` | `ez2dj5th` |
| --- | --- | --- |
| `\\.\NTICE` | 실패 1회 | 실패 1회 |
| `\\.\FEnteDev` | 성공 4회 | 성공 3회 |
| initialize | 2 | 1 |
| handshake | 3 | 3 |
| descriptor | 2 | 2 |

### ez2dj1st 실행 기본값 정정

프로파일이 정정 이전 1st SE의 값을 물려받았으므로 관측과 대조했습니다.

| 항목 | 이전 | 정정 | 근거 |
| --- | --- | --- | --- |
| `hle_windows_directory` | `true` | `false` | 켜면 `handoff_prepared=false`. 이 빌드의 import table에 `GetWindowsDirectoryA`가 없습니다 |
| `hle_dynamic_vfs` | 미설정 | `true` | 없으면 `CreateFileA`가 `route=win32`로 가고 Hardlock 요청이 0건입니다 |
| `device_mock_path_prefix` | `\\.\LPTDI` | `\\.\FEnteDev` | 장치 추적에서 확인 |
| `device_mock_target_state_hex` | `0900000000000000` | 해제 | `\\.\LPTDI`를 열지 않으므로 답할 대상이 없습니다 |
| `hardlock_cfg_material_default` | 미설정 | `true` | 같은 Hardlock 계열 |

`hle_command_line`, `hle_d3d3`, `hle_directsound`, `demo_volume`, 레거시 I/O 포트, `run_detached`는 유지했습니다. 1st SE의 CHD 빌드와 달리 이 빌드는 `DirectDrawCreate`와 `GetPrivateProfileIntA`를 가지고 있어 `d3d3_prepared`와 `demo_volume_prepared`가 모두 참입니다.

정정 후 `re2dj ez2dj1st`가 preparation 전 항목 true, `iat_verified=true`, outcome `success`로 게스트를 실행합니다.

레거시 I/O helper RVA는 1st SE `.gtide` 빌드의 `0x00038987`·`0x000389ab`가 그대로 남아 있었습니다. 아래 판별 과정에서 이 값이 틀렸음을 확인하고 정정했습니다.

### ez2dj1st transform 응답 판별

사용자가 준비한 후보 map 134개를 [워크스루](../guides/hardlock-seed-recovery-walkthrough.md) Stage 7의 기준으로 전수 판별했습니다. 실행마다 종료 코드, `.vfs.log` 줄 수, IOCTL 종류별 횟수, 자산 개방 수를 기록하고 각 실행 뒤 잔여 게스트 프로세스를 회수했습니다. 실행당 약 5.7초, 전체에 약 13분이 걸렸습니다.

첫 판별에서는 어느 후보도 정상 종료하거나 자산을 열지 못했습니다.

| 관찰 | 후보 133개 | `candidate-84` |
| --- | --- | --- |
| 종료 코드 | `0xc0000005` 93, `0xc0000096` 28, `0xc000001d` 12 | `0xc0000096` |
| `.vfs.log` 줄 수 | 245 또는 247 | 275 |
| descriptor 요청 | 16 | 17 |
| 자산 개방 | 0 | 0 |

`candidate-84`만 transform loop를 넘어갑니다. 그리고 그것이 멈추는 지점이 `0xc0000096`(privileged instruction), 즉 트랩되지 않은 port I/O입니다. 프로파일의 레거시 I/O helper RVA가 이 빌드에서 확인되지 않은 값이었으므로, 응답이 틀린 것이 아니라 I/O 경계가 준비되지 않은 것이라는 뜻입니다.

crash 지점 RVA `0x00035757` 주변의 code window를 디코드했습니다.

```
0x00035750  33 c0              xor eax, eax
0x00035752  66 8b 54 24 04     mov dx, [esp+4]
0x00035757  ec                 in al, dx          ← fault
0x00035758  c3                 ret
...
0x00035770  33 c0              xor eax, eax
0x00035772  66 8b 54 24 04     mov dx, [esp+4]
0x00035777  8a 44 24 08        mov al, [esp+8]
0x0003577b  ee                 out dx, al
0x0003577c  c3                 ret
```

**확인됨.** runtime은 예외 주소를 RVA와 비교하므로 helper 진입점이 아니라 opcode 바이트의 주소를 씁니다. `in`은 `0x00035757`, `out`은 `0x0003577b`입니다. 이전에 있던 1st SE `.gtide` 값 `0x00038987`·`0x000389ab`는 다른 빌드의 것이며 이 실행 파일에서는 한 번도 일치하지 않았습니다.

정정된 RVA로 `candidate-84`를 다시 실행했습니다.

| 관찰 | 정정 전 | 정정 후 |
| --- | --- | --- |
| `.vfs.log` 줄 수 | 275 | 1875 |
| 자산 개방 | 0 | 484 (고유 경로 248개) |
| transform | 15 | 15 |
| crash | `0xc0000096` | 진입부 anti-debug `0xc0000005`만 |

게스트가 `System\WarningMsg\WarningMsg.bmp`, `System\CompanyLogo\logo.str`, `AMUSEWORLD_BG.bmp` 등 자기 아트워크를 읽습니다. 오답 후보가 우연히 원본 자산의 실제 경로를 만들어 낼 수 없으므로 **이 후보의 응답 map이 옳다는 것은 확정입니다.**

`response450`과 `tail44c`는 3rd·4th·1st SE와 같은 값이 성립합니다. 네 제품이 같은 handshake 재생값을 씁니다.

확정된 자료는 저장소가 아니라 Git이 무시하는 `cfg/` 아래에만 두었습니다. `re2dj ez2dj1st`가 추가 옵션 없이 그 자료를 읽어 `hardlock_cfg_material`의 세 항목을 모두 인식하고 자산 484건을 엽니다.

### ez2dj5th transform 응답 판별

후보 map 176개를 같은 기준으로 전수 판별했습니다.

| 관찰 | 후보 175개 | `candidate-135` |
| --- | --- | --- |
| 종료 코드 | `0xc0000005` 136, `0xc0000096` 28, `0xc000001d` 11 | `0xc0000096` |
| `.vfs.log` 줄 수 | 329 또는 331 | 363 |
| descriptor 요청 | 38 | 40 |
| 자산 개방 | 0 | 0 |

1st와 같은 모양입니다. `candidate-135`만 transform loop를 넘어가고, 멈추는 지점이 트랩되지 않은 port I/O입니다. 5th 프로파일은 4th의 helper RVA를 물려받았으므로 이 빌드에서 확인된 값이 아니었습니다.

crash 지점 RVA `0x000ca067` 주변의 code window를 디코드해 이 빌드의 helper 쌍을 얻었습니다. `in`은 `0x000ca067`, `out`은 `0x000ca09b`입니다. 물려받았던 4th 값 `0x000c3817`·`0x000c384b`는 이 실행 파일에서 한 번도 일치하지 않았습니다.

정정된 RVA로 `candidate-135`를 다시 실행했습니다.

| 관찰 | 정정 전 | 정정 후 |
| --- | --- | --- |
| `.vfs.log` 줄 수 | 363 | 2493 |
| 자산 개방 | 0 | 67 |
| transform | 37 | 37 |
| crash | `0xc0000096` | 진입부 anti-debug `0xc0000005`만 |

게스트가 `LOGO.str`, `Title.str`, `2PLAYERInsertCoin.str`, `2PLAYERPressStart.str` 등 자기 자산을 읽습니다. **이 후보의 응답 map이 옳다는 것은 확정입니다.**

확정 자료를 `cfg/`에 배치한 뒤, Hardlock 옵션을 하나도 주지 않고 프로파일 기본값만으로 실행해 `hardlock_cfg_material`의 세 항목이 모두 인식되고 transform 37건, 자산 71건이 나오는 것을 확인했습니다.

`response450`과 `tail44c`는 여기서도 3rd·4th·1st SE·1st와 같은 값이 성립합니다. **다섯 제품이 같은 handshake 재생값을 씁니다.**

### ez2dj5th CHD 마운트

`re2dj ez2dj5th`는 `CHD MBR has no in-range FAT32 partition`으로 실패합니다. CHD probe가 이유를 보여 줍니다.

```
logical_bytes=20842827264 hunk_bytes=4096 unit_bytes=512
metadata tag=GDDD value=CYLS:646169,HEADS:3,SECS:21,BPS:512
lba0_prefix=eb 58 90 4d 53 57 49 4e 34 2e 31 00 02 20 24 00 signature=55aa
```

**확인됨.** LBA 0의 앞부분이 `eb 58 90`(short jump)에 이어 OEM 문자열 `MSWIN4.1`입니다. 이 이미지는 파티션 테이블이 없는 whole-disk FAT32 볼륨이고, LBA 0 자체가 FAT32 boot sector입니다. 현재 reader는 LBA 0을 MBR로 읽고 파티션 항목에서 FAT32 형식을 찾으므로 이 배치를 인식하지 못합니다.

이 작업의 descriptor 관측은 CHD가 아니라 `roms/ez2dj5th/ez2dj`의 추출된 디렉터리를 source root로 써서 수행했습니다. 실행한 것은 5th의 실제 실행 파일입니다.

## English

### PE layout

Both executables are the same `.protect` family as the 1st SE CHD build, 3rd, and 4th: the import directory sits inside `.protect` while the original `.idata` survives in the file without the header pointing at it.

`ez2dj1st` runs `ez2dj/Ez2DJ.exe` with six sections, timestamp `0x3862fd9d`, entry RVA `0x0199b240`, size of image `0x019b6000`, `.protect` at `0x0199b000`, its import directory at `0x019b5620`, and the original `.idata` at `0x01985000`. `ez2dj5th` runs `EZ2DJ/EZ2DJ.exe` with six sections, timestamp `0x3f53377b`, entry RVA `0x0070d240`, size of image `0x00746000`, `.protect` at `0x0070d000`, its import directory at `0x007458e0`, and the original `.idata` at `0x006fe000`.

**Confirmed.** The `ez2dj1st` fingerprint already recorded in the built-in profile matches this executable.

### Reaching the descriptor

The [extraction procedure](../guides/hardlock-descriptor-extraction.md) alone reaches no descriptor for either executable; two further steps are needed.

First, dynamic resolution. With the procedure's options alone, `CreateFileA` resolves at `route=win32`, the guest opens the host's real device and calls `ExitProcess` with no Hardlock request at all. With `--hle-dynamic-vfs` it tries `\\.\NTICE`, fails with error 123, opens `\\.\FEnteDev`, and reaches the `0x9c402468` initialize and `0x9c402450` handshake.

Second, a handshake response. The guest judges the response and, failing it, exits without requesting the `0x9c40244c` descriptor — two initialize and three handshake requests and no more. Replaying the synthetic oracle from [task 109](../work-logs/20260901-109-ez2dj3rd-hardlock-450-response.md), whose helper returns the third word only when the marker word is `0xFAFA`, reaches the descriptor.

### Descriptor header

Both report `header_valid=1` with `module_id=0x0000`, `data_address=0x00000000`, `block_count=0`, `function=0x0000`, `status=0x0000`, `remote=0x0001`, and `port=0x0378`, over two descriptor requests each. `module_address` is `0x15e5` for `ez2dj1st` and `0x4c5c` for `ez2dj5th`.

`id_ref` and `id_verify` are non-zero for both and identical across requests. **Their raw values are not recorded in the repository; they live only in the Git-ignored `cfg/hardlock-id.ini` section for each profile**, the same policy used for 1st SE and 6th.

The addresses show the family directly: `ez2dj1stse` is `0x15e1` and `ez2dj1st` is `0x15e5`, while `ez2dj6th` is `0x4c51` and `ez2dj5th` is `0x4c5c`. **Confirmed** — 1st sits in the same `0x15xx` band as 1st SE, and 5th in the same `0x4cxx` band as 6th.

### Device boundary

Both executables try `\\.\NTICE` first, fail with error 123, then open `\\.\FEnteDev`, and neither opens `\\.\LPTDI` — the same as the 1st SE CHD build. `ez2dj1st` opens the device four times with two initialize, three handshake and two descriptor requests; `ez2dj5th` opens it three times with one initialize, three handshake and two descriptor requests.

### Correcting the ez2dj1st execution defaults

The profile inherited the pre-correction 1st SE values, so each was checked against observation. `hle_windows_directory` becomes `false` because turning it on gives `handoff_prepared=false` — `GetWindowsDirectoryA` is not in this build's import table. `hle_dynamic_vfs` becomes `true` because without it `CreateFileA` goes to `route=win32` and no Hardlock request is made at all. `device_mock_path_prefix` becomes `\\.\FEnteDev` as the device trace shows, and `device_mock_target_state_hex` is cleared because the `\\.\LPTDI` device this build never opens has nothing to answer. `hardlock_cfg_material_default` becomes `true` for the same Hardlock family.

`hle_command_line`, `hle_d3d3`, `hle_directsound`, `demo_volume`, the legacy I/O ports and `run_detached` all stay: unlike the 1st SE CHD build, this one carries `DirectDrawCreate` and `GetPrivateProfileIntA`, so `d3d3_prepared` and `demo_volume_prepared` are both true.

With the corrections `re2dj ez2dj1st` runs the guest with every preparation item true, `iat_verified=true`, and outcome `success`.

The legacy-I/O helper RVAs still held the 1st SE `.gtide` values `0x00038987` and `0x000389ab`. Judging the candidates below showed them to be wrong and established the right ones.

### Judging the ez2dj1st transform responses

The 134 candidate maps the user prepared were judged exhaustively against the [walkthrough](../guides/hardlock-seed-recovery-walkthrough.md) Stage 7 criteria, recording each run's exit code, `.vfs.log` line count, per-kind IOCTL counts and asset opens, and reclaiming leftover guest processes after each. A run took about 5.7 seconds and the sweep about thirteen minutes.

On the first pass no candidate exited cleanly or opened an asset. 133 of them crashed with `0xc0000005` (93), `0xc0000096` (28) or `0xc000001d` (12), producing 245 or 247 trace lines and sixteen descriptor requests. `candidate-84` alone produced 275 lines and seventeen descriptor requests — the only one to get past the transform loop — and it stopped at `0xc0000096`, a privileged instruction, meaning untrapped port I/O. Since the profile's legacy I/O helper RVAs were unconfirmed for this build, that pointed at an unprepared I/O boundary rather than a wrong response.

Decoding the code window around the faulting RVA `0x00035757` showed the usual pair of helpers: `xor eax,eax; mov dx,[esp+4]; in al,dx; ret` beginning at `0x00035750`, and `xor eax,eax; mov dx,[esp+4]; mov al,[esp+8]; out dx,al; ret` beginning at `0x00035770`.

**Confirmed.** The runtime compares the exception address against the RVA, so the values it wants are the opcode bytes and not the helper entry points: `0x00035757` for `in` and `0x0003577b` for `out`. The 1st SE `.gtide` values that stood there before, `0x00038987` and `0x000389ab`, belong to a different build and never matched.

Re-running `candidate-84` with the corrected RVAs took the trace from 275 lines to 1875 and from zero asset opens to 484 across 248 distinct paths, with the same fifteen transforms and no crash beyond the entry-time anti-debug `0xc0000005`. The guest reads its own artwork — `System\WarningMsg\WarningMsg.bmp`, `System\CompanyLogo\logo.str`, `AMUSEWORLD_BG.bmp` and the rest. A wrong candidate cannot invent the real asset paths, so **this candidate's response map is confirmed correct.**

The same `response450` and `tail44c` as 3rd, 4th and 1st SE hold here; four products share one handshake replay. The confirmed material lives only under the Git-ignored `cfg/`, and `re2dj ez2dj1st` reads it with no extra options, recognising all three `hardlock_cfg_material` items and opening the 484 assets.

### Judging the ez2dj5th transform responses

The 176 candidates were judged by the same criteria. 175 of them crashed with `0xc0000005` (136), `0xc0000096` (28) or `0xc000001d` (11), producing 329 or 331 trace lines and 38 descriptor requests. `candidate-135` alone produced 363 lines and 40 descriptor requests, and stopped at `0xc0000096` — the same shape as 1st, where the fault is untrapped port I/O rather than a wrong response. The 5th profile had inherited 4th's helper RVAs, which were never confirmed for this build.

Decoding the code window around the faulting RVA `0x000ca067` gave this build's own helper pair: `0x000ca067` for `in` and `0x000ca09b` for `out`. The inherited 4th values `0x000c3817` and `0x000c384b` never matched this executable.

Re-running `candidate-135` with the corrected RVAs took the trace from 363 lines to 2493 and from zero asset opens to 67, with the same 37 transforms and no crash beyond the entry-time anti-debug `0xc0000005`. The guest reads its own assets — `LOGO.str`, `Title.str`, `2PLAYERInsertCoin.str`, `2PLAYERPressStart.str`. **This candidate's response map is confirmed correct.**

With the confirmed material placed under `cfg/`, a run carrying no Hardlock options at all and only the profile defaults recognises all three `hardlock_cfg_material` items and reaches 37 transforms and 71 asset opens.

The same `response450` and `tail44c` hold here as for 3rd, 4th, 1st SE and 1st: **five products share one handshake replay.**

### The ez2dj5th CHD mount

`re2dj ez2dj5th` fails with `CHD MBR has no in-range FAT32 partition`, and the CHD probe shows why: LBA 0 begins `eb 58 90` — a short jump — followed by the OEM string `MSWIN4.1`.

**Confirmed.** The image is a whole-disk FAT32 volume with no partition table, where LBA 0 is itself the FAT32 boot sector. The current reader treats LBA 0 as an MBR and looks for a FAT32 type in its partition entries, so it does not recognise this layout.

The descriptor observation in this task did not use the CHD: it used the extracted directory at `roms/ez2dj5th/ez2dj` as the source root, running the real 5th executable.
