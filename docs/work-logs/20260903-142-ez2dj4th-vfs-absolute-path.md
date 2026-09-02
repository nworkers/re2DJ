# VFS 호스트 절대 경로 재해석 작업 로그

관련 설계: [VFS 호스트 절대 경로 재해석 설계](../design/20260903-142-ez2dj4th-vfs-absolute-path.md)
관련 작업 지시: [VFS 호스트 절대 경로 재해석 작업 지시](../work-orders/20260903-142-ez2dj4th-vfs-absolute-path.md)

*Related design: [VFS host-absolute path remapping design](../design/20260903-142-ez2dj4th-vfs-absolute-path.md).*
*Related work order: [VFS host-absolute path remapping work order](../work-orders/20260903-142-ez2dj4th-vfs-absolute-path.md).*

## 작업 목적 / Objective

Hardlock 응답 재료 적용 후 `EZ2DJ.ini` 요청에서 발생한 VFS 경로 이중 결합을 제거하고,
같은 경로 정책을 runtime probe로 고정했습니다.

*Remove the VFS path double-join observed at `EZ2DJ.ini` after Hardlock response material
was applied, and pin the same path policy in the runtime probe.*

## 변경 사항 / Changes

- `MapVfsPath`에 설정된 HDD root와 HLE Windows root 하위 host absolute path를 상대 suffix로 복원하는 경로를 추가했습니다.
- Windows 경로 구분자 `\\`와 `/`를 동일하게 처리하고, root 경계 뒤에 separator 또는 문자열 끝이 있는지 확인합니다.
- `GuestHddSuffix`에도 HDD root absolute path 해석을 적용하여 CHD materialization/fallback이 같은 상대 경로를 사용하게 했습니다.
- `windows_vfs_runtime_probe`에 HDD absolute read, HLE Windows-root absolute read, absolute copy-on-write write 검증을 추가했습니다.
- 원본 HDD는 변경하지 않고 overlay에만 기록되는지 probe에서 확인하도록 했습니다.
- 현재 VFS 경로 정책을 `ARCHITECTURE.md`에 반영했습니다.

*Added host-absolute path conversion under the configured HDD and HLE Windows roots to
`MapVfsPath`; treated `\\` and `/` as equivalent with a root-boundary check; applied the
same HDD-root conversion to `GuestHddSuffix` for CHD materialization/fallback; and added
HDD-absolute read, HLE-Windows-root-absolute read, and absolute copy-on-write coverage to
`windows_vfs_runtime_probe`. The probe checks that writes reach only the overlay. The
current VFS path policy is also reflected in `ARCHITECTURE.md`.*

## 검증 결과 / Verification

| 항목 | 결과 |
| --- | --- |
| `git diff --check` | 통과 |
| Windows x86 Debug build | 통과 |
| `re2dj_unit_tests.exe` | `checks: 1184, failures: 0` |
| `re2dj_windows_product_loader_probe` | 통과 |
| `re2dj_windows_vfs_runtime_probe` | 15초 제한에서 timeout; 기존 window-size 검증 이후 hang으로 원인 미확정 |
| `re2dj.exe ez2dj4th` | launcher host 실행 통과; child exit `0xc0000096` |

*`git diff --check` and the Windows x86 Debug build passed. `re2dj_unit_tests.exe`
reported `checks: 1184, failures: 0`, and the product loader probe passed. The existing
`re2dj_windows_vfs_runtime_probe` timed out under a 15-second bound after its known
window-size check; its cause remains outside this task. The product launcher completed
its host handoff, while the child exited with `0xc0000096`.*

## 실행 증거 / Runtime evidence

최신 로그는 다음과 같습니다.

- [launcher JSONL](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-001920-780.jsonl)
- [VFS trace](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-001920-780.vfs.log)

JSONL은 `hardlock_cfg_material`에서 `response450=true`, `tail44c=true`, `map=true`를
기록했습니다. VFS trace는 transform 36회를 모두 `mapped=1:unmapped=0`으로 기록하고,
`EZ2DJ.ini`에 대해 다음과 같이 기록합니다.

```text
request=C:\...\EZ2DJ\EZ2DJ.ini
mapped=C:\...\EZ2DJ\EZ2DJ.ini:success=1:error=0
```

이후 `runtime_detached_exit`의 child code는 `0xc0000096`입니다. 이는 이번 수정으로
Hardlock 통과와 `EZ2DJ.ini` 경로 이중 결합은 해결되었지만, 그 다음 Win32/HLE 경계가
아직 남아 있음을 의미합니다. fault instruction address는 현재 로그에 없으므로 다음
작업에서 예외 지점 계측을 추가해야 합니다.

*The latest evidence is the [launcher JSONL](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-001920-780.jsonl)
and [VFS trace](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-001920-780.vfs.log).
The JSONL records `response450=true`, `tail44c=true`, and `map=true`. The VFS trace records
all 36 transforms as `mapped=1:unmapped=0`, and records the `EZ2DJ.ini` request and mapped
path identically with `success=1:error=0`. The child then exits with `0xc0000096`; no faulting
instruction address is present, so exception-point instrumentation is the next task.*

## 결론 / Conclusion

이번 작업의 VFS 경로 수정은 완료되었습니다. Hardlock 이후 첫 파일 경계인 `EZ2DJ.ini`까지
정상 진행했으며, 다음 blocker는 별도의 예외 지점 분석 대상으로 분리합니다.

*The VFS path fix is complete. Execution now reaches the first file boundary after Hardlock,
`EZ2DJ.ini`, and the next blocker is separated as a distinct exception-point analysis task.*
