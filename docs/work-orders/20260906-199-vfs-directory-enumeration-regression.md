# Task 199: 디렉터리 기반 VFS 열거 회귀 수정
# Task 199: Fix Directory-Backed VFS Enumeration Regression

## 작업 목표 (Goal)

디렉터리 기반 HDD에서 `FindFirstFileA`의 상대 검색 패턴을 게스트의 논리 현재 디렉터리 기준으로 호스트 검색 패턴에 매핑하여, `ez2dj1stse`가 `System\Title`이 아닌 호스트 `ez2dj` 루트의 항목을 받지 않도록 합니다.

Map relative `FindFirstFileA` search patterns on directory-backed HDDs against the guest's logical current directory so `ez2dj1stse` does not receive entries from the host `ez2dj` root while it is in `System\Title`.

## 선행 문서 (Preceding Documents)

- [Task 199 설계](../design/20260906-199-vfs-directory-enumeration-regression.md)
- [Task 177 VFS 게스트 현재 디렉터리 설계](../design/20260904-177-vfs-guest-working-directory.md)
- [Task 177 작업 로그](../work-logs/20260904-177-vfs-guest-working-directory.md)
- [Task 198 목적지 색상 마스크 작업 로그](../work-logs/20260905-198-destination-color-mask-blending.md)

## 구현 범위 (Implementation Scope)

1. `Re2djVfsFindFirstFileA`의 directory-backed 경로에서 기존 `MapVfsPath`/`ResolveGuestNativeSuffix`를 사용하여 검색 패턴을 매핑합니다.
2. 매핑된 host path를 native `FindFirstFileA`에 전달하고, 성공/실패와 mapped path를 제한된 VFS trace에 기록합니다.
3. CHD synthetic enumeration과 native `FindNextFileA`/`FindClose` handle 경로는 유지합니다.
4. Windows VFS runtime probe에 게스트 CWD 기준 열거 회귀 테스트를 추가합니다.
5. `ARCHITECTURE.md`, 관련 analysis 문서, 작업 로그를 갱신합니다.

1. Use the existing `MapVfsPath`/`ResolveGuestNativeSuffix` for directory-backed `Re2djVfsFindFirstFileA` searches.
2. Pass the mapped host path to native `FindFirstFileA` and record the mapped path plus success/failure in the bounded VFS trace.
3. Preserve CHD synthetic enumeration and native `FindNextFileA`/`FindClose` handle paths.
4. Add a guest-CWD enumeration regression check to the Windows VFS runtime probe.
5. Update `ARCHITECTURE.md`, the relevant analysis topic, and the work log.

## 비범위 (Out of Scope)

- overlay와 원본 디렉터리 항목의 병합 열거.
- CHD에 일치 항목이 없을 때의 기존 fallback 정책 재설계.
- 코인 입력, LPTDI, 보호 루틴 또는 Direct3D 상태 변경.
- 원본 HDD/실행 파일 변경.

Overlay/original directory-entry merge enumeration, redesign of the existing empty-CHD fallback policy, coin/LPTDI/protection/Direct3D changes, and modification of original assets or executables are out of scope.

## 최소 검증 (Minimum Verification)

```powershell
cmd /c scripts\build_win32.bat
.\build\windows-x86\bin\Debug\re2dj_windows_vfs_runtime_probe.exe --vfs-enumeration-only
ctest --test-dir build\windows-x86 -C Debug -R re2dj_unit_tests --output-on-failure
git diff --check
```

실행 검증은 사용자가 같은 명령으로 수행합니다.

The user performs product verification with the same command:

```powershell
.\build\windows-x86\bin\Debug\re2dj.exe ez2dj1stse --io-config .\config\ez2dj-io.example.ini
```

코인 입력 후 종료되지 않고 다음 화면으로 진행하는지, 그리고 `.vfs.log`에 `System\Title\*.*` 매핑 기록이 남는지를 확인합니다.

After inserting a coin, verify that the process proceeds instead of exiting and that the `.vfs.log` records a `System\Title\*.*` mapping.
