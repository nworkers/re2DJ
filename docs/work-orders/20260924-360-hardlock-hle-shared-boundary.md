# 작업 360 작업 지시서 — Hardlock HLE 연결부 공용화와 WTS 이름 정정 / Task 360 work order — Sharing the Hardlock HLE wiring and correcting the WTS names

설계: [20260924-360-hardlock-hle-shared-boundary.md](../design/20260924-360-hardlock-hle-shared-boundary.md)

## 단계 / Steps

1. 변경 전 Windows build로 실제 4th를 실행해 Hardlock 요청 기준 기록을 남긴다.
2. `hle/hardlock/device_material`(A), `hle/hardlock/device_call`(B), `hle/guest_device_path`(C)를 추가하고 단위 테스트를 작성한다.
3. launcher가 A를, injected runtime이 B·C를 쓰게 바꾼다. 동작은 바꾸지 않는다.
4. WTS 이름을 정정한다(D).
5. Windows x86 build, CTest, product loader probe, Linux x64·x86 build와 CTest를 실행한다. 변경 후 build로 실제 4th를 다시 실행해 1번 기록과 비교한다.
6. 분석, `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그를 갱신하고 커밋한다.

*Steps: (1) run the real 4th with the pre-change Windows build to record a Hardlock-request baseline; (2) add `hle/hardlock/device_material` (A), `hle/hardlock/device_call` (B), and `hle/guest_device_path` (C) with unit tests; (3) move the launcher onto A and the injected runtime onto B and C without behavior change; (4) correct the WTS names (D); (5) run the Windows x86 build, CTest, and the product loader probe, plus Linux x64/x86 builds and CTest, then rerun the real 4th with the post-change build and compare with step 1; (6) update the analysis, `ARCHITECTURE.md`, `docs/TODO.md`, and the work log, then commit.*

## 완료 조건 / Completion criteria

* 새 단위 테스트와 기존 테스트가 통과한다(기존 Windows probe 실패 제외).
* 실제 4th의 Hardlock 요청 종류·횟수·결과와 cfg 출처 기록이 변경 전후로 같다.

*Completion: the new and existing tests pass (apart from the existing Windows probe failure), and the real 4th's Hardlock request kinds, counts, and outcomes plus the cfg-provenance record match before and after.*
