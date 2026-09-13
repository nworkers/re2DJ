# 작업 지시 / Work order

[설계](../design/20260914-283-streaming-silence-progress.md)에 따라 소비 기반 ring 공급을
구현하고 합성 무음→tone 테스트를 추가합니다. 기존 구현에서 실패하는지 먼저 확인하고
수정 후 Windows x86 빌드와 관련 테스트를 실행합니다. 실제 JAM 청취는 미확정으로 남깁니다.

Implement consumption-based ring supply following the linked design. Add a synthetic
silence-to-tone test, verify failure before the fix, then build and run relevant Windows
x86 tests. Keep real JAM listening confirmation unresolved.
