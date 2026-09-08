# 작업 로그: ez2dj1stse Hardlock 후보 판별

## 한국어

### 관련 문서

- 작업 지시: [ez2dj1stse Hardlock 후보 판별](../work-orders/20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)
- 절차: [Hardlock seed 복구 워크스루](../guides/hardlock-seed-recovery-walkthrough.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [CHD 프로파일 실행 정책 정정](20260908-223-ez2dj1stse-chd-profile-correction.md)

### 선행 확인

재부팅으로 이전 작업의 잔여 게스트 프로세스가 사라져, [작업 223](20260908-223-ez2dj1stse-chd-profile-correction.md)에서 남겨 두었던 end-to-end 검증을 먼저 끝냈습니다. `re2dj ez2dj1stse`는 2.9초 만에 preparation 전 항목 true로 게스트를 실행했고, `\\.\FEnteDev`를 5회 열고 initialize·handshake까지 도달한 뒤 exit code `8`로 종료했습니다. 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-081443-290`입니다.

### 수행 내용

1. map 하나(`candidate-0`)를 주입해 15개 항목이 17개 transform 요청을 전부 덮는지 확인했습니다. `mapped=1:unmapped=0` 17건으로 완전 피복입니다.
2. 후보 129개를 전부 주입 실행하는 스크립트를 만들어 실행했습니다. 실행마다 종료 코드, `.vfs.log` 줄 수, IOCTL 종류별 횟수, `\\.\FEnteDev` 개방 횟수, 게스트 설정 파일 개방 여부를 CSV로 기록하고, 각 실행 뒤 잔여 게스트 프로세스를 회수했습니다.
3. 갈린 후보를 재실행해 재현성을 확인했습니다.
4. 확정된 map과 재생값을 3rd·4th와 같은 형태로 배치했습니다.
5. `re2dj ez2dj1stse`가 별도 옵션 없이 그 자료를 소비하는지 확인했습니다.

주입 명령은 정정된 프로파일 기본값과 같은 조합에 `--hardlock-device`, `--device-mock-hardlock-450-response`, `--device-mock-hardlock-44c-tail`, `--hardlock-transform-map`을 더한 것입니다. 실행당 약 3.3초, 전체 129개에 약 7분이 걸렸습니다.

### 판별 결과 — 확인됨

129개가 정확히 하나로 갈렸습니다.

| 관찰 | 후보 128개 | 후보 1개 |
| --- | --- | --- |
| 종료 코드 | `0xc0000005` 85, `0xc0000096` 28, `0xc000001d` 11, `0x80000003` 2, `0xc000008c` 1, `0x80000004` 1 | `0x00000000` |
| `.vfs.log` 줄 수 | 252 (128개 전부 동일) | 316 |
| descriptor 요청 | 18 | 19 |
| 게스트 자산 개방 | 0 | 11 |

갈린 후보의 실행은 transform loop 이후 다음을 수행합니다.

- `System\Common\coin0.wav` 읽기
- `System\WarningMsg\WarningMsg.bmp` 열기
- `System\CompanyLogo\logo.str` 52,892 바이트 읽기
- `AMUSEWORLD_BG.bmp`, `AMUSEWORLD_BG-UP.bmp`, `AMUSEWORLD_BG-Overlap.bmp`, `LIGHT.bmp`, `AMUSEWORLD_OBJ-Shadow.bmp`, `AMUSEWORLD_OBJ256.bmp`, `AMUSEWORLD_OBJ-R-S.bmp`, `AMUSEWORLD_OBJ-R.bmp` 열기
- 작업 디렉터리를 `System\Title`로 이동하고 `*.*` 열거
- `function=0x0001` descriptor를 한 번 더 요청
- `ExitProcess(0)`

Hardlock 요청 합계는 `total=40:initialize=2:handshake=2:descriptor=19:transform=17:other=0:rejected=0`입니다. 재실행에서 종료 코드 `0x00000000`, 316줄, 자산 개방 11건이 동일하게 재현되었습니다.

오답 후보가 우연히 원본 자산의 실제 경로를 만들어 낼 수 없으므로, 이 후보의 응답 map이 옳다는 것은 확정입니다. 워크스루 Stage 7이 3rd·4th에서 쓴 기준(정상 종료, 늘어난 trace, 게스트 자신의 파일 읽기)이 1st SE에서도 그대로 성립했습니다.

`response450=0100fafa0010`과 `tail44c=0001`은 3rd·4th에서 가져온 값인데 1st SE에서도 성립합니다. 세 제품이 같은 handshake 재생값을 씁니다.

### 배치한 자료

저장소가 아니라 Git이 무시하는 `cfg/` 아래에만 둡니다.

| 경로 | 내용 |
| --- | --- |
| `cfg/hardlock-ez2dj1stse.map` | 확정된 challenge-response map 15줄 |
| `cfg/hardlock.ini` `[ez2dj1stse]` | `response450`, `tail44c` |
| `cfg/ez2dj1stse/resolved-seeds.txt` | 확정 seed 한 줄과 출처 주석 |
| `cfg/ez2dj1stse/resolved.mapcfg` | module address와 확정 seed |
| `cfg/ez2dj1stse/maps/resolved.map` | 확정 map 사본 |

seed 값과 응답 바이트는 이 문서에도 저장소 어디에도 기록하지 않습니다.

### 검증

`re2dj ez2dj1stse`를 옵션 없이 실행해 다음을 확인했습니다. 로그는 `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-082352-165`입니다.

- `hardlock_cfg_material`: `response450=true`, `tail44c=true`, `map=true`
- `hardlock_device`: `enabled=true`
- `hardlock_transform_map`: `entries=15`
- transform 17건 전부 `mapped=1:unmapped=0`
- `runtime_detached_exit`: `0x00000000`, `.vfs.log` 316줄

직접 주입 실행과 값이 완전히 같습니다. 즉 제품 경로가 자료를 정확히 소비합니다.

### 새로 드러난 문제

복호화된 게스트가 작업 디렉터리 설정에 두 번 실패합니다.

- `SetCurrentDirectory("c:\ez2dj")` → `resolved=ez2dj:success=0`
- `System\Title`로 이동한 뒤 `SetCurrentDirectory("Songs")` → `resolved=System/Title/Songs:success=0`

두 번째는 상대 경로를 현재 디렉터리에 이어 붙여 해석한 결과입니다. 게스트가 게임 root 기준을 기대했다면 잘못된 해석입니다. 이것이 로고 단계에서 멈추는 직접 원인인지는 확인되지 않았습니다.

### 남은 과제

- VFS 작업 디렉터리 해석 수정 후 도달 경계가 바뀌는지 확인
- packed import 제약 아래에서 graphics HLE를 연결할 경로 조사 (`hle_d3d3`가 꺼져 있어 DirectDraw가 HLE를 거치지 않음)
- legacy I/O helper RVA가 이 빌드에서도 유효한지 실행으로 확인
- 1stse가 실제로 호출하는 Hardlock function이 기본값 `0x000e`인지 trace로 확인

## English

### Related documents

- Work order: [ez2dj1stse Hardlock candidate judgement](../work-orders/20260908-224-ez2dj1stse-hardlock-candidate-judgement.md)
- Procedure: [Hardlock seed recovery walkthrough](../guides/hardlock-seed-recovery-walkthrough.md)
- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [CHD profile execution policy correction](20260908-223-ez2dj1stse-chd-profile-correction.md)

### Preliminary confirmation

A reboot cleared the leftover guest processes from the previous task, so the end-to-end verification deferred in [task 223](20260908-223-ez2dj1stse-chd-profile-correction.md) was completed first. `re2dj ez2dj1stse` executed the guest in 2.9 seconds with every preparation flag true, opened `\\.\FEnteDev` five times, reached initialize and handshake, and exited with code `8`. The log is `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-081443-290`.

### What was done

One map (`candidate-0`) was injected first to confirm coverage: its fifteen entries answered all seventeen transform requests with `mapped=1:unmapped=0`. A script then injected all 129 candidates, recording per run the exit code, `.vfs.log` line count, per-IOCTL counts, `\\.\FEnteDev` open count, and whether the guest configuration file was opened, and reclaiming any leftover guest process after each run. The candidate that separated was rerun to confirm reproducibility, its map and replay values were installed in the same shape 3rd and 4th use, and `re2dj ez2dj1stse` was checked to consume them with no extra options.

The injection command is the corrected profile's own option combination plus `--hardlock-device`, `--device-mock-hardlock-450-response`, `--device-mock-hardlock-44c-tail`, and `--hardlock-transform-map`. Each run took about 3.3 seconds and all 129 took about seven minutes.

### Judgement result — confirmed

The 129 candidates separated into exactly one.

| Observation | 128 candidates | 1 candidate |
| --- | --- | --- |
| exit code | `0xc0000005` ×85, `0xc0000096` ×28, `0xc000001d` ×11, `0x80000003` ×2, `0xc000008c` ×1, `0x80000004` ×1 | `0x00000000` |
| `.vfs.log` lines | 252, identical across all 128 | 316 |
| descriptor requests | 18 | 19 |
| guest asset opens | 0 | 11 |

After the transform loop, the separating candidate's run reads `System\Common\coin0.wav`, opens `System\WarningMsg\WarningMsg.bmp`, reads 52,892 bytes of `System\CompanyLogo\logo.str`, opens the logo bitmaps `AMUSEWORLD_BG.bmp`, `AMUSEWORLD_BG-UP.bmp`, `AMUSEWORLD_BG-Overlap.bmp`, `LIGHT.bmp`, `AMUSEWORLD_OBJ-Shadow.bmp`, `AMUSEWORLD_OBJ256.bmp`, `AMUSEWORLD_OBJ-R-S.bmp`, and `AMUSEWORLD_OBJ-R.bmp`, moves its working directory to `System\Title` and enumerates `*.*`, issues one more `function=0x0001` descriptor, and calls `ExitProcess(0)`. Its Hardlock totals are `total=40:initialize=2:handshake=2:descriptor=19:transform=17:other=0:rejected=0`, and a rerun reproduced the same exit code, the same 316 lines, and the same eleven asset opens.

A wrong candidate cannot invent the original's real asset paths by chance, so this candidate's response map is established as correct. The Stage 7 criteria that worked for 3rd and 4th — a clean exit, a longer trace, and the guest reading its own files — held for 1st SE as well.

`response450=0100fafa0010` and `tail44c=0001`, carried over from 3rd and 4th, also hold for 1st SE, so all three products share the same handshake replay values.

### Installed material

Everything lives under the Git-ignored `cfg/`, never the repository: `cfg/hardlock-ez2dj1stse.map` holds the confirmed fifteen-line challenge-response map, the `[ez2dj1stse]` section of `cfg/hardlock.ini` holds `response450` and `tail44c`, and `cfg/ez2dj1stse/` holds `resolved-seeds.txt`, `resolved.mapcfg`, and `maps/resolved.map`. Seed values and response bytes are recorded neither here nor anywhere else in the repository.

### Verification

Running `re2dj ez2dj1stse` with no options confirmed `hardlock_cfg_material` reporting `response450=true`, `tail44c=true`, and `map=true`; `hardlock_device` enabled; `hardlock_transform_map` with fifteen entries; all seventeen transform requests answered `mapped=1:unmapped=0`; and `runtime_detached_exit` of `0x00000000` with a 316-line `.vfs.log`. The log is `logs/windows_x86_launcher_probe/ez2dj1stse/20260908-082352-165`. Those values match the direct injection run exactly, so the product path consumes the material correctly.

### Newly surfaced problem

The decrypted guest fails two working-directory changes: `SetCurrentDirectory("c:\ez2dj")` reports `resolved=ez2dj:success=0`, and after moving to `System\Title` a `SetCurrentDirectory("Songs")` reports `resolved=System/Title/Songs:success=0`. The second resolves the relative path against the current directory, which is wrong if the guest expected the game root. Whether this is the direct reason the run stops at the logo stage is unconfirmed.

### Remaining work

- Confirm whether the reached boundary changes after fixing the VFS working-directory resolution.
- Investigate connecting the graphics HLE under the packed-import constraint, since `hle_d3d3` is off and DirectDraw does not pass through the HLE.
- Confirm by execution whether the legacy-I/O helper RVAs hold for this build.
- Confirm by trace whether 1st SE really issues the default Hardlock function `0x000e`.
