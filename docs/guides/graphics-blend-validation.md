# 그래픽 블렌드 검증
# Graphics Blend Validation

근거: [설계 198](../design/20260905-198-destination-color-mask-blending.md), [작업 로그 198](../work-logs/20260905-198-destination-color-mask-blending.md).

*References: [Design 198](../design/20260905-198-destination-color-mask-blending.md) and [work log 198](../work-logs/20260905-198-destination-color-mask-blending.md).*

## 자산 없는 GPU 검사 / Asset-Free GPU Check

Win32 Debug build 후 저장소 루트의 PowerShell에서 실행합니다. OpenGL driver와 사용 가능한 desktop이 필요하며 작은 테스트 창을 만들고 즉시 숨긴 뒤 종료합니다. 게임 자산은 읽지 않습니다.

*After the Windows Debug build, run from repository-root PowerShell. An OpenGL driver and usable desktop are required. The probe creates and immediately hides a small test window, then exits without reading game assets.*

```powershell
& '.\build\windows-x86\bin\Debug\re2dj_opengl_blend_probe.exe'
```

기대 결과는 `pixel checks: 9, failures: 0`입니다. 이것은 공용 factor 변환과 실제 RGB565 OpenGL 합성 검증이며 Music Select 화면 검증을 대신하지 않습니다.

*Expect `pixel checks: 9, failures: 0`. This validates common factor conversion and actual RGB565 OpenGL composition; it does not replace Music Select verification.*

## 게임 화면 비교 / Game-Screen Comparison

기존에 사용자가 확인한 HDD/CHD·I/O 설정의 launcher 명령으로 실행하고 코인과 시작 입력을 통해 Music Select에 진입합니다. 상단 헤더의 검정 배경이 작은 디스크를 가리는지, 중앙 artwork 뒤 배경 광선이 과도하게 보이는지 확인한 뒤 게임을 종료합니다. 종료 출력의 `diagnostic_log`와 같은 이름의 `.ddraw.log`를 분석합니다.

*Launch with the previously verified user HDD/CHD and I/O settings, then enter Music Select through coin/start input. Check whether the black header occludes the small disc and whether background rays remain excessively visible through the center artwork. Close the game and inspect the `.ddraw.log` alongside the reported `diagnostic_log`.*

최신 실행에서 목적지 색상 draw와 실패를 추리는 한 줄 명령입니다. 이전 실행을 분석할 때는 `$blendLog`에 해당 로그의 경로를 직접 지정합니다.

*This one-line command extracts destination-color draws and failures from the latest run. For an older run, set `$blendLog` to that log explicitly.*

```powershell
$blendLog=(Get-ChildItem '.\logs\windows_x86_launcher_probe\ez2dj4th\*.ddraw.log' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName; rg 'DrawPrimitive:.*(srcblend=9:|srcblend=10:|result=0x800|result=0x887)' $blendLog
```

`reason=success`에 frame과 bounds가 있으면 해당 draw가 backend까지 도달한 것입니다. 성공 여부와 화면 개선을 함께 판단하며, 다른 texture의 첫 실패가 남으면 별도로 확인합니다.

*A `reason=success` record with frame and bounds confirms backend submission. Judge it together with the screen change and inspect any first failure from another texture separately.*
