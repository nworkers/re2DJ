# ez2dj3rd Hardlock 입출력 쌍 추적 작업 로그

## 결과

3rd `EZ2DJ.EXE`에서 `Function 0x0e`에 들어가는 18개 입력 블록을 파일 위치까지 확정했습니다. 이들은 `.text`의 raw offset/RVA `0x1000`부터 `0x8000` 간격으로 이어지는 18개 32KiB chunk의 첫 8바이트입니다. 18회 호출에는 17개의 고유 입력이 있으며 한 블록이 두 번 사용됩니다.

유효한 출력 8바이트는 EXE만으로 확정하지 못했습니다. 호출부는 in-place 반환과 API 성공 여부를 사용하며 출력을 평문 고정 상수와 직접 비교하지 않습니다. synthetic no-op에서 output이 input과 같은 것은 실제 입출력 쌍이 아닙니다.

*The 18 input blocks passed by the 3rd `EZ2DJ.EXE` to `Function 0x0e` were mapped to exact file locations. They are the first eight bytes of 18 consecutive 32 KiB `.text` chunks, spaced every `0x8000` bytes from raw offset/RVA `0x1000`. The calls contain 17 unique inputs, with one block used twice. Valid eight-byte outputs could not be established from the executable alone. The call path uses an in-place return and API success, without directly comparing the output to a plaintext fixed constant. Output equal to input under a synthetic no-op is not a valid input/output pair.*

## 분석 대상

- 파일 크기: `1,216,512`바이트
- SHA-256: `E370CA0DEBE58E57784EE72E50A1C3A8811C83555238B9D1A40E1717BE724B4D`
- PE32 image base: `0x00400000`
- 보호 entry point: `0x00a42240`

*Target: 1,216,512-byte PE32 image with SHA-256 `E370CA0DEBE58E57784EE72E50A1C3A8811C83555238B9D1A40E1717BE724B4D`, image base `0x00400000`, and protected entry point `0x00a42240`.*

## 수행 내용

1. PE section과 보호 entry point를 재확인했습니다.
2. 디스크의 `.protect`가 실행 시 복호화되는 보호 바이트여서 정적 dumpbin 결과를 런타임 코드로 사용할 수 없음을 확인했습니다.
3. 기존 `20260830-233623-425.vfs.log`에서 18개 0x0e packet의 마지막 8바이트를 순서대로 추출했습니다.
4. 각 입력을 원본 EXE 전체에서 검색해 `0x1000 + n * 0x8000` chunk 시작과 정확히 대응시켰습니다.
5. post-IOCTL 진단기에 정적 `ExitProcess` import가 없는 bounded trace fallback, 선택적 API watch 오류 정리, system API watch 분리, control-code filter를 추가했습니다.
6. 3rd에서 runtime `DeviceIoControl` breakpoint 자체가 앞선 보호 초기화에 영향을 주어 `0x9c402458` 반환 이후의 새 trace는 확보하지 못했습니다. 추정 응답을 제품 코드에 남기지 않았습니다.

*Work performed: reconfirmed PE sections and the protected entry point; established that the on-disk `.protect` bytes are not usable as runtime disassembly; extracted the final eight bytes from 18 packets in `20260830-233623-425.vfs.log`; mapped every input back to the original executable at `0x1000 + n * 0x8000`; improved post-IOCTL diagnostics with a bounded no-static-ExitProcess fallback, optional-watch error cleanup, separated system watches, and a control-code filter; and confirmed that the runtime `DeviceIoControl` breakpoint itself perturbs the earlier 3rd protection initialization, preventing a new post-`0x9c402458` trace. No guessed response remains in product code.*

## 검증

- Windows x86 Debug launcher 빌드 성공.
- `git diff --check` 통과.
- bounded 실행 `20260831-002320-473.jsonl`, `20260831-002716-788.jsonl`에서 fallback과 control-code filter 준비를 확인했습니다.
- CTest `3/3` 통과.

*Verification: the Windows x86 Debug launcher built successfully; `git diff --check` passed; bounded runs `20260831-002320-473.jsonl` and `20260831-002716-788.jsonl` confirmed fallback and filter preparation; and CTest passed `3/3` tests.*

## 결론 및 다음 입력

EXE 분석으로 입력 18개는 확정했지만 출력은 dongle의 secret-dependent 변환 결과입니다. 실제 3rd Hardlock에서 이 18개 요청을 기록하거나 seed를 확보해야 완전한 입출력 표를 만들 수 있습니다. 원본 실행 파일과 dump는 저장소에 추가하지 않습니다.

*The executable establishes all 18 inputs, but the outputs are results of a dongle secret-dependent transform. A complete input/output table requires recording these requests against the real 3rd Hardlock or obtaining its seeds. Original executables and dumps are not added to the repository.*

## 관련 문서

- `docs/design/20260831-105-ez2dj3rd-hardlock-io-pair-trace.md`
- `docs/work-orders/20260831-105-ez2dj3rd-hardlock-io-pair-trace.md`
- `docs/analysis/ez2dj3rd-hardlock-function-0e.md`

*Related documents: `docs/design/20260831-105-ez2dj3rd-hardlock-io-pair-trace.md`, `docs/work-orders/20260831-105-ez2dj3rd-hardlock-io-pair-trace.md`, and `docs/analysis/ez2dj3rd-hardlock-function-0e.md`.*

## 후속 분석 / Follow-up analysis

- 겹치는 런타임 코드 창을 합쳐 `0x00a4f008..0x00a4f167`의 Hardlock wrapper를 복원했습니다. wrapper는 `0x100 + count * 8` 임시 in-place packet을 구성하지만 응답 8바이트를 고정 상수와 비교하지 않습니다.
- injected runtime에 synthetic device IOCTL의 control code와 input/output 크기를 기록하는 경량 로그를 추가했습니다. 응답 정책은 변경하지 않았습니다.
- 성공 반환 비교 실행 `20260831-015158-836`은 `0x9c402468` 뒤 종료 코드 5, 실패 반환 비교 실행 `20260831-015257-962`는 같은 요청 뒤 종료 코드 8을 기록했습니다. 둘 다 `0x458`에는 도달하지 않았습니다.
- 세 16비트 seed나 유효 output으로 오인될 수 있는 통계 후보와 임시 응답 분기는 제품 코드에 남기지 않았습니다.

*Overlapping runtime windows reconstructed the Hardlock wrapper at `0x00a4f008..0x00a4f167`. It builds a `0x100 + count * 8` temporary in-place packet but does not compare the returned eight bytes with a fixed constant.*

*The injected runtime now writes a lightweight synthetic-device IOCTL log containing the control code and input/output sizes; response policy is unchanged. Run `20260831-015158-836` recorded exit code 5 after `0x9c402468`, while failure-return comparison run `20260831-015257-962` recorded exit code 8 after the same request. Neither reached `0x458`. Statistical candidates or experimental response branches that could be mistaken for the three seeds or valid outputs were not retained in product code.*

최종 Windows x86 Debug 빌드와 CTest `3/3`이 통과했습니다. VFS runtime probe는 `device-ioctl-entry`에 control code `0x9c406410`, input size 4, output size 8이 기록되는지 직접 검증합니다. `git diff --check`도 통과했습니다.

*The final Windows x86 Debug build and all `3/3` CTest cases passed. The VFS runtime probe directly verifies a `device-ioctl-entry` record containing control code `0x9c406410`, input size 4, and output size 8. `git diff --check` also passed.*
