# 목적지 알파 블렌드 지원 작업 지시서

## 목표

원본 `ez2dj4th`가 사용하는 Direct3D 목적지 알파 blend 계수 7과 8을 HLE 공용 상태와 SDL3/OpenGL backend에서 지원하여, 해당 draw가 backend 진입 전에 탈락하지 않도록 한다.

## 범위

- `BlendFactor` enum과 raw D3D blend decoder 확장
- SDL3/OpenGL blend factor 변환 확장
- decoder 단위 테스트 및 OpenGL blend probe 회귀 테스트
- 설계, 분석, 아키텍처, 작업 로그 문서 갱신

다음은 범위에서 제외한다.

- profile 또는 launcher 설정 변경
- VFS/CHD 읽기 방식 변경
- 원본 실행 파일이나 게임 자산 변경
- note 판정·게임플레이 로직 재구현
- 지원되지 않은 blend 계수에 대한 임의 fallback

## 구현 순서

1. 확인된 진단 근거와 변환 계약을 설계 문서에 기록한다.
2. 공용 enum/decoder에 `DESTALPHA`와 `INVDESTALPHA`를 추가한다.
3. OpenGL backend에 `GL_DST_ALPHA`와 `GL_ONE_MINUS_DST_ALPHA` 매핑을 추가한다.
4. 단위 테스트와 blend probe를 추가한다.
5. 관련 KB, graphics-path 분석, `ARCHITECTURE.md`를 갱신한다.
6. Windows x86 빌드, CTest, blend probe, 진단 실행으로 검증한다.
7. 작업 로그를 작성하고 변경을 커밋한다.

## 완료 조건

- raw ABI 7과 8이 각각 올바른 공용 enum으로 변환된다.
- OpenGL backend가 두 enum을 올바른 destination-alpha 계수로 사용한다.
- 기존 blend 회귀 테스트가 유지되고 새 probe 검사가 통과한다.
- 동일 진단 실행에서 해당 unsupported blend 오류가 사라진다.
- 원본 자산과 CHD는 저장소에 추가되거나 변경되지 않는다.

## English

# Work Order: Destination-Alpha Blend Support

## Goal

Support the Direct3D destination-alpha blend factors 7 and 8 used by the original `ez2dj4th` in the shared HLE state and SDL3/OpenGL backend, so those draws are not dropped before entering the backend.

## Scope

- Extend the shared `BlendFactor` enum and raw D3D blend decoder.
- Extend the SDL3/OpenGL blend-factor conversion.
- Add decoder unit coverage and an OpenGL blend-probe regression.
- Update the design, analysis, architecture, and work-log documents.

Out of scope:

- Profile or launcher configuration changes
- VFS or CHD read-path changes
- Original executable or game-asset changes
- Reimplementation of note judgment or gameplay logic
- Arbitrary fallback for unsupported blend factors

## Implementation order

1. Record the confirmed diagnostic evidence and conversion contract in the design.
2. Add `DESTALPHA` and `INVDESTALPHA` to the shared enum and decoder.
3. Map them to `GL_DST_ALPHA` and `GL_ONE_MINUS_DST_ALPHA` in the OpenGL backend.
4. Add unit and blend-probe coverage.
5. Update the relevant KB, graphics-path analysis, and `ARCHITECTURE.md`.
6. Verify with the Windows x86 build, CTest, blend probe, and a diagnostic run.
7. Write the work log and commit the change.

## Acceptance criteria

- Raw ABI values 7 and 8 decode to the correct shared enum values.
- The OpenGL backend uses the correct destination-alpha factors.
- Existing blend regressions remain green and the new probe check passes.
- The same diagnostic run no longer reports the affected unsupported blend error.
- No original asset or CHD is added to or modified in the repository.
