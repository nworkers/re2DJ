# 작업 지시서: EZ2DJ 6th 변환 입력 덤프

## 한국어

### 배경

올바른 CHD를 포함한 `ez2dj6th` 실행은 Hardlock `0x458` 변환 요청까지 진행하지만, 기존 194개 후보 맵은 7개 입력을 모두 미매칭합니다. 실행 로그에서 실제 descriptor function은 `0x0011`로 확인되었습니다.

### 작업

1. 런타임에 외부 임시 출력 경로를 받는 export를 추가합니다.
2. launcher의 `--hardlock-transform-input-dump` 옵션을 추가합니다.
3. 부모 bootstrap과 child handoff에 경로를 전달합니다.
4. CHD 실행과 빌드·단위 테스트로 확인합니다.
5. 원시 입력은 저장소에 남기지 않고, reSoftlock 재계산에 필요한 관찰 결과만 분석 문서에 반영합니다.

### 완료 조건

- 새 옵션으로 6th child 실행 시 7개 입력 블록이 외부 파일에 기록됩니다.
- 기존 후보 맵의 미매칭 원인을 `0x0011` 입력 경계로 설명할 수 있습니다.
- 빌드와 단위 테스트가 통과합니다.

## English

### Background

With the correct CHD attached, `ez2dj6th` reaches the Hardlock `0x458` transform request, but all seven inputs fail to match the existing 194 candidate maps. The observed descriptor function is `0x0011`.

### Work

1. Add a runtime export for a user-selected temporary output path.
2. Add the launcher option `--hardlock-transform-input-dump`.
3. Forward the path through both the parent bootstrap and child handoff.
4. Verify with the CHD run, build, and unit tests.
5. Keep raw input out of the repository and record only the necessary observation in analysis documentation.

### Done when

- A 6th child run with the new option writes seven input blocks to an external file.
- The candidate mismatch is explained by the `0x0011` input boundary.
- The build and unit tests pass.
