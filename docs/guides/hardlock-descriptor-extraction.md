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
