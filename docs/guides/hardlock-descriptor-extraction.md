# Hardlock descriptor ID 추출 / Hardlock descriptor ID extraction

근거: [descriptor 진단 설계](../design/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md), [descriptor 진단 작업 로그](../work-logs/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md)

*Basis: the [descriptor diagnostic design](../design/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md) and [descriptor diagnostic work log](../work-logs/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md).*

새 프로파일의 원본 실행이 Hardlock descriptor를 실제로 요청하는지 확인하고 `module_address`, `id_ref`, `id_verify`를 추출할 때 launcher probe의 `--hardlock-descriptor-dump` 옵션을 사용합니다. 이 옵션은 첫 번째 유효한 256바이트 descriptor만 추출하며, 출력 파일의 같은 프로파일 section은 갱신하고 다른 section은 보존합니다.

*Use the launcher's `--hardlock-descriptor-dump` option when checking whether a new profile's original executable requests a Hardlock descriptor and extracting `module_address`, `id_ref`, and `id_verify`. It extracts the first valid 256-byte descriptor, updates only the matching profile section, and preserves other sections in the output file.*

## 실행 / Run

다음 명령은 한 줄로 실행할 수 있습니다. `<staged CHD root>`와 `<internal executable>`은 해당 프로파일의 CHD materialization 결과와 실제 실행 대상에 맞게 바꿉니다.

*The following command is intentionally one line. Replace `<staged CHD root>` and `<internal executable>` with the profile's CHD materialization root and actual executable target.*

```powershell
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd "<staged CHD root>" --chd ".\roms\<profile>\<image>.chd" --target <profile> --target-executable "<internal executable>" --software-breakpoint --hle-vfs --device-mock-lptdi --device-mock-lptdi-path-prefix "\\.\FEnteDev" --hardlock-descriptor-dump ".\cfg\hardlock-id.ini" --diagnostic-idle-timeout 8000
```

`--hardlock-descriptor-dump`는 descriptor 관찰에 필요한 VFS, synthetic device, runtime injection, software breakpoint를 자동으로 켭니다. 원본이 descriptor 단계까지 가려면 해당 게임에 필요한 display/audio 옵션이나 기존 Hardlock handshake 응답·transform map 옵션을 같은 명령에 추가해야 할 수 있습니다. 출력은 다음 형태의 로컬 파일에 남습니다.

*`--hardlock-descriptor-dump` automatically enables the VFS, synthetic device, runtime injection, and software breakpoint needed for descriptor observation. Add the display/audio options or the existing Hardlock handshake-response and transform-map options required by the specific game if it must reach the descriptor stage. The local output has this shape:*

```ini
[<profile>]
module_address=0x....
id_ref=<16 hex digits>
id_verify=<16 hex digits>
```

`cfg/`는 전체가 Git에서 무시됩니다. 일반 VFS trace에는 원문 ID가 기록되지 않고 `id_ref_hash`, `id_verify_hash`, non-zero 여부만 기록됩니다. raw 값은 사용자가 명시한 로컬 dump 파일에만 남으므로 이 파일을 저장소에 추가하지 않습니다.

*The entire `cfg/` directory is ignored by Git. Normal VFS traces do not contain raw IDs; they retain only `id_ref_hash`, `id_verify_hash`, and non-zero flags. Raw values remain only in the explicitly selected local dump file, which must not be added to the repository.*

## 확인 / Confirm

- launcher JSONL에 `hardlock_descriptor_dump` 이벤트가 있어야 합니다.
- `.vfs.log`에 `header_valid=1`, `module_address`, `id_ref_hash`, `id_verify_hash`가 있는 descriptor 줄이 있어야 합니다.
- 출력 파일의 section이 선택한 프로파일 이름인지 확인합니다.
- 여러 프로파일을 같은 `cfg/hardlock-id.ini`에 추출하면 각 프로파일 section이 유지되는지 확인합니다.

*Confirm that the launcher JSONL contains `hardlock_descriptor_dump`, the `.vfs.log` contains a descriptor line with `header_valid=1`, `module_address`, `id_ref_hash`, and `id_verify_hash`, and the output section matches the selected profile. When extracting multiple profiles into the same `cfg/hardlock-id.ini`, verify that each profile section remains present.*

## `.protect` 계열에서 추가로 필요한 것 / What the `.protect` family additionally needs

위 명령만으로는 `.protect` 계열(1st, 1st SE, 3rd, 4th, 5th, ez2d2m)이 descriptor에 도달하지 못합니다. 세 가지를 더 붙입니다.

*The command above does not reach a descriptor for the `.protect` family — 1st, 1st SE, 3rd, 4th, 5th and ez2d2m. Add three things.*

| 추가 옵션 | 없을 때의 증상 |
| --- | --- |
| `--hle-dynamic-vfs` | `CreateFileA`가 `route=win32`로 해석되고 Hardlock 요청 0건 |
| `--run-detached` | 첫 VFS 파일 개방이 handoff로 처리되어 원본이 즉시 종료 |
| `--hardlock-device`와 `--device-mock-hardlock-450-response`·`--device-mock-hardlock-44c-tail` | initialize 뒤 descriptor를 요청하지 않고 종료 |

*Without `--hle-dynamic-vfs`, `CreateFileA` resolves `route=win32` and no Hardlock request is made. Without `--run-detached`, the launcher treats the first VFS file open as the handoff and terminates the original immediately. Without `--hardlock-device` and the replay values, the guest stops after initialize without asking for a descriptor.*

`cfg/hardlock.ini`의 프로파일 section은 같은 이름의 transform map이 있을 때만 적용됩니다. 새 프로파일에는 아직 map이 없으므로, 재생값은 명시적 옵션으로 넘깁니다. 명시적 옵션이 파일보다 우선합니다.

*A `cfg/hardlock.ini` profile section is applied only alongside a transform map of the same name. A new profile has none yet, so pass the replay values as explicit options; an explicit option outranks the file.*

## 셸 인용 주의 / A shell-quoting trap

`--device-mock-lptdi-path-prefix`의 값은 백슬래시로 시작합니다. **Git Bash에서는 이 인자를 넘기지 마십시오.** 인용을 어떻게 하든 백슬래시 하나가 사라져 `\.\FEnteDev`가 전달되고, 장치 경로가 일치하지 않아 open이 `error=123`으로 실패합니다. 증상이 회귀처럼 보이지만 원인은 인용입니다.

이 옵션은 생략하는 것이 정답입니다. 생략하면 프로파일의 `device_mock_path_prefix`가 그대로 쓰입니다. 값을 직접 지정해야 한다면 PowerShell에서 실행합니다.

*The `--device-mock-lptdi-path-prefix` value begins with backslashes. **Do not pass this argument from Git Bash**: whatever the quoting, one backslash is lost, `\.\FEnteDev` arrives, the device path does not match, and the open fails with `error=123`. It looks like a regression and is not one.*

*The right answer is to omit the option, which uses the profile's own `device_mock_path_prefix`. Run from PowerShell if a value really must be given explicitly.*

확인 방법: `.vfs.log`의 `device-open` 줄이 `success=1:error=0`이어야 합니다. `error=123`이면 prefix가 게스트가 여는 이름과 다릅니다.

*To check: the `device-open` line in `.vfs.log` must read `success=1:error=0`. An `error=123` means the prefix does not match the name the guest opens.*
