# 20260905-183 호스트 표시 모드 불변 정책 결과
# 20260905-183 Host Display Mode Invariance — Results

## 1. 개요 (Overview)

Task 182의 1st SE 회귀 실행이 호스트 데스크탑 해상도를 실제로 바꾼 문제를 고쳤다.

**결론: 원인은 제품 실행 경로에 표시 모드 경계가 아예 설치되지 않은 것이었다. 경계를 모든 주입 실행에 무조건 설치하고 요청 모양과 무관하게 전부 흡수하도록 바꿨다. 실행 전후 데스크탑이 3,840×2,160×32 @60Hz로 동일하고, 그리기 동작은 변경 전과 완전히 같다.**

The 1st SE regression run of Task 182 changed the host desktop resolution because the display boundary was never installed on the product path at all. The boundary is now installed on every injected run and absorbs every request regardless of its shape; the desktop is identical before and after a run, and the drawing behavior is unchanged.

---

## 2. 원인 (Root Cause)

세 갈래로 샜다.

| 지점 | 상태 |
| --- | --- |
| 제품 실행 경로 | `TargetRunDefaults`에 표시 항목이 없고 `BuildOriginalProcessArguments`가 `--hle-display-mode`를 넘기지 않아, 제품 실행에서는 훅이 설치되지 않았다. **이것이 사용자가 겪은 경로다** |
| 훅의 범위 | device name null, `CDS_UPDATEREGISTRY`, 정확히 640×480×16인 요청만 흡수하고 나머지는 실제 USER32로 넘겼다 |
| 동적 해석 | `Re2djHleGetProcAddress`에 표시 이름이 없어, import를 `GetProcAddress`로 푸는 게스트는 훅을 지나쳤다 |

DirectDraw의 `SetDisplayMode`/`SetCooperativeLevel`과 `ApplyRe2djWindowMode`의 전체화면 경로는 이미 호스트를 건드리지 않았다. 새는 곳은 Win32 `ChangeDisplaySettings` 계열 하나뿐이었다.

---

## 3. 변경 내용 (Changes Implemented)

`src/platform/windows/display_mode_boundary.{h,cpp}` (신규)

1. **경계 분리.** 표시 모드 경계를 전용 파일로 옮겼다. `injected_runtime.cpp`에는 include와 동적 해석 연결만 남는다.
2. **전면 흡수.** `Re2djHleChangeDisplaySettingsExA`가 device name, flags, `DEVMODE` 무엇이 오든 호스트를 부르지 않고 `DISP_CHANGE_SUCCESSFUL`을 돌려준다. `CDS_TEST`와 null `DEVMODE`(기본값 복원)도 같다.
3. **비확장 진입점.** `Re2djHleChangeDisplaySettingsA`를 같은 정책으로 추가했다.
4. **요청 기록.** 흡수한 요청을 최대 16줄까지 `re2dj:hle:display-mode:absorbed`로 남긴다. entry, device, flags, `dmFields`, 해상도·색 깊이·주사율을 적는다.

`src/platform/windows/injected_runtime.cpp`

5. **동적 해석 연결.** 두 이름을 `Re2djHleGetProcAddress`가 이 경계로 돌려준다. VFS 동적 resolver 플래그 **밖**에 두었다. 표시 정책은 VFS 정책과 무관하다.
6. 기존의 좁은 구현과 그 메시지 상수를 제거했다.

`src/tools/windows_x86_launcher_probe/main.cpp`

7. **무조건 설치.** 런타임이 주입된 모든 실행에서 두 이름의 USER32 IAT 슬롯을 우회시킨다. `--hle-display-mode` 여부와 무관하다. 슬롯이 없으면 건너뛴다.
8. `--hle-display-mode`는 런타임 주입 요청과 첫 기록 대기 용도로만 남기고, 기대 메시지를 새 접두사로 갱신했다.

`src/tools/windows_vfs_runtime_probe/main.cpp`

9. **검사 정정.** "모양이 다른 요청은 호스트로 넘어간다"를 검사하던 항목을 "모양이 다른 요청도 흡수된다"로 바꿨다. 비확장 진입점과 null `DEVMODE` 검사, 그리고 경계 전후로 `EnumDisplaySettings(ENUM_CURRENT_SETTINGS)`를 비교하는 불변식 검사를 추가했다.

---

## 4. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 1st SE 제품 실행: 변경 전 `20260905-005825-782`, 변경 후 `20260905-012007-893`, `20260905-012235-527`.

### 4.1 데스크탑 해상도가 바뀌지 않는다 (확인됨)

실행 전후, 그리고 실행 중 14회 표본 모두 동일했다.

```
BEFORE: 3840x2160x32 @60Hz
t=0..13 desktop=3840x2160
AFTER : 3840x2160x32 @60Hz
```

- **확인됨 — 요청이 흡수된다.** 변경 후 실행 로그에 다음 한 줄이 남는다. `flags=0x1`은 `CDS_UPDATEREGISTRY`, `fields=0x1c0000`은 `DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT`다.

```
re2dj:hle:display-mode:absorbed:entry=ChangeDisplaySettingsExA:device=default:
  flags=0x00000001:fields=0x001c0000:640x480x16:refresh=0
```

- **확인됨 — 변경 전에는 그 줄이 없다.** `20260905-005825-782`에 흡수 기록이 하나도 없다. 경계가 설치되지 않았다는 직접 증거다.
- **확인됨 — 이 요청은 분석 문서가 기록한 그 요청이다.** `ez2dj-exe-structures.md`의 `RVA 0x00437cba` 640×480×16 `CDS_UPDATEREGISTRY`와 일치한다.

### 4.2 그리기 동작이 바뀌지 않았다 (확인됨)

세 실행의 그래픽 추적을 항목별로 셌다.

| 항목 | 변경 전 `-005825-782` | 변경 후 `-012007-893` | 변경 후 `-012235-527` |
| - | - | - | - |
| `DrawPrimitive` | 25 | 25 | 25 |
| `Flip` | 8 | 8 | 8 |
| `Blt` | 8 | 8 | 8 |
| `CreateSurface` | 10 | 10 | 10 |
| `GetDC` / `ReleaseDC` | 9 / 9 | 9 / 9 | 9 / 9 |
| `LateDraw` | 2,599 | 2,599 | 2,599 |
| 실패 기록 | 0 | 0 | 0 |

- **확인됨 — 완전히 동일하다.** 유일한 차이는 새로 추가된 `display-mode` 한 줄이다. 흡수가 게스트를 `PostQuitMessage` 분기로 보내지 않는다.

### 4.3 검증하지 못한 것 (미확정)

- **미확정 — `re2dj_windows_vfs_runtime_probe.exe`의 새 검사.** 이 probe는 표시 검사보다 앞선 `unmapped script path was not rejected`에서 실패해 멈추므로, 이번에 추가한 표시 불변식 검사에 도달하지 못한다. **이 실패는 이번 변경과 무관한 기존 실패다.** 변경을 stash하고 기준 커밋 `bbe5930`에서 다시 빌드해 같은 실패를 재현했다. VFS의 rooted guest path 처리 문제이며 이번 작업 범위 밖이다.
- **미확정 — 창 내용의 픽셀 확인.** `GetDC`+`GetPixel`은 OpenGL로 합성되는 창에서 신뢰할 수 없다. 화면 캡처는 창이 배치되는 순간을 잡아 client가 검은 상태였다. 그리기 여부는 4.2의 추적 동수로 판정했다.

---

## 5. 새로 드러난 문제 (Newly Visible Defect)

**창 모드 크기가 의도와 다르다.** 데스크탑이 더 이상 640×480으로 강제되지 않자, 3,840×2,160 고DPI 데스크탑에서 창이 868×677로 나온다. 의도한 값은 원본 640×480의 2배인 1,280×960이다.

| 데스크탑 | 관측된 창 크기 |
| - | - |
| 640×480 (게스트가 강제로 바꾼 상태) | 1,296×999 — 의도한 값 |
| 3,840×2,160 (정상 상태) | 868×677 |

- **추정 — 게스트 프로세스가 DPI 인식을 선언하지 않아 Windows가 창을 축소 배치한다.** 1,296 × (96/144) ≈ 864로 관측값과 맞는다. `AdjustWindowBoundsForDpi`는 client가 아니라 frame만 보정하므로 이 축소를 막지 못한다.
- 이 결함은 이번 변경이 만든 것이 아니라, 데스크탑이 640×480으로 강제되던 동안 가려져 있던 것이 드러난 것이다.
- 사용자가 요구한 "강제 창모드 지원"에 직접 닿는 문제이지만 DPI 인식이라는 별도 하위 시스템이므로 이번 작업 지시서 범위에 넣지 않았다. 다음 작업으로 남긴다.

---

## 6. 다음 작업 (Next Task)

1. 창 모드의 DPI 축소를 바로잡는다. 호스트 창의 DPI 인식과 client 크기 계산을 함께 본다. 기존 [DWM caption·DPI 분석](../analysis/win32-caption-dpi.md)이 출발점이다.
2. `re2dj_windows_vfs_runtime_probe`의 기존 실패를 고쳐 이번에 추가한 표시 불변식 검사가 실제로 실행되게 한다.

---

## 7. 관련 문서 (Related Documents)

- [Task 183 설계](../design/20260905-183-host-display-mode-policy.md)
- [Task 183 작업 지시서](../work-orders/20260905-183-host-display-mode-policy.md)
- [Task 182 작업 로그](20260905-182-directx7-legacy-delegation.md)
- [원본 실행 파일 구조 분석](../analysis/ez2dj-exe-structures.md)
- [Win32 실행 가이드](../guides/windows-x86-runtime.md)
