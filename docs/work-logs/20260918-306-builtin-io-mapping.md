# 작업 306 작업 로그 — 기본 키 매핑 내장과 `--io-config` 덮어쓰기 / Task 306 work log — Built-in key mapping with `--io-config` as an override

설계: [20260918-306-builtin-io-mapping.md](../design/20260918-306-builtin-io-mapping.md)
작업 지시: [20260918-306-builtin-io-mapping.md](../work-orders/20260918-306-builtin-io-mapping.md)

## 한국어

### 구현

* **기본값.** `Ez2DjKeyboardInput`·`Ez2DancerKeyboardInput`의 바인딩 표에 기본 키를 **이름**으로 넣었다. 값이 아니라 이름이므로 기본값도 INI 값과 같은 해석 함수를 지난다. 해석 함수는 `ParseKeyboardKeyName`으로 공개했다.
* **덮어쓰기.** `ReadKeyboardKeyBinding`이 `bool* present`로 항목 존재 여부를 알려준다. 없는 항목은 기본값을 유지하고, `NONE`이라고 적힌 항목만 바인딩을 해제한다. 존재 여부는 키 이름으로 나올 수 없는 표식 문자열을 `GetPrivateProfileStringA`의 기본값으로 넘겨 판별한다.
* **경로 선택.** `Initialize(nullptr)` 또는 빈 문자열이면 전부 기본값으로 성공한다. `turntables.step`도 파일이 없으면 기본 4다.
* **주입 런타임.** port read 트랩에서 경로 요구를 없앴다. 진단 로그에 `source=default` 또는 `source=file`을 남긴다.
* 테스트용으로 바인딩 결과를 읽는 const 접근자를 두 클래스에 추가했다.

### 검증

* Windows x86 Debug 빌드: 오류·경고 0건.
* 단위 테스트: `checks: 1821, failures: 0`. 키보드 테스트는 EZ2DJ 70건, EZ2Dancer 51건.
* `ctest`: 5/5 통과(원래 멈추는 `re2dj_windows_vfs_runtime_probe` 제외).
* **드리프트 감지 확인.** 내장 기본값 하나를 `Z`에서 `Y`로 바꾸자 "기본값 = 예제 INI" 검사가 실패했고(`failures: 1`), 되돌리니 통과했다. 코드와 예제 파일이 어긋나면 테스트가 잡는다.
* 실행: `--io-config` 없이 `ez2dj3rd`를 실행해 런타임 로그에 `io-config:profile=ez2dj:source=default:status=initialized`가 남는 것을 확인했고, 사용자가 키 입력 동작을 확인했다.

### 남은 것

* 부분 INI를 쓰던 사용자는 동작이 달라진다. 적지 않은 항목이 바인딩 해제에서 기본값으로 바뀌므로, 끄려면 `NONE`을 적어야 한다. 문서에 적었다.

## English

### Implementation

Default keys live in each input class's binding table as **names** rather than values, so a default passes through the same interpretation as an INI value; that interpretation is now exposed as `ParseKeyboardKeyName`. `ReadKeyboardKeyBinding` reports through `bool* present` whether the file held the entry, so a missing entry keeps its default and only an entry written as `NONE` unbinds; presence is detected by passing a marker no key name can produce as `GetPrivateProfileStringA`'s default. `Initialize(nullptr)` or an empty path succeeds with every default in place, including `turntables.step` at 4. In the injected runtime the port-read trap no longer requires a path and records `source=default` or `source=file` in the diagnostic log. Both classes gained const accessors so tests can read the resulting bindings.

### Verification

The Windows x86 Debug build is clean; unit tests report `checks: 1821, failures: 0`, with 70 checks for EZ2DJ and 51 for EZ2Dancer keyboard input, and `ctest` passes 5/5 excluding the known hanging `re2dj_windows_vfs_runtime_probe`. **Drift detection was checked**: changing one built-in default from `Z` to `Y` failed the "defaults equal the example INI" check (`failures: 1`), and restoring it passed, so code and example cannot diverge unnoticed. A run of `ez2dj3rd` **without** `--io-config` logged `io-config:profile=ez2dj:source=default:status=initialized`, and the user confirmed the keys work.

### Remaining

Anyone who used a partial INI sees a behavior change: entries it omits now take the built-in default instead of being unbound, so unbinding requires writing `NONE`. This is stated in the documents.
