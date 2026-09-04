# Task 179: Direct3D7 정점 버퍼 facade 구현

## 작업 목표

`IDirect3D7::CreateVertexBuffer`가 실제 `IDirect3DVertexBuffer7` 객체를 돌려주게 하여, 게스트가 null을 역참조하며 멈추던 지점을 넘어가게 합니다.

## 선행 문서

- [Task 179 설계](../design/20260904-179-direct3d7-vertex-buffer-facade.md)
- [Task 178 작업 로그](../work-logs/20260904-178-ez2dj4th-panel-null-object.md)
- [Task 177 작업 로그](../work-logs/20260904-177-vfs-guest-working-directory.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)

## 구현 범위

1. **전용 파일 추가.** `src/platform/windows/direct3d7_vertex_buffer_facade.{h,cpp}`에 `IDirect3DVertexBuffer7` facade와 `CreateDirect3DVertexBuffer7Facade`를 둡니다. 저장소는 `re2dj::graphics::LegacyVertexBuffer`를 씁니다.
2. **연결.** `D3d7CreateVertexBuffer`가 서술자를 검사하고 새 facade를 만들어 돌려주게 합니다. 성공과 함께 null을 돌려주는 조합을 없앱니다.
3. **빌드 배선.** 새 소스를 `CMakeLists.txt`의 해당 대상에 추가합니다.
4. **진단 실행과 판정.** 진입 추적을 실행해 `Lock`이 성공하는지, `RVA 0x0001290e`의 접근 위반이 사라지는지, 다음 중단 지점이 어디인지 확인합니다.
5. **문서 갱신.** 작업 로그와 `docs/analysis/ez2dj4th-hardlock-runtime.md`를 갱신하고, `ARCHITECTURE.md`에 새 파일을 반영합니다.

## 비범위

- 정점 데이터를 DX7 그리기 경로에 연결.
- `ProcessVertices` 변환 구현.
- DX6 경로(`direct3d3_com_facade`) 변경.

## 최소 검증

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
```

실제 CHD가 있으면 진입 추적을 실행합니다.

```powershell
$common = '--hdd', "$env:TEMP\re2dj\chd\ez2dj4th", '--target', 'ez2dj4th',
  '--chd', 'roms\ez2dj4th\4thTrax.chd',
  '--target-executable', 'EZ2DJ/EZ2DJ.EXE',
  '--hle-vfs', '--hle-dynamic-vfs', '--hle-d3d3', '--hle-io-ports',
  '--device-mock-lptdi', '--device-mock-lptdi-path-prefix', '\\.\FEnteDev',
  '--device-mock-wts-console-session', '--diagnostic-idle-timeout', '60000'
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe @common --null-context-entry-trace
```

## 자기 검증 기준

- `.ddraw.log`에 `CreateVertexBuffer`가 stride와 바이트 수를 포함해 성공으로 남고, 뒤이어 `IDirect3DVertexBuffer7::Lock`이 남아야 합니다. 관측된 요청이라면 stride 32, 3,872 바이트입니다.
- `RVA 0x0001290e`의 접근 위반이 사라져야 합니다.
- 로그에는 크기와 형식 값만 남기고 정점 내용은 남기지 않습니다.

---

# Task 179: Implementing the Direct3D7 Vertex Buffer Facade

## Goal

Make `IDirect3D7::CreateVertexBuffer` return a real `IDirect3DVertexBuffer7` so the guest gets past the null dereference.

## Scope

1. Add `src/platform/windows/direct3d7_vertex_buffer_facade.{h,cpp}` holding the interface and its creator, backed by `re2dj::graphics::LegacyVertexBuffer`.
2. Have `D3d7CreateVertexBuffer` validate the descriptor and return the new facade, removing the success-with-null combination.
3. Add the source to `CMakeLists.txt`.
4. Run the entry trace and check the lock, the cleared access violation, and the next stopping point.
5. Update the work log, the analysis topic, and `ARCHITECTURE.md`.

## Out of Scope

Wiring vertex data into the DX7 draw path, implementing `ProcessVertices`, and changing the DX6 path.

## Minimum Verification

Build, unit tests, and the product loader probe, then the entry trace against the real CHD.

## Self-Check

The graphics trace must show `CreateVertexBuffer` succeeding with its stride and byte count — 32 and 3,872 for the observed request — followed by a `Lock`, and the access violation at `RVA 0x0001290e` must be gone. Logs record sizes and formats only, never vertex contents.
