# 작업 지시서: 프로파일 API VFS 경계

## 한국어

### 관련 설계

[프로파일 API VFS 경계 설계](../design/20260908-233-profile-api-vfs.md)

### 작업 항목

1. 파일 이름을 VFS로 해석하고 필요하면 CHD에서 materialize하는 공용 helper를 만듭니다.
2. `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`, `GetPrivateProfileSectionA` wrapper를 추가하고 읽기를 추적에 기록합니다.
3. dynamic resolver가 네 이름에 HLE를 돌려주게 합니다.
4. Windows x86 build와 시험을 검증합니다.
5. StreetMix 진입 실행으로 `0xC0000094`가 사라지고 게임플레이에 도달하는지 확인합니다.
6. 3rd·4th 회귀를 확인합니다.
7. 분석과 작업 로그를 갱신합니다.

### 제외 범위

- 프로파일 쓰기(`WritePrivateProfileStringA`)와 overlay 기록 정책
- INI 파싱 재구현
- overlay와 CHD 사이 INI 우선순위 변경
- 보고된 첫 번째 문제(Warning→로고 깜빡임)

### 완료 조건

- 네 API가 `route=hle`로 해석됩니다.
- `Songs\music.ini` 읽기가 추적에 남습니다.
- StreetMix 진입에서 크래시가 없고 게임플레이 화면에 도달합니다.
- unit test, product loader probe, VFS runtime probe가 통과하고 3rd·4th에 회귀가 없습니다.

## English

### Related design

[Profile API VFS Boundary Design](../design/20260908-233-profile-api-vfs.md)

### Work items

1. Add a shared helper that resolves a profile file name through the VFS, materialising it from the CHD when needed.
2. Add wrappers for `GetPrivateProfileIntA`, `GetPrivateProfileStringA`, `GetPrivateProfileSectionNamesA`, and `GetPrivateProfileSectionA`, recording each read in the trace.
3. Make the dynamic resolver answer all four names with the HLE entry points.
4. Verify the Windows x86 build and tests.
5. Confirm by a StreetMix run that `0xC0000094` is gone and gameplay is reached.
6. Confirm no regression for 3rd and 4th.
7. Update the analysis document and the work log.

### Out of scope

- Profile writes such as `WritePrivateProfileStringA` and the overlay write policy
- Reimplementing INI parsing
- Changing INI precedence between the overlay and the CHD
- The first reported problem, the Warning-to-logo flicker

### Completion criteria

- All four APIs resolve at `route=hle`.
- Reads of `Songs\music.ini` appear in the trace.
- Entering StreetMix does not crash and reaches the gameplay screen.
- The unit tests, product-loader probe, and VFS runtime probe pass with no 3rd or 4th regression.
