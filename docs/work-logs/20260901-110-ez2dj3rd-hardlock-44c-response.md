# ez2dj3rd Hardlock 0x9c40244c descriptor 응답 경계 작업 로그

관련 설계: [ez2dj3rd Hardlock 0x9c40244c descriptor 응답 경계](../design/20260901-110-ez2dj3rd-hardlock-44c-response.md)

*Related design: [ez2dj3rd Hardlock `0x9c40244c` descriptor response boundary](../design/20260901-110-ez2dj3rd-hardlock-44c-response.md).*

## 결과

- `0x44c` 전용 post-IOCTL trace `20260901-003431-926`에서 현재 Function 0 descriptor tail word `0x0000`과 반환 뒤 byte `+0xfe` 소비를 확인했습니다.
- `0x00a4ed17`이 byte `+0xfe`를 읽고 `0x00a4ed1d`에서 검사하며, 0 경로는 handle을 닫고 전역 handle을 invalid로 되돌립니다.
- historical synthetic `0x450` replay와 명시적 `tail=0x0001`을 결합한 실행 `20260901-004347-276`은 Function 6 `0x44c` 30회와 Function `0x0e` `0x458` 30회에 도달했습니다.
- 따라서 descriptor `+0xfe`의 nonzero 값은 handle-retention과 다음 descriptor 호출 도달에 인과적입니다.
- `0x0001`은 실제 driver/dongle 응답이 아니라 분기 실험값입니다. Function `0x0e`의 유효 마지막 8바이트 출력과 실제 seed는 확정하지 않았습니다.
- bounded 관찰 뒤 해당 원본 process가 남아 있지 않음을 확인했습니다. 원본 HDD와 overlay는 변경하지 않았습니다.

*The `0x44c`-filtered post-IOCTL trace in `20260901-003431-926` confirmed the current Function-0 tail word `0x0000` and consumption of byte `+0xfe` after return. Address `0x00a4ed17` loads that byte and `0x00a4ed1d` tests it; zero closes the handle and resets the global handle to invalid. Run `20260901-004347-276`, combining the historical synthetic `0x450` replay with explicit `tail=0x0001`, reached thirty Function-6 `0x44c` calls and thirty Function-`0x0e` `0x458` calls. A nonzero descriptor byte `+0xfe` is therefore causal for handle retention and next-descriptor reachability. Value `0x0001` is a branch experiment, not a physical driver/dongle response; neither the valid final eight-byte Function-`0x0e` output nor physical seeds are identified. No matching original process remained after the bounded observation, and neither the original HDD nor overlay was changed.*

## 구현

- 플랫폼 중립 descriptor parser에 exact 256바이트 tail-word reader와 정확히 4자리인 hex parser를 추가했습니다.
- 기존 bounded descriptor marker가 알려진 고정 field 뒤 `tail_word`만 기록하도록 확장했습니다.
- 기본 비활성 `--device-mock-hardlock-44c-tail` 옵션은 synthetic device의 exact-size Function 0 `0x44c` output offset `0xfe`만 patch하고 나머지 254바이트를 보존합니다.
- 전용 runtime probe와 단위 테스트가 tail marker, exact call patch와 parser 오류를 검증합니다.

*Implementation adds a platform-neutral exact-256-byte tail-word reader and exact-four-digit hex parser. The bounded descriptor marker now records only `tail_word` after the known fixed fields. Default-off `--device-mock-hardlock-44c-tail` patches only output offset `0xfe` for an exact-size Function-0 `0x44c` on the synthetic device and preserves the other 254 bytes. Dedicated runtime and unit tests cover the marker, exact-call patch, and parser errors.*

## 검증

- Windows x86 Debug launcher, Hardlock descriptor probe와 unit tests build
- 전용 Hardlock descriptor probe 통과
- unit tests `1013` checks, `0` failures
- current-tail trace 1회와 synthetic-tail bounded 원본 실행 1회
- Windows x86 Debug CTest 4/4
- `git diff --check`

*Verification includes the Windows x86 Debug launcher, dedicated Hardlock descriptor probe and unit-test build; a passing dedicated probe; 1,013 unit checks with zero failures; one current-tail trace and one bounded synthetic-tail original run; Windows x86 Debug CTest 4/4; and `git diff --check`.*

## 다음 경계

다음 작업은 `0x9c402458` Function `0x0e` 반환 뒤 마지막 8바이트 소비와 원본 분기 oracle을 추적합니다. Task 107의 여러 seed 후보는 명시적 synthetic 분석값으로만 시험하며, 독립적인 판별 증거 없이 실제 dongle seed로 승격하지 않습니다.

*The next task traces consumption of the final eight bytes after Function-`0x0e` `0x9c402458` and the original's branch oracle. Task 107's multiple seed candidates will be tested only as explicit synthetic analysis values and will not be promoted to physical-dongle seeds without independent distinguishing evidence.*
