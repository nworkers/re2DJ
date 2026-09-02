# ez2dj4th 프로파일별 raw I/O 재사용 작업 지시서

관련 설계: [ez2dj4th 프로파일별 raw I/O 재사용 설계](../design/20260903-144-ez2dj4th-profile-io-reuse.md)

## 목적

`ez2dj1stse`에 이미 사용 중인 공용 IO 보드와 byte bus를 `ez2dj4th`의 확인된 privileged byte-read 지점에 연결하여, 동일한 idle 응답으로 다음 실행 경계까지 진행 가능한지 진단합니다.

## 범위

- `TargetLptdiPolicy`에 profile별 raw I/O helper RVA와 제품 기본 활성화 필드를 추가합니다.
- 1st profile에 기존 RVA를 명시합니다.
- 4th profile에 확인된 `IN AL,DX` RVA `0x000c3817`만 명시합니다.
- 런처와 injected runtime의 하드코딩된 1st RVA를 profile 값으로 교체합니다.
- 4th의 raw I/O는 explicit diagnostic opt-in으로만 허용하고 일반 제품 기본값으로 켜지 않도록 합니다.
- product-loader probe, unit/build 검증을 갱신합니다.
- 실제 CHD 기반 4th 진단을 실행하여 다음 boundary를 기록합니다.

## 제외 범위

- 4th의 물리 IO 보드 응답 알고리즘 확정
- 확인되지 않은 4th `OUT` RVA 추가
- 원본 EXE/HDD 자산의 저장소 복사
- 진단 결과만으로 4th 제품 기본 실행 정책 승격

## 구현 순서

1. 설계와 작업 지시 문서를 추가합니다.
2. target profile policy와 product argument 변환을 수정합니다.
3. debugger trap과 injected runtime export/handler를 profile RVA 기반으로 수정합니다.
4. profile probe와 관련 단위 검증을 갱신합니다.
5. Debug build, unit test, product-loader probe를 실행합니다.
6. 4th explicit IO 진단을 실행하고 JSONL에서 privileged/IO/boundary 이벤트를 확인합니다.
7. 분석 문서와 작업 로그를 갱신하고 task commit을 남깁니다.

## 완료 조건

- 1st 기존 실행 인자와 raw I/O 동작이 유지됩니다.
- 3rd는 raw I/O capability가 계속 비활성입니다.
- 4th는 `0x004c3817` read를 공용 bus에 연결할 수 있습니다.
- 4th에 확인되지 않은 write 주소나 물리 응답을 추가하지 않습니다.
- 빌드와 테스트 결과, 진단 로그 경로가 작업 로그에 남습니다.

---

# ez2dj4th Profile-Specific Raw I/O Reuse Work Order

Related design: [Profile-Specific Raw I/O Reuse for ez2dj4th](../design/20260903-144-ez2dj4th-profile-io-reuse.md)

## Objective

Connect the shared I/O board and byte bus already used by `ez2dj1stse` to the confirmed privileged byte-read site in `ez2dj4th`, then determine whether the shared idle response reaches the next execution boundary.

## Scope

- Add profile-specific raw-I/O helper RVAs and a product-default switch to `TargetLptdiPolicy`.
- Make the existing 1st RVAs explicit in the 1st profile.
- Register only the confirmed 4th `IN AL,DX` RVA `0x000c3817`.
- Replace hard-coded 1st RVAs in the launcher and injected runtime with profile values.
- Keep 4th raw I/O as explicit diagnostic opt-in, not a normal product default.
- Update the product-loader probe and relevant build/unit verification.
- Run the real-CHD 4th diagnostic and record the next boundary.

## Out of scope

- Identifying the physical 4th I/O board response algorithm
- Adding an unconfirmed 4th `OUT` RVA
- Copying original executables or HDD assets into the repository
- Promoting 4th to the product default based only on this diagnostic

## Implementation sequence

1. Add the design and work-order documents.
2. Update target-profile policy and product argument conversion.
3. Make debugger traps and injected-runtime exports/handlers use profile RVAs.
4. Update profile probes and related unit verification.
5. Run the Debug build, unit tests, and product-loader probe.
6. Run the explicit 4th I/O diagnostic and inspect privileged/I/O/boundary events in JSONL.
7. Update analysis and work-log documents, then create the task commit.

## Completion criteria

- Existing 1st execution arguments and raw-I/O behavior remain intact.
- 3rd remains without raw-I/O capability.
- 4th can route `0x004c3817` reads to the shared bus.
- No unconfirmed write address or physical response is added.
- Build/test results and the diagnostic log path are recorded in the work log.
