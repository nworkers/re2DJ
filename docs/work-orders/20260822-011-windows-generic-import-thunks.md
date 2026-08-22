# 작업 지시: Windows import별 native thunk

## 목표

Windows helper가 일반 PE32 이름/ordinal import를 해석하고 각 고유 import를 실행 가능한 thunk와 synthetic gate metadata로 노출하게 합니다.

## Goal

Make the Windows helper parse general PE32 named/ordinal imports and expose each unique import through an executable thunk and synthetic gate metadata.

## 작업 항목

1. protocol version을 2로 올리고 import count/metadata packet을 추가합니다.
2. helper의 hard-coded import parser를 descriptor/thunk 전체 순회로 교체합니다.
3. `ImportGateTable`로 고유 gate를 배정하고 중복 IAT 항목을 같은 thunk에 연결합니다.
4. `EDX:EAX`, 동적 stack cleanup과 synthetic gate identity를 보존하는 x86 thunk/bridge를 구현합니다.
5. backend가 metadata를 검증해 `LoadedPeImage.imports`를 채우게 합니다.
6. synthetic image와 host probe를 이름 import 및 ordinal import 두 번 호출로 확장합니다.
7. 관련 아키텍처, 포팅 계획, README, TODO와 작업 로그를 갱신합니다.
8. x64/x86 warnings-as-errors build와 모든 관련 test/probe를 실행하고 커밋합니다.

## Work items

1. Bump the protocol to version 2 and add import-count/metadata packets.
2. Replace the helper's hard-coded import parser with full descriptor/thunk traversal.
3. Assign unique gates through `ImportGateTable` and connect duplicate IAT entries to one thunk.
4. Implement an x86 thunk/bridge preserving `EDX:EAX`, dynamic stack cleanup, and synthetic gate identity.
5. Make the backend validate metadata and populate `LoadedPeImage.imports`.
6. Extend the synthetic image and host probe to call one named and one ordinal import.
7. Update architecture, porting plan, READMEs, TODO, and the work log.
8. Run x64/x86 warnings-as-errors builds and all relevant tests/probes, then commit.

## 완료 조건

host가 `LoadedPeImage.imports`에서 이름/ordinal metadata 두 개를 확인하고 각 event의 synthetic gate를 식별하며, 인자 41→EAX 42와 인자 42→EDX:EAX `1:43` 두 왕복 후 guest result 44 및 child exit 0을 확인하면 완료입니다.

## Completion criteria

The task is complete when the host sees named and ordinal metadata in `LoadedPeImage.imports`, identifies each event's synthetic gate, completes argument/result round trips from 41 to EAX 42 and from 42 to EDX:EAX `1:43`, and observes guest result 44 with child exit zero.
