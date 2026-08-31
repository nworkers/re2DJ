# 3rd Hardlock 보류 체크포인트 작업 지시

관련 설계: [3rd Hardlock 보류 체크포인트](../design/20260901-112-3rd-hardlock-pause-checkpoint.md)

*Related design: [3rd Hardlock pause checkpoint](../design/20260901-112-3rd-hardlock-pause-checkpoint.md).*

## 범위

1. Task 109와 Task 110의 확인된 경계를 누적 문서에서 다시 연결합니다.
2. `0x44c` tail 소비 주소, 합성 `tail=0x0001` 도달 결과, 실제 응답으로 승격하지 않는 금지선을 기록합니다.
3. TODO에서 3rd Hardlock을 사용자 보류 영역으로 옮기고 다음 작업을 Task 111로 명시합니다.
4. 보류 상태 문서와 작업 로그를 커밋한 뒤 `main`에 squash merge합니다.

*Scope: reconnect the confirmed Task 109 and Task 110 boundaries; record the `0x44c` tail consumer addresses, synthetic `tail=0x0001` reachability, and prohibition against promoting it to a physical response; move 3rd Hardlock into the user-paused TODO section and name Task 111 as the next work; commit the checkpoint and squash-merge it into `main`.*

## 완료 조건

- 확인됨/미확정/합성값의 구분이 설계·TODO·작업 로그에 일관되게 나타납니다.
- Task 111 재개에 필요한 실행 ID와 문서 링크가 남습니다.
- 원본 자산, 전체 packet, seed 후보를 저장소에 추가하지 않습니다.
- 병합 전 `VERSION` patch를 증가시키고, 병합 후 annotated tag를 생성합니다.
