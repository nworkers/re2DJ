# 20260904-174 EZ2DJ 4th I/O out helper 트랩 연결 결과
# 20260904-174 EZ2DJ 4th I/O Out Helper Trap Results

## 1. 개요 (Overview)

Task 173이 남긴 중단 지점을 해소했다.

**결론: ez2dj4th 프로필에 `legacy_io_out_byte_rva = 0x000c384b`을 설정하자 `out dx, al`이 HLE 포트 경로로 처리된다. 같은 실행에서 특권 명령 트랩이 4,913건 발생했고 그중 두 번째 기회로 넘어간 것은 0건이다. 실행은 텍스처 surface 생성 단계까지 더 진행한 뒤 `RVA 0x00009701`의 읽기 접근 위반(`0xc0000005`)으로 종료한다.**

Setting `legacy_io_out_byte_rva = 0x000c384b` routes `out dx, al` through the HLE port path: the run records 4,913 privileged-instruction traps with none reaching a second chance, advances into texture surface creation, and now stops at a read access violation at `RVA 0x00009701`.

---

## 2. 변경 내용 (Changes Implemented)

1. **프로필 설정.** `src/target/target_profile.cpp`의 ez2dj4th 항목에 `legacy_io_out_byte_rva = 0x000c384b`을 추가했다.
2. **검증 기대값 갱신.** `src/tools/windows_product_loader_probe/main.cpp`의 ez2dj4th 단언을 새 값으로 고쳤다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,253 checks, 0 failures.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.
- 진입 추적: `logs/windows_x86_launcher_probe/ez2dj4th/20260904-141951-723.jsonl`.

### 3.1 out 경로가 트랩된다 (확인됨)

| 지표 | Task 173 실행 | 이번 실행 |
| - | - | - |
| `privileged_instruction` 건수 | 11 | 4,913 |
| `first_chance:false` 건수 | 1 | **0** |
| `exit_process` 코드 | `0xc0000096` | `0xc0000005` |
| 디버그 이벤트 수 | 252 | 5,233 |

특권 명령이 4,913건으로 늘어난 것은 게스트가 이제 I/O 폴링 루프를 정상적으로 돌린다는 뜻이다.

### 3.2 실행이 텍스처 생성까지 진행한다 (확인됨)

`.ddraw.log`의 호출 분포다.

| 호출 | 횟수 |
| - | - |
| `IDirectDraw7::CreateSurface` | 26 |
| `IDirect3D7::CreateDevice` | 1 |
| `IDirect3D7::EnumZBufferFormats` | 2 |
| `IDirectDraw7::EnumSurfaces` | 1 |
| `IDirectDraw7::RestoreAllSurfaces` | 1 |

`caps=0x10005000` 128x128 surface가 반복 생성된다. 텍스처 적재 단계다. `RestoreAllSurfaces`와 `EnumSurfaces`도 처음 나타났다.

### 3.3 새 중단 지점은 초기화되지 않은 포인터 역참조다 (확인됨)

```
{"debug_event":"exception","code":"0xc0000005","address":"0x00409701"}
{"event":"av_access","kind":"read","address":"0xccccccd4"}
{"event":"av_registers","eax":"0xcccccccc","ecx":"0x00513b64","eip":"0x00409701"}
{"exception_bytes":"8b400889420c8b4dfc8b1183bad80100"}
```

- **확인됨 — faulting 명령은 `mov eax, [eax+8]`이고 `EAX`가 `0xcccccccc`다.** 이어지는 명령은 `mov [edx+0x0c], eax`이므로 객체 필드 하나를 다른 객체로 옮기는 자리다.
- **확인됨 — `0xcccccccc`는 MSVC 디버그 빌드의 초기화되지 않은 스택 채움 값이다.** 이 실행 파일의 다른 함수들도 프롤로그에서 지역을 `0xcccccccc`로 채운다(예: 드라이버 콜백의 `rep stosd`). 따라서 값이 없는 지역을 그대로 역참조한 것이다.
- **확인됨 — 함수는 `RVA 0x00009696`에서 시작하고 호출자는 `RVA 0x000658d7`의 직접 호출이다.** `av_indirect_call`이 `target 0x004023d8`(incremental link thunk)를 거쳐 `0x00409696`으로 이어짐을 기록했다.
- **미확정 — 어떤 값이 비어 있는가.** 비어 있는 지역의 출처와 그것을 채워야 할 HLE 응답은 아직 관측하지 않았다.

---

## 4. 가설 판정 (Hypothesis Verdicts)

| 가설 | 판정 |
| - | - |
| out helper RVA 누락이 `0xc0000096`의 원인이다 | **확인.** 설정 후 두 번째 기회 예외가 사라졌다 |
| 트랩 정책과 주입 런타임은 이미 out 경로를 다룬다 | **확인.** 프로필 값 외 코드 변경이 없었다 |

---

## 5. 다음 작업 (Next Task)

`RVA 0x00009696` 함수와 그 호출자 `RVA 0x000658d7` 구간을 덤프해 `EAX`가 어디서 오는지 복원한다. Task 172에서 추가한 코드 범위 덤프를 그대로 쓸 수 있다. 값이 비어 있는 이유가 우리 facade 응답이면 그 응답을 고치고, 게스트 내부 상태면 그 상태를 채우는 경로를 추적한다.

Dump `RVA 0x00009696` and its caller region at `RVA 0x000658d7` with the code-range facility Task 172 added, and recover where `EAX` comes from. If the empty value traces back to a facade response, fix that response; if it is guest-internal state, trace the path that should fill it.

---

## 6. 관련 문서 (Related Documents)

- [Task 174 설계](../design/20260904-174-ez2dj4th-io-out-helper.md)
- [Task 174 작업 지시서](../work-orders/20260904-174-ez2dj4th-io-out-helper.md)
- [Task 173 작업 로그](../work-logs/20260904-173-ez2dj4th-ddcaps2-windowed.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
