# 병렬포트 디바이스 검사 HLE 계획 작업 로그

관련 작업 지시: [병렬포트 디바이스 검사 HLE 계획 작업 지시](../work-orders/20260823-047-lptdi-device-hle-plan.md)  
관련 설계: [병렬포트 디바이스 검사 HLE 계획](../design/20260823-047-lptdi-device-hle-plan.md)

## 구현 결과 (Phase A)

launcher probe의 watched API에 `DeviceIoControl`, `ReadFile`, `WriteFile`, `CloseHandle`을 추가했다. 기존 4-인자 기록 형식이라 `DeviceIoControl`의 IOCTL 코드(두 번째 인자)와 `ReadFile`의 요청 바이트 수가 그대로 수집된다.

## 검증 결과

Windows x86 Debug build 성공, CTest 2/2 통과. canonical 실행(logs/…/122344-248.jsonl, fault 도달 100,538 step)에서 전체 post-entry API 기록은 다음뿐이었다.

| API | 논리 호출 수 | caller |
| --- | --- | --- |
| GetVersion | 2 | `0x01ed49d9`, `0x01ed2582` |
| CreateFileA(`\\.\LPTDI1`) | 1 | `0x01ed41f1` |
| LoadLibraryA("WSOCK32.DLL") | 1 | `0x01ed2599` |
| GetProcAddress | 3(게스트 1 + 시스템 2) | 게스트: `WSAGetLastError` |
| FreeLibrary | 1 | `0x01ed25c9` |
| **DeviceIoControl / ReadFile / WriteFile / CloseHandle** | **0** | — |

### 확인됨: 개방 이후 디바이스 핸들 사용이 전혀 없다

`CreateFileA("\\.\LPTDI1")` 직후 스텁은 핸들을 건드리지 않고 곧장 GetVersion → Winsock 프로브로 넘어간다. 읽기도 IOCTL도 닫기도 없다. 즉 개방 자체가 TDSD 병렬포트 I/O 보드 드라이버의 **존재 확인**이다.

개방 반환 값은 직접 포획하지 않았다. 실패 판정은 덤프 루트에 이름이 바뀐 `Tdsd.vxd111`(비활성 드라이버)만 있고 후속 사용이 전무하다는 사실에 근거한 추정이다.

## 결론과 Phase B 함의

검사는 "드라이버가 있어서 개방되는가" 하나만 볼 가능성이 높다. 따라서 HLE도 성공처럼 보이는 가상 핸들 반환으로 충분할 수 있다. 단, **성공 경로는 지금까지 한 번도 실행된 적이 없으므로** 모킹 첫 실행이 곧 성공 경로의 첫 관찰이 된다 — 성공 시 비로소 ReadFile·DeviceIoControl이 나타날 수 있고, 그때 Phase A watch list가 이미 준비돼 있다. Phase B 구현(work-order 047의 다음 항목)에서 모킹 on/off 비교와 함께 확인한다.

---

# Parallel-Port Device Check HLE Plan Work Log

Related work order: [Parallel-Port Device Check HLE Plan Work Order](../work-orders/20260823-047-lptdi-device-hle-plan.md)  
Related design: [Parallel-Port Device Check HLE Plan](../design/20260823-047-lptdi-device-hle-plan.md)

## Implementation result (Phase A)

Extended the launcher probe watch list with `DeviceIoControl`, `ReadFile`, `WriteFile`, and `CloseHandle`; the existing four-argument logging already captures IOCTL codes and requested byte counts.

## Verification result

Build succeeded, CTest passed 2/2. The canonical run's entire post-entry API surface was: GetVersion ×2 logical, CreateFileA(`\\.\LPTDI1`) ×1, LoadLibraryA("WSOCK32.DLL") ×1, GetProcAddress ×3 (one guest call for WSAGetLastError), FreeLibrary ×1 — and zero device-handle usage of any kind.

## Conclusion

The open itself is the check — a pure driver-presence probe for the TDSD parallel-port board; the stub never reads or controls the device afterward. Failure is inferred (absent driver, renamed-off `Tdsd.vxd111`, no follow-up usage) since the return value was not captured directly. For HLE this means a successful-looking synthetic open may satisfy everything, but the success path has never executed — so the first mocking run doubles as its first observation, with the extended watch list already in place.
