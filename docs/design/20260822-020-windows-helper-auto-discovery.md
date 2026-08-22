# Windows Helper Auto-Discovery

## 한국어

### 결정

Windows observer와 이후 Windows host는 `--helper`를 선택 override로만 유지합니다. 지정하지 않으면 host executable 위치에서 다음 순서로 helper를 찾습니다.

1. `<host-dir>/re2dj_native_ipc_helper.exe`
2. `<host-dir>/helpers/win32/re2dj_native_ipc_helper.exe`

둘 다 없으면 사용 가능한 기본 helper가 없다는 오류와 탐색 경로를 보고합니다. 이 정책은 helper가 re2DJ 배포물의 내부 구성요소이며, 원본 target profile과 독립적이라는 점을 반영합니다.

## English

### Decision

The Windows observer and later Windows hosts retain `--helper` only as an optional override. Without it, the host searches for the helper in this order relative to its own executable:

1. `<host-dir>/re2dj_native_ipc_helper.exe`
2. `<host-dir>/helpers/win32/re2dj_native_ipc_helper.exe`

If neither exists, it reports that no bundled default helper is available and lists the searched paths. This reflects that the helper is an internal re2DJ distribution component independent of the original target profile.
