# Windows Preferred-Base Diagnostic

## 한국어

1. requested-base `VirtualAlloc` 실패 diagnostic을 추가합니다.
2. Windows helper와 observer를 빌드합니다.
3. 실제 HDD observer를 재실행하고 충돌 매핑의 종류를 확인합니다.
4. helper 전용 UTF-8 active-code-page manifest를 적용합니다.
5. 예약 실패 시 PE 전체 크기 범위에서 첫 점유 영역을 보고합니다.
6. CRT 이전 reservation 실험으로 loader 이전 확보 가능 여부를 확인합니다.
7. 안전하지 않은 실험 코드는 제거하고, 확인된 Windows 실행 경계를 작업 로그에 기록합니다.

## English

1. Add diagnostics for requested-base `VirtualAlloc` failures.
2. Build the Windows helper and observer.
3. Re-run the live HDD observer and identify the conflicting mapping type.
4. Apply a helper-only UTF-8 active-code-page manifest.
5. On a reservation failure, report the first occupied region across the complete PE-image range.
6. Test whether a pre-CRT reservation can obtain the address before loader state is established.
7. Remove unsafe experimental code and record the confirmed Windows execution boundary in the work log.
