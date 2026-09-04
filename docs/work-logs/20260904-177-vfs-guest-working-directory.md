# 20260904-177 VFS 게스트 현재 디렉터리 추적 결과
# 20260904-177 VFS Guest Working Directory Tracking Results

## 1. 개요 (Overview)

Task 175가 확정한 두 번째 결함을 고쳤다.

**결론: VFS가 게스트의 현재 디렉터리를 추적하게 하자 자원 적재가 성공한다. 게스트가 `System\Common`으로 디렉터리를 바꾸고 `2PLAYERInsertCoin.str`을 열면 이제 `chd://EZ2DJ/System/Common/2PLAYERInsertCoin.str`으로 매핑되어 열린다. 한 실행에서 CHD 기반 열기가 54건 성공했고, 실패한 열기는 안티디버그용 `\\.\NTICE` 탐색 두 건뿐이다.**

**검증 중 별도의 결함도 드러났다. 런처가 `--chd`를 받은 그대로 런타임에 넘겨, 상대 경로일 때 자식 프로세스 안에서 이미지가 열리지 않았다. 이 때문에 지금까지 CHD 읽기 경로가 한 번도 쓰이지 않았다.**

Tracking the guest's working directory makes resource loads succeed: after the guest changes to `System\Common`, `2PLAYERInsertCoin.str` maps to `chd://EZ2DJ/System/Common/2PLAYERInsertCoin.str` and opens. One run records 54 successful CHD-backed opens with only the two anti-debug `\\.\NTICE` probes failing. Verification also exposed a second defect: the launcher passed `--chd` through verbatim, so a relative path never mounted inside the child and the CHD read path had never been used at all.

---

## 2. 변경 내용 (Changes Implemented)

`src/platform/windows/injected_runtime.cpp`

1. **현재 디렉터리 상태.** HDD 루트 기준 구성요소 목록을 둔다. 초기값은 루트, 즉 게스트가 보는 `EZ2DJ` 디렉터리다. 호스트 프로세스의 작업 디렉터리는 바꾸지 않는다.
2. **해석 일원화.** `ResolveGuestRelativePath`가 요청을 루트 기준 상대 경로로 바꾸고, `MapVfsPath`와 `ChdRelativePath`가 이것만 쓴다. 경로 파싱·정규화·결합은 기존 `re2dj/storage/guest_path.h`를 그대로 쓴다.
3. **`Re2djVfsSetCurrentDirectoryA`.** 해석한 경로가 디렉터리로 존재할 때만 상태를 갱신한다. 확인 순서는 native 트리, overlay, CHD다. 실패하면 상태를 유지하고 `FALSE`와 `ERROR_PATH_NOT_FOUND`를 돌려준다.
4. **`Re2djVfsGetCurrentDirectoryA`.** 매핑된 native 절대 경로를 Win32 버퍼 규약대로 돌려준다.
5. **추적.** `re2dj:vfs:current-directory` 항목을 남긴다.

`src/tools/windows_x86_launcher_probe/main.cpp`

6. **동적 resolver와 정적 IAT 목록**에 두 API를 추가했다.
7. **CHD 경로 절대화.** `--chd` 경로를 절대 경로로 바꾼 뒤 런타임에 기록한다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진입 추적: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-150114-649`.

### 3.1 CHD 읽기 경로가 처음으로 쓰인다 (확인됨)

`.vfs.log`의 stage 분포다.

| stage | 절대 경로 수정 전 | 수정 후 |
| - | - | - |
| `chd` | 0 | **54** |
| `native` | 32 | 1 |
| `set` (현재 디렉터리) | 1 | 5 |
| `get` (현재 디렉터리) | 1 | 3 |
| `unmapped` | 2 | 2 |

- **확인됨 — 이전 실행들이 CHD를 한 번도 읽지 않았다.** `stage=chd`가 0건이었고, 성공한 열기는 모두 staging 디렉터리에 미리 풀려 있던 파일이었다.
- **확인됨 — 원인은 상대 CHD 경로다.** 런처가 `roms\ez2dj4th\4thTrax.chd`를 그대로 넘겼고, 자식의 작업 디렉터리는 게스트 쪽이라 `Fat32Volume::Open`이 실패했다. 절대화 후 정상 마운트된다.

### 3.2 디렉터리 변경이 반영된다 (확인됨)

```
current-directory:stage=set:request=System\Common:resolved=System/Common:success=1
current-directory:stage=set:request=...\chd\ez2dj4th\EZ2DJ:resolved=:success=1
current-directory:stage=set:request=System\AmuseLogo:resolved=System/AmuseLogo:success=1
```

게스트는 상대 이름(`System\Common`)과 절대 경로 두 형태를 모두 쓴다. 절대 경로는 우리가 `GetCurrentDirectoryA`로 돌려준 값이며, 그대로 다시 받아 루트로 해석된다.

### 3.3 실패했던 자원이 열린다 (확인됨)

```
create-file:stage=request:request=2PLAYERInsertCoin.str:access=0x80000000:disposition=0x00000003
asset-open:api=CreateFileA:request=2PLAYERInsertCoin.str:mapped=chd://EZ2DJ/System/Common/2PLAYERInsertCoin.str:success=1
create-file:stage=chd:request=2PLAYERInsertCoin.str:mapped=chd://:success=1
```

- **확인됨 — Task 175에서 실패하던 34건이 사라졌다.** 남은 실패는 `\\.\NTICE` 두 건뿐이며, 이는 보호 코드의 디버거 탐색이라 실패가 정상이다.
- **확인됨 — 적재 대상은 `System\Common`, `System\AmuseLogo`, `System\warning.abm`, `Credits*.abm`, `1/2PLAYER*` 계열이다.**

### 3.4 실행이 크게 전진했다 (확인됨)

| 지표 | Task 174 실행 | 이번 실행 |
| - | - | - |
| 디버그 이벤트 | 5,233 | 50,035 |
| 특권 명령 트랩 | 4,913 | 47,343 |
| `IDirectDraw7::CreateSurface` | 26 | **236** |
| 종료 | `0xc0000005`로 프로세스 종료 | idle 경계까지 살아 있음 |

`RestoreAllSurfaces`와 `EnumSurfaces`가 각각 3회로 늘었고, surface 236개가 만들어진다. 게스트가 화면 자원을 실제로 적재하는 단계다.

### 3.5 새 중단 지점 (확인됨)

`RVA 0x0001290e`에서 읽기 접근 위반이 난다.

```
{"debug_event":"exception","code":"0xc0000005","address":"0x0041290e"}
{"exception_bytes":"8b0a50ff510c3bf4e885f20a0066c745"}
```

복원하면 `mov ecx, [edx]` 다음 `push eax; call [ecx+0x0c]`로, 객체 포인터가 유효하지 않은 상태에서 가상 호출을 시도한다. 이번 실행은 프로세스가 종료되지 않고 idle 경계에 도달했으므로, 이 예외가 치명적인지 게스트가 흡수하는지는 아직 모른다.

- **미확정 — `RVA 0x0001290e`의 접근 위반이 실행을 멈추는가.** 예외가 두 번 기록되었고 프로세스는 살아 있었다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| 게스트가 현재 디렉터리를 바꾼 뒤 상대 이름으로 연다 | **확인.** `System\Common`으로 바꾼 뒤 상대 이름으로 연다 |
| 현재 디렉터리를 추적하면 자원 적재가 성공한다 | **확인.** 실패 34건이 0건이 되었다 |
| CHD 읽기 경로가 동작하고 있었다 | **반증.** 상대 경로 때문에 한 번도 쓰이지 않았다 |

---

## 5. 다음 작업 (Next Task)

`RVA 0x0001290e`의 접근 위반을 관측한다. 먼저 이 예외가 실행을 멈추는지, 게스트의 예외 처리기가 흡수하는지 판단하고, 멈춘다면 Task 172가 추가한 코드 범위 덤프로 해당 함수와 호출자를 읽는다.

Observe the access violation at `RVA 0x0001290e`: first decide whether it stops execution or the guest's own handler absorbs it, and if it stops, read the function and its caller with the code-range dump.

---

## 6. 관련 문서 (Related Documents)

- [Task 177 설계](../design/20260904-177-vfs-guest-working-directory.md)
- [Task 177 작업 지시서](../work-orders/20260904-177-vfs-guest-working-directory.md)
- [Task 176 작업 로그](../work-logs/20260904-176-fat32-long-name-assembly.md)
- [Task 175 작업 로그](../work-logs/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
