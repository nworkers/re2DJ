# ez2dj6th transform 입력 덤프 작업 로그

## 한국어

### 결과

외부 임시 경로를 지정하는 `--hardlock-transform-input-dump <path>` 옵션을 추가했습니다. 이 옵션은 transform trace를 함께 켜고, 부모 bootstrap과 followed child 모두에 출력 경로를 전달합니다.

올바른 CHD 실행에서 다음을 확인했습니다.

- dump 파일 생성 성공
- function `0x0011`
- 7개 입력 block 파싱 성공
- 각 block 크기 8바이트
- child 종료 코드 `0x00000000`

원시 block 값은 저장소에 기록하지 않았습니다. dump는 사용자가 reSoftlock 입력으로 변환한 뒤 삭제할 수 있는 외부 일회성 산출물입니다.

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1374, failures: 0`
- launcher diagnostic log: `20260906-180222-512.jsonl`
- child VFS trace에서 transform input trace와 transform request 확인

### 추가 검증 — reSoftlock map 재생성

추출한 challenge 파일은 16-hex 입력 7개로 reSoftlock이 직접 읽을 수 있는 형식으로 기록했습니다. 6th EXE에서 정적으로 생성한 20개 challenge와 비교한 결과 교집합은 0개였습니다. runtime 7개를 사용해 임시 경로에 194개 response map을 재생성했고, candidate-0 검증에서 `mapped=7:unmapped=0`을 확인했습니다. 이후 194개 전체 실행도 모두 완전 매칭됐지만, transform 이후 정상적인 후속 Hardlock 경계에 도달한 후보는 없었습니다.

## English

### Result

Added `--hardlock-transform-input-dump <path>`. The option also enables the transform trace and forwards the selected output path to both the parent bootstrap and the followed child.

The correct CHD run confirmed:

- the dump file was created
- function `0x0011`
- seven input blocks were parsed
- each block is eight bytes
- the child exited with code `0x00000000`

Raw block values were not stored in the repository. The dump is an external disposable artifact that can be converted into reSoftlock input and then removed.

### Verification

- `cmd /c scripts\build_win32.bat` succeeded.
- `build/windows-x86/bin/Debug/re2dj_unit_tests.exe`: `checks: 1374, failures: 0`.
- Launcher diagnostic log: `20260906-180222-512.jsonl`.
- Child VFS trace contains both the transform input trace and transform request.

## Additional verification — regenerated reSoftlock maps

The extracted challenge file contains seven 16-hex inputs in the format accepted directly by reSoftlock. Comparing it with the 20 challenges statically generated from the 6th executable produced zero intersection. Reusing the runtime seven inputs generated 194 response maps in a temporary directory, and candidate 0 reported `mapped=7:unmapped=0`. The full 194-run sweep also mapped every block, but no candidate reached a valid later Hardlock boundary after transform.
