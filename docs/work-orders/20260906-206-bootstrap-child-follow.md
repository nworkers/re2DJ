# EZ2DJ 6th bootstrap 자식 프로세스 추적 작업 지시서

## 관련 설계

[EZ2DJ 6th bootstrap 자식 프로세스 추적 설계](../design/20260906-206-bootstrap-child-follow.md)

## 목표

`EZ2DJ.EXE`가 생성하는 `EZ2DJ6th.EXE` 자식 프로세스를 launcher가 추적하고,
실제 게임 프로세스에 HLE runtime과 Hardlock candidate map을 주입하여 seed 후보를
실행으로 판정할 수 있게 합니다.

## 작업 항목

1. bootstrap child-follow 실행 정책과 launcher 옵션을 추가합니다.
2. `DEBUG_PROCESS` debug event에서 child image와 primary thread를 식별합니다.
3. child entry breakpoint와 runtime injection 경계를 재사용 가능한 함수로 분리합니다.
4. bootstrap에는 VFS/device/Hardlock를 유지하고, child에는 VFS, device, Hardlock response,
   transform map, message-box, graphics/audio/input HLE를 설정합니다.
5. parent/child debug trace와 종료 상태를 분리 기록합니다.
6. 명시적 synthetic baseline을 사용한 candidate 0으로 child follow plumbing을 검증하고,
   유효한 6th response/seed가 확정된 뒤 전체 194개 candidate sweep을 수행합니다.
7. 관련 분석 문서와 작업 로그를 갱신하고 Windows x86 Debug build 및 unit test를 수행합니다.

## 범위 제외

- 6th response450 값을 추측하거나 코드 상수로 추가하지 않습니다.
- reSoftlock seed/response 계산을 re2DJ에 구현하지 않습니다.
- 원본 EXE 또는 CHD를 저장소에 추가하지 않습니다.
- 기존 direct 실행 경로는 다른 profile에 대해 유지하며, 6th는 bootstrap follow 정책으로
  전환합니다.

---

# EZ2DJ 6th Bootstrap Child-Process Follow Work Order

## Related design

[EZ2DJ 6th Bootstrap Child-Process Follow Design](../design/20260906-206-bootstrap-child-follow.md)

## Goal

Make the launcher follow the `EZ2DJ6th.EXE` child created by `EZ2DJ.EXE`, then inject
the HLE runtime and Hardlock candidate map into the actual game process so seed
candidates can be judged by execution.

## Work items

1. Add the bootstrap child-follow policy and launcher option.
2. Identify the child image and primary thread from `DEBUG_PROCESS` events.
3. Extract child entry-breakpoint and runtime-injection preparation into a reusable helper.
4. Keep VFS/device/Hardlock active in the bootstrap and configure VFS, device, Hardlock,
   transform-map, message-box, graphics/audio/input HLE in the child.
5. Record parent/child debug traces and exit states separately.
6. Verify child-follow plumbing with candidate 0 under an explicit synthetic baseline;
   run all 194 candidates only after a valid 6th response/seed is confirmed.
7. Update analysis/work-log documents and run the Windows x86 Debug build and unit tests.

## Out of scope

- Do not guess a 6th response450 value or add one as a code constant.
- Do not implement reSoftlock seed/response calculation in re2DJ.
- Do not add the original executable or CHD to the repository.
- Keep direct execution policies for other profiles; 6th intentionally selects the
  bootstrap and follows its game child.
