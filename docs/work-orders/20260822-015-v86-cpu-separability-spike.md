# 작업 지시: v86 CPU 분리성 spike

## 목표

공식 v86 소스의 CPU·메모리·실행 루프를 완전한 PC 모델 없이 Web `ExecutionBackend`에 연결할 수 있는지 검증한다.

## 범위

1. 임시 shallow checkout으로 v86의 CPU 경계와 build 구조를 조사한다.
2. gate stop/resume, 상태 접근, write invalidation 구현 지점을 확인한다.
3. 최소 소스 집합과 직접 라이선스 고지를 기록한다.
4. 후보의 적합·부적합 판단을 architecture, knowledge base, TODO에 반영한다.
5. v86 소스나 binary를 re2DJ에 도입하지 않는다.

## 검증

- 임시 checkout의 commit ID와 라이선스 파일을 기록한다.
- 조사 결과가 공식 소스의 경로·symbol·build target에 대응하는지 확인한다.
- `git status`로 re2DJ 변경이 문서뿐인지 확인한다.

---

# Work Order: v86 CPU Separability Spike

## Objective

Verify whether the CPU, memory, and execution loop of the official v86 source can connect to Web `ExecutionBackend` without the complete PC model.

## Scope

1. Inspect v86 CPU boundaries and build structure through a temporary shallow checkout.
2. Locate gate stop/resume, state access, and write-invalidation implementation points.
3. Record the minimum source set and direct license notices.
4. Reflect suitability or rejection in the architecture, knowledge base, and TODO.
5. Do not introduce v86 source or binaries into re2DJ.

## Verification

- Record the temporary checkout commit ID and license file.
- Confirm findings correspond to official-source paths, symbols, and build targets.
- Use `git status` to confirm that re2DJ changes are documentation only.
