# ez2dj4th 프로파일별 raw I/O 재사용 작업 로그

관련 설계: [ez2dj4th 프로파일별 raw I/O 재사용 설계](../design/20260903-144-ez2dj4th-profile-io-reuse.md)

## 결과

`ez2dj1stse`의 공용 `LegacyIoPortBus`/`Ez2DjIoBoard`를 `ez2dj4th`의 확인된 raw byte-read 지점에 연결했습니다. 실행 파일별 helper RVA를 `TargetLptdiPolicy`로 이동했고, 4th는 아직 물리 응답 계약이 확인되지 않았으므로 explicit diagnostic opt-in으로만 활성화했습니다.

## 코드 변경

- 1st profile은 기존 read/write RVA `0x00038987`/`0x000389ab`와 제품 기본 활성화를 유지합니다.
- 4th profile은 read RVA `0x000c3817`만 capability로 등록합니다. write RVA는 0이고 제품 기본 활성화는 false입니다.
- debugger trap과 injected runtime이 profile별 RVA를 사용합니다.
- injected runtime export에 read/write RVA를 추가하고 detached 진단 로그에 기록합니다.
- product-loader probe에서 1st 기본 인자, 3rd 비활성, 4th capability/no-default를 확인합니다.
- 설계상 공용 bus의 `0x0103`/`0x0104` center 값과 `0x0105` counter 값은 물리 보드 응답으로 승격하지 않았습니다.

## 검증

### 빌드와 테스트

실행 명령:

```text
cmd /c scripts\build_win32.bat
ctest --test-dir build\windows-x86 -C Debug -R "re2dj_windows_product_loader_probe|re2dj_unit_tests" --output-on-failure
```

결과:

- Windows x86 Debug build: 통과
- `re2dj_unit_tests`: `checks: 1184, failures: 0`
- `re2dj_windows_product_loader_probe`: `profile-defaults=ok unsupported-target=ok`
- 선택 CTest: `2/2 passed`
- 전체 CTest는 기존 `re2dj_windows_vfs_runtime_probe`가 시작 후 대기하여 중단했습니다. 이 probe는 이전 작업에서도 `windowed client size policy failed` 이후 timeout이 관찰된 기존 제한이며, 이번 raw I/O 변경의 테스트 실패로 판정하지 않았습니다.

### 4th attached 진단

실행 명령은 실제 CHD와 사용자 staging root를 사용했습니다.

```text
re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --device-mock-lptdi --device-mock-wts-console-session --slot-writer-trace --trace
```

로그: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004511-065.jsonl`

확인된 이벤트:

- 기존 Hardlock material, 36개 transform, `EZ2DJ.ini` read가 먼저 성공했습니다.
- `0x004c3817` / `0xc0000096` privileged event가 profile trap으로 처리되었습니다.
- 공용 bus가 `0x0103 -> 0x80`, `0x0104 -> 0x80`, `0x0105 -> 0x00`을 반환했습니다.
- 이후 child는 `0x00434137` / `0xc0000005` access violation까지 진행했습니다.
- 즉, 이전 4th raw-I/O boundary는 제거되었고 다음 AV가 새 관찰 경계가 되었습니다.

### 4th injected runtime 진단

실행 명령:

```text
re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd E:\MYWORK\Projects\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --hle-io-ports --run-detached --device-mock-lptdi --device-mock-wts-console-session
```

로그: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004600-318.jsonl`

`io_port_runtime` 이벤트가 `in_rva=0x000c3817`, `out_rva=0x00000000`으로 기록되었고 child exit code는 `0xc0000005`였습니다. attached 경로와 동일한 다음 경계를 확인하여 injected runtime의 profile RVA 전달도 검증했습니다.

### 일반 제품 기본값 회귀

실행 로그 `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004925-120.jsonl`의 `launch` 이벤트에서 `hle_io_ports:false`, `run_detached:true`가 확인되었고 child는 기존처럼 `0xc0000096`으로 종료했습니다. 따라서 4th raw I/O는 일반 `re2dj.exe ez2dj4th`에 암묵적으로 추가되지 않았습니다.

## 판단과 다음 경계

공용 1st IO 구현을 4th에 재사용하는 구조와 주소 연결은 검증되었습니다. 다만 `0x80`은 현재 `Ez2DjIoBoard`의 turntable center 값일 뿐 4th 물리 보드의 응답으로 확인되지 않았습니다. 다음 작업은 `0x00434137` AV의 원인과 호출 맥락을 분석하는 것이며, 4th `OUT` RVA나 제품 기본 IO 활성화는 별도 증거가 생길 때까지 추가하지 않습니다.

---

# ez2dj4th Profile-Specific Raw I/O Reuse Work Log

Related design: [Profile-Specific Raw I/O Reuse for ez2dj4th](../design/20260903-144-ez2dj4th-profile-io-reuse.md)

## Result

The shared `LegacyIoPortBus`/`Ez2DjIoBoard` used by `ez2dj1stse` is now connected to the confirmed raw byte-read site in `ez2dj4th`. Executable-specific helper RVAs live in `TargetLptdiPolicy`; 4th remains explicit diagnostic opt-in because its physical response contract is not confirmed.

## Code changes

- 1st keeps read/write RVAs `0x00038987`/`0x000389ab` and product-default activation.
- 4th registers only read RVA `0x000c3817`; its write RVA is zero and product-default activation is false.
- The debugger trap and injected runtime use profile-specific RVAs.
- The injected runtime exports read/write RVAs and records them in the detached diagnostic event.
- The product-loader probe verifies 1st defaults, 3rd disablement, and 4th capability/no-default.
- The shared bus's `0x0103`/`0x0104` center values and `0x0105` counter value are not promoted as physical-board responses.

## Verification

### Build and tests

Commands:

```text
cmd /c scripts\build_win32.bat
ctest --test-dir build\windows-x86 -C Debug -R "re2dj_windows_product_loader_probe|re2dj_unit_tests" --output-on-failure
```

Results:

- Windows x86 Debug build: passed
- `re2dj_unit_tests`: `checks: 1184, failures: 0`
- `re2dj_windows_product_loader_probe`: `profile-defaults=ok unsupported-target=ok`
- Selected CTest: `2/2 passed`
- The full CTest run was interrupted because the existing `re2dj_windows_vfs_runtime_probe` remained waiting after startup. The same probe previously timed out after `windowed client size policy failed`; this was treated as a pre-existing limitation, not a raw-I/O test failure.

### 4th attached diagnostic

The real CHD and user staging root were used. Log: `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004511-065.jsonl`.

Observed:

- Existing Hardlock material, all 36 transforms, and the `EZ2DJ.ini` read succeeded first.
- The `0x004c3817` / `0xc0000096` privileged event was handled by the profile trap.
- The shared bus returned `0x0103 -> 0x80`, `0x0104 -> 0x80`, and `0x0105 -> 0x00`.
- The child then advanced to an access violation at `0x00434137` / `0xc0000005`.
- The previous 4th raw-I/O boundary is therefore removed and the AV is the next observed boundary.

### 4th injected-runtime diagnostic

The explicit `--hle-io-ports --run-detached` log `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004600-318.jsonl` records `io_port_runtime` with `in_rva=0x000c3817` and `out_rva=0x00000000`; the child exits with `0xc0000005`. This verifies profile-RVA delivery in the injected runtime and the same next boundary as the attached path.

### Normal product-default regression

The launch event in `logs/windows_x86_launcher_probe/ez2dj4th/20260903-004925-120.jsonl` has `hle_io_ports:false` and `run_detached:true`; the child exits with the previous `0xc0000096`. 4th raw I/O is therefore not implicitly added to normal `re2dj.exe ez2dj4th` execution.

## Judgement and next boundary

The shared 1st I/O implementation and profile-specific address connection are verified. The `0x80` value is currently only the `Ez2DjIoBoard` turntable-center value, not a confirmed 4th physical-board response. The next task should analyze the `0x00434137` AV and its call context; no 4th `OUT` RVA or product-default raw-I/O activation should be added without new evidence.
