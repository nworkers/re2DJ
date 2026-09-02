# ez2dj4th privileged-instruction 진단 작업 로그

관련 설계: [ez2dj4th privileged-instruction 진단 설계](../design/20260903-143-ez2dj4th-privileged-instruction-diagnostic.md)

*Related design: [ez2dj4th privileged-instruction diagnostic design](../design/20260903-143-ez2dj4th-privileged-instruction-diagnostic.md).*

## 작업 목적 / Objective

VFS 절대 경로 수정 뒤 남은 child exit `0xc0000096`의 실제 명령어와 register
상태를 확인하고, 4th raw-I/O HLE를 추측으로 활성화하지 않는 근거를 확보했습니다.

*Identify the instruction and register state behind the remaining child exit
`0xc0000096` after the VFS absolute-path fix, while keeping 4th raw-I/O HLE disabled
unless its contract is confirmed.*

## 변경 사항 / Changes

- attached-debugger exception loop에 `EXCEPTION_PRIV_INSTRUCTION` 전용 recorder를 추가했습니다.
- first/second-chance 상태, exception code/address, 최대 16바이트 opcode window, x86 general/control registers를 기록합니다.
- `0xec`/`0xee`를 각각 `IN AL,DX`/`OUT DX,AL`로 분류하고 EDX low word와 EAX low byte를 port/value로 기록합니다.
- 예외를 처리하거나 EIP를 변경하지 않고 기존 `DBG_EXCEPTION_NOT_HANDLED` 흐름을 유지했습니다.
- 확인된 fault 위치와 미확정 반환값 상태를 [4th Hardlock runtime 분석](../analysis/ez2dj4th-hardlock-runtime.md)에 반영했습니다.

*Added a privileged-instruction recorder to the attached-debugger exception loop;
recorded first/second-chance state, exception code/address, up to 16 opcode bytes,
and x86 general/control registers; classified `0xec`/`0xee` as `IN AL,DX`/`OUT DX,AL`
and recorded EDX's low word and EAX's low byte as port/value; preserved the existing
unhandled-exception flow without changing EIP; and updated the [4th Hardlock runtime
analysis](../analysis/ez2dj4th-hardlock-runtime.md).*

## 검증 결과 / Verification

| 항목 | 결과 |
| --- | --- |
| Windows x86 Debug build | 통과 |
| `re2dj_unit_tests.exe` | `checks: 1184, failures: 0` |
| `re2dj_windows_product_loader_probe.exe` | `profile-defaults=ok unsupported-target=ok` |
| 실제 4th `--slot-writer-trace` 실행 | 통과; privileged event 2회 기록 |
| fault | `0xc0000096` at `0x004c3817`, `kind=in_al_dx`, `port=0x0103` |

*The Windows x86 Debug build passed. `re2dj_unit_tests.exe` reported
`checks: 1184, failures: 0`, and the product-loader probe reported
`profile-defaults=ok unsupported-target=ok`. The real 4th `--slot-writer-trace` run
recorded two privileged events: first and second chance at `0x004c3817`, classified
as `in_al_dx`, with port `0x0103`.*

## 실행 증거 / Runtime evidence

- [launcher JSONL](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-003150-996.jsonl)
- [VFS trace](../../logs/windows_x86_launcher_probe/ez2dj4th/20260903-003150-996.vfs.log)

핵심 event는 다음과 같습니다.

```text
first_chance=true  address=0x004c3817 kind=in_al_dx edx=0x00ac0103 port=0x0103
first_chance=false address=0x004c3817 kind=in_al_dx edx=0x00ac0103 port=0x0103
```

두 번째 예외 뒤 child는 `0xc0000096`으로 종료했습니다. 이 값은 4th의
`IN AL,DX` fault를 확정하지만, `0x0103`에 제공해야 하는 byte 값까지 확정하지는
않습니다.

*The key events are:*

```text
first_chance=true  address=0x004c3817 kind=in_al_dx edx=0x00ac0103 port=0x0103
first_chance=false address=0x004c3817 kind=in_al_dx edx=0x00ac0103 port=0x0103
```

*The child exits with `0xc0000096` after the second exception. This confirms the
4th `IN AL,DX` fault but does not establish which byte should be supplied for port
`0x0103`.*

## 결론 / Conclusion

이번 작업으로 다음 blocker는 4th raw-I/O `IN AL,DX` helper RVA `0x004c3817`,
port `0x0103`으로 좁혀졌습니다. 다음 구현 작업은 이 port의 반환 계약을 추가
관찰한 뒤 profile별 I/O trap을 설계하는 것입니다.

*The next blocker is now narrowed to the 4th raw-I/O `IN AL,DX` helper at RVA
`0x004c3817`, port `0x0103`. The next implementation task is to observe the port's
return contract before designing a profile-specific I/O trap.*
