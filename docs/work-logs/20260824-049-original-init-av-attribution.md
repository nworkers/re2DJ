# 원본 초기화 간접 호출 AV 귀속 작업 로그

관련 설계: [원본 초기화 간접 호출 AV 귀속](../design/20260824-049-original-init-av-attribution.md)  
관련 작업 지시: [작업 지시 049](../work-orders/20260824-049-original-init-av-attribution.md)

## 결과

`0x19d521bd` access violation을 원본 initializer 배열의 손상된 첫 slot 호출로 확정했다. `GetProcAddress("IsProcessorFeaturePresent")`는 로그상 마지막 API였을 뿐 직접 원인이 아니다.

두 canonical mock-on 실행 모두 다음 상태를 기록했다.

* access kind `execute`, target/EIP `0x19d521bd`
* `EAX=ECX=EDX=0x0045c008`
* `[0x0045c008]=0x19d521bd`
* ESP `0x001afee4`, 첫 return `0x0043b688`, caller `0x0043b4c1`
* return 주변 bytes `...8b5508ff12 8b4508...` — `call dword ptr [edx]` 뒤 다음 명령

## 정적 대조

비보호 동형 빌드 `ez2dj1.exe`의 정적 코드는 다음 구조다.

```text
0x0043b670  initializer 범위 순회 시작
0x0043b67e  cmp dword ptr [ecx], 0
0x0043b683  call dword ptr [edx]
0x0043b688  다음 slot으로 진행
```

caller `0x0043b4a0`은 범위 `[0x0045c008, 0x0045c014)`를 넘긴다. 비보호 파일의 정상 dword는 `0x0045c000`부터 `{0, 0, 0, 0x0043c600, 0x0044e710, 0, 0, 0x0043c730}`이다.

canonical mock-on runtime은 두 실행 모두 같은 위치에서 `{0xb9f5c1dd, 0x69e5f14d, 0x19d521bd, 0xc908172d, 0x79f968ad, 0x29a5b10d, 0xd995e17d, 0x89c8d81d}`를 기록했다. 따라서 첫 initializer sentinel이어야 할 `0x0045c008`이 nonzero invalid target으로 남았다. 반면 `0x0043b680` 주변 `.text` bytes는 비보호 빌드와 일치했다. 관찰 범위 안에서 `.text` 호출부는 복원됐지만 initializer `.data`는 정상 복원되지 않았다.

## 구현

`--api-trace` first-chance access-violation diagnostic에 다음을 추가했다.

1. exception information의 read/write/execute kind와 대상 주소
2. x86 integer/control register 전체
3. main image를 가리키는 register마다 image 경계 안의 bounded 8-dword window
4. stack 상위 main-image 주소마다 bounded 24-byte code window와 section 이름

진단은 target 값을 수정하거나 예외를 삼키지 않는다.

## 검증

Windows x86 Debug build와 CTest 2/2가 통과했다. 비교 로그는 다음과 같다.

* `20260824-004846-047.jsonl`
* `20260824-004930-990.jsonl`

두 로그의 AV target, EAX/ECX/EDX, `.data` 8 dword, return code window가 동일했다. 로그와 문서에는 원본 파일 raw dump를 추가하지 않았고 필요한 구조·주소·관찰 값만 기록했다.

## 해석 경계와 다음 단계

확인됨: AV의 직접 원인은 손상된 initializer slot이다. 제한된 `.text` 호출부는 정상 코드와 일치하고 `.data` initializer 영역은 정상 값과 다르다.

추정: synthetic handle로 수행한 IOCTL `0x9c406410`·`0x9c406414`가 host에서 실패하거나 출력 데이터를 제공하지 않아 `.data` 복원 키/단계가 완성되지 않았을 가능성이 높다. 아직 IOCTL 반환값과 output buffer 사용을 수집하지 않았으므로 직접 인과로 확정하지 않는다.

다음 작업은 IOCTL 8개 인자, 입출력 buffer 전후, 반환값을 관찰해 최소 디바이스 응답을 결정하는 것이다.

---

# Original-Initialization Indirect-Call AV Attribution Work Log

Related design: [Original-Initialization Indirect-Call AV Attribution](../design/20260824-049-original-init-av-attribution.md)  
Related work order: [Work Order 049](../work-orders/20260824-049-original-init-av-attribution.md)

## Result

The execute access violation at `0x19d521bd` is confirmed as a call through the corrupt first slot of the original initializer array. `GetProcAddress("IsProcessorFeaturePresent")` was only the last logged API, not the direct cause.

Both canonical mock-on runs recorded execute target/EIP `0x19d521bd`, `EAX=ECX=EDX=0x0045c008`, `[0x0045c008]=0x19d521bd`, ESP `0x001afee4`, return `0x0043b688`, and caller `0x0043b4c1`. Runtime bytes around the return contain `call dword ptr [edx]` immediately before it.

## Static comparison

Sibling unprotected `ez2dj1.exe` has an initializer walker at `0x0043b670`; `0x0043b683` calls each nonzero dword in `[0x0045c008, 0x0045c014)`. Its normal array begins `{0, 0, 0, 0x0043c600, 0x0044e710, 0, 0, 0x0043c730}` at `0x0045c000`.

Both canonical runtime runs instead hold `{0xb9f5c1dd, 0x69e5f14d, 0x19d521bd, 0xc908172d, 0x79f968ad, 0x29a5b10d, 0xd995e17d, 0x89c8d81d}`. The limited `.text` call site matches the unprotected build, while initializer `.data` is not normally restored.

## Implementation and verification

First-chance AV diagnostics now record access kind/address, full x86 registers, bounded eight-dword windows around main-image register pointers, and bounded code windows around main-image stack addresses. They do not modify target state or swallow the exception.

Windows x86 Debug build and CTest 2/2 passed. Evidence logs are `20260824-004846-047.jsonl` and `20260824-004930-990.jsonl`; both carry identical attribution state. No original-file raw dump was added.

## Interpretation boundary and next step

Confirmed: the direct AV cause is a corrupt initializer slot; the limited `.text` caller is correct while the initializer `.data` region is not.

Inferred: host-failed IOCTLs `0x9c406410` and `0x9c406414` on the synthetic handle may withhold data or a key needed to restore `.data`. Return values and output-buffer use have not yet been captured, so direct causality is not confirmed. The next task captures all eight IOCTL arguments, buffers before/after, and the return value to define the minimal device response.
