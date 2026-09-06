# ez2dj6th Hardlock 후보 검증 작업 지시

## 작업 목표

사용자가 제공한 `cfg/ez2dj6th` 후보 map 194개를 원본 6th bootstrap 경로에서 검증하고, 실제 `0x458` 복호화 요청에 도달하는 후보가 있는지 확인합니다.

*Work objective*

Test the 194 candidate maps supplied under `cfg/ez2dj6th` through the original 6th bootstrap path and determine whether any candidate reaches a real `0x458` decryption request.

## 절차

1. `EZ2DJ/EZ2DJ.EXE`를 시작점으로 사용합니다.
2. `EZ2DJ6th.EXE` child를 추적하고 child에 동일한 HLE/Hardlock 진단 경계를 전달합니다.
3. 각 candidate map을 하나씩 주입합니다.
4. handshake, descriptor, transform 횟수를 별도로 기록합니다.
5. transform이 발생하지 않으면 후보 seed 판정을 보류합니다.

*Procedure*

1. Use `EZ2DJ/EZ2DJ.EXE` as the entry point.
2. Follow the `EZ2DJ6th.EXE` child and pass the same HLE/Hardlock diagnostic boundary to it.
3. Inject one candidate map at a time.
4. Count handshake, descriptor, and transform requests separately.
5. If no transform occurs, withhold seed judgement.

## 안전 범위

- 원본 CHD/HDD와 실행파일은 변경하지 않습니다.
- candidate response bytes와 seed values는 문서와 로그 요약에 복사하지 않습니다.
- 진단 후 남은 프로세스가 없는지 확인합니다.
- 이 작업에서는 product 기본 Hardlock 설정을 변경하지 않습니다.

*Safety scope*

- Do not modify the original CHD/HDD or executables.
- Do not copy candidate response bytes or seed values into documentation or summary logs.
- Verify that no process remains after diagnostics.
- Do not change the product's default Hardlock configuration in this task.
