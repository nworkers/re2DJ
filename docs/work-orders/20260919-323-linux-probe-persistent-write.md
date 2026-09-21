# 작업 323: Linux probe persistent-write 기대값 / Task 323: Linux probe persistent-write expectation

## 목표 / Goal

보호 페이지 subrange 검증에서 허용된 first-page write 뒤에도 x64/x86 native IPC probe가 성공하도록 fixture의 persistent byte 기대값을 수정한다.

*Correct the fixture's persistent-byte expectation so the x64/x86 native IPC probe succeeds after the allowed first-page write in protected-page subrange validation.*

## 작업 / Work

1. initial bytes와 first-page write 뒤 bytes를 별도 이름으로 표현한다.
2. 다음 import의 readback이 변경된 bytes를 비교하게 한다.
3. helper protocol과 보호 정책은 변경하지 않는다.
4. full x64/x86 native-helper probe 스크립트를 실행하고 결과를 기록한다.

*1. Represent initial bytes and post-first-page-write bytes with distinct names.
2. Make next-import readback compare the changed bytes.
3. Do not change helper protocol or protection policy.
4. Run the full x64/x86 native-helper probe script and record the result.*

## 완료 기준 / Completion criteria

두 호스트 아키텍처에서 script가 성공하고, first-page write 성공·second-page write 거부·다음 import의 changed-byte readback·free가 모두 검증되어야 한다.

*The script succeeds on both host architectures, verifying successful first-page write, rejected second-page write, changed-byte readback at the next import, and free.*
