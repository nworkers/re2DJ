# Linux ez2dj4th first-boundary observation / Linux ez2dj4th 첫 경계 관찰

## 목적 / Purpose

Windows에서 사용한 `ez2dj4th` built-in profile과 같은 로컬 MAME CHD를 Linux x64 host와 i386 helper에서 읽어, 원본 PE32가 Linux native execution 경로에서 처음 보고하는 boundary를 관찰합니다.

*Read the same local MAME CHD used by the Windows `ez2dj4th` built-in profile with the Linux x64 host and i386 helper, then observe the first boundary reported by the original PE32 on the Linux native-execution path.*

## 방법과 범위 / Method and scope

저장소 기준 `roms/ez2dj4th/4thTrax.chd`의 FAT32 내부 `EZ2DJ/EZ2DJ.EXE`를 profile shortcut으로 선택합니다. Linux CLI는 플랫폼 중립 `Fat32Volume::MaterializeFile()`로 canonical PE와 필요한 6th child만 caller-owned temporary staging에 복사한 뒤 별도 i386 helper를 명시해 `--run`을 실행합니다. 원본 CHD는 읽기 전용이고, guest write overlay와 CHD runtime VFS는 이 첫-boundary 작업 범위에 포함하지 않습니다. 현재 CLI 정책은 첫 import gate·fault·process exit를 결과로 보고한 뒤 helper를 종료하므로, 미등록 Win32 import를 성공으로 답하지 않습니다.

*Select `EZ2DJ/EZ2DJ.EXE` inside the FAT32 volume of repository-relative `roms/ez2dj4th/4thTrax.chd` through the profile shortcut. The Linux CLI uses platform-neutral `Fat32Volume::MaterializeFile()` to copy the canonical PE and only the required 6th child into caller-owned temporary staging, then passes the separate i386 helper to `--run`. The original CHD stays read-only; guest write overlay and CHD runtime VFS are outside this first-boundary task. The current CLI policy reports the first import gate, fault, or process exit, then stops the helper; it never completes an unregistered Win32 import successfully.*

관찰 결과는 Linux backend/PE mapping/protocol 경계의 증거이며, 보호 해제 성공·Hardlock 호환·게임 실행 성공으로 해석하지 않습니다. 새 사실은 `docs/analysis/`에 confirmed 또는 unresolved로 기록합니다.

*The result is evidence for the Linux backend, PE mapping, and protocol boundary. It does not establish successful protection removal, Hardlock compatibility, or game execution. Record new facts in `docs/analysis/` as confirmed or unresolved.*

## 검증 / Verification

WSL Ubuntu 24.04에서 i386 helper ELF class를 확인하고 Linux x64 CLI를 실행합니다. 결과에 profile, canonical path, load/entry address, first event kind와 import metadata 또는 fault/exit 상태가 있는지 확인합니다.

*On WSL Ubuntu 24.04, verify the i386 helper ELF class and run the Linux x64 CLI. Check the result for profile, canonical path, load/entry address, and the first event kind with import metadata or fault/exit status.*
