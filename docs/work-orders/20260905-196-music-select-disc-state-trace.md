# 작업 지시 196: Music Select 원판 상태 추적

# Work Order 196: Music Select Disc-State Trace

## 목적 / Purpose

일반 graphics trace 상한에 가려진 Music Select 중앙 원판 draw의 texture-coordinate transform 및 정점 modulation 상태를 기록합니다.

*Record the texture-coordinate transform and vertex modulation state of the Music Select center-disc draws, which are currently hidden by the ordinary graphics-trace limit.*

## 범위 / Scope

1. `texture=279`와 `texture=387`의 별도 bounded diagnostic을 추가합니다.
2. 해당 diagnostic에 raw Direct3D stage state, texture matrix, cull/blend state와 원본 정점을 기록합니다.
3. 설계·분석·작업 로그를 갱신하고 Win32 build 및 테스트를 실행합니다.

*This task is diagnostic-only. It does not change blend, texture sampling, culling, or rendering output.*

## 완료 기준 / Acceptance Criteria

- 일반 trace 예산이 소진되어도 두 texture identity의 draw가 기록됩니다.
- 기록만으로 texture transform 활성화 여부와 정점별 diffuse 값 차이를 판별할 수 있습니다.
- Win32 build, unit tests, CTest가 통과합니다.
