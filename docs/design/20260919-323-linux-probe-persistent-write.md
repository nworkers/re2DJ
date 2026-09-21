# Linux protected-page probe persistent-write expectation / Linux 보호 페이지 probe 지속 쓰기 기대값

## 목적 / Purpose

Linux native IPC host probe가 8 KiB guest allocation의 첫 페이지에 성공적으로 쓴 바이트를 다음 import event에서 정확히 검증하게 한다. 이 fixture는 첫 페이지 쓰기 허용과 두 번째 read-only 페이지 쓰기 거부를 같은 실행에서 확인하므로, 이후 persistent-memory 검사는 실제 변경된 값을 기대해야 한다.

*Make the Linux native IPC host probe verify at the next import event the byte that it successfully wrote to the first page of an 8 KiB guest allocation. Because this fixture checks allowed first-page writing and rejected second-page writing in one execution, the later persistent-memory check must expect the actual changed value.*

## 계약 / Contract

초기 persistent bytes는 `{9, 8, 7, 6}`이다. second page를 read-only로 바꾼 뒤 first page의 첫 바이트에 `{5}`를 쓰는 것은 성공해야 하며, 다음 import에서는 `{5, 8, 7, 6}`을 읽어야 한다. second-page write는 실패해야 하고, protection restore, full-range protection, free 검사는 기존대로 유지한다.

*Initial persistent bytes are `{9, 8, 7, 6}`. After making the second page read-only, writing `{5}` to the first byte of the first page must succeed, and the next import must read `{5, 8, 7, 6}`. The second-page write must fail, while protection restore, full-range protection, and free checks remain unchanged.*

이 변경은 helper protocol, allocation access policy, product runner 또는 Win32 memory API를 바꾸지 않는다. fixture의 기대값만 실제 허용된 쓰기와 일치시킨다.

*This change does not alter the helper protocol, allocation access policy, product runner, or any Win32 memory API. It only aligns the fixture expectation with the allowed write it actually performed.*

## 검증 / Validation

`bash scripts/test_linux_native_helper_probe.sh`가 x64 host와 x86 host에서 모두 성공해야 한다. 이 스크립트는 각 CTest, shared i386 helper, normal import completion, terminal stop, capability rejection, fault path를 다시 확인한다.

*`bash scripts/test_linux_native_helper_probe.sh` must succeed for both x64 and x86 hosts. It rechecks each CTest, the shared i386 helper, normal import completion, terminal stop, capability rejection, and the fault path.*
