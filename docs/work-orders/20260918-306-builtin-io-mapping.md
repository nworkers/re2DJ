# 작업 306 작업 지시 — 기본 키 매핑 내장과 `--io-config` 덮어쓰기 / Task 306 work order — Built-in key mapping with `--io-config` as an override

설계: [20260918-306-builtin-io-mapping.md](../design/20260918-306-builtin-io-mapping.md)

## 한국어

### 구현 범위

1. **기본값 내장.** `Ez2DjKeyboardInput`·`Ez2DancerKeyboardInput`의 바인딩 표에 기본 키 이름을 넣는다. 값은 현재 예제 INI 두 개와 같게 한다. 턴테이블 네 항목과 `step`도 포함한다.
2. **덮어쓰기 동작.** `ReadKeyboardKeyBinding`이 INI 항목의 존재 여부를 알려주도록 고치고, 항목이 없으면 기본값을 유지한다. `NONE`은 그 항목만 바인딩 해제, 알 수 없는 이름은 오류를 유지한다. `Initialize`는 경로 없이도 성공한다.
3. **주입 런타임.** port read 트랩이 경로 없이도 키보드 입력을 초기화·폴링하게 한다. 진단 상태 문자열에 기본값인지 파일인지 남긴다.
4. **테스트.** 두 게임 각각에 대해 기본값과 예제 INI 일치, 일부 INI의 부분 덮어쓰기, `NONE` 해제, 잘못된 키 이름 오류를 검사한다.
5. **문서.** 실행 가이드의 키 입력 절, `config/*.example.ini` 머리말, 분석·작업 로그, `IMPLEMENTED.md`.
6. **사용자 확인.** `--io-config` 없이 실행해 입력이 동작하는지, 예제 INI를 준 실행이 기존과 같은지.

### 범위에서 뺀 것

키 이름 문법 확장, 게임패드, 재매핑 UI, 프로파일별 기본 매핑, 예제 INI 삭제.

## English

### Scope

1. **Build the defaults in**: add default key names to the binding tables of `Ez2DjKeyboardInput` and `Ez2DancerKeyboardInput`, matching today's two example INIs, including the turntable entries and `step`.
2. **Override behavior**: have `ReadKeyboardKeyBinding` report whether the INI holds the entry, keep the default when it does not, keep `NONE` as unbinding that one entry and an unknown name as an error, and let `Initialize` succeed with no path.
3. **Injected runtime**: initialize and poll keyboard input even without a path, recording in the diagnostic status whether defaults or a file were used.
4. **Tests**, per game: defaults match the example INI, a partial INI overrides only its entries, `NONE` unbinds, and an unknown key name still errors.
5. **Documents**: the runtime guide's input section, headers in `config/*.example.ini`, analysis and work log, and `IMPLEMENTED.md`.
6. **User check**: input works with no `--io-config`, and a run with the example INI behaves as before.

### Out of scope

Key-name syntax extensions, gamepads, a remapping UI, per-profile default mappings, and removing the example INIs.
