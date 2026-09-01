# 작업 로그 123 — ez2dj4th EIP=0 fault 호출 대상 귀속

## 결과

`RecordAccessViolationContext`에 fault stack return address 직전의 bounded
x86 call attribution을 추가했습니다. `E8 rel32`, `FF 15 addr32`, register
indirect `FF /2`만 제한적으로 관찰하며, absolute memory-indirect call은
pointer slot의 현재 값과 target memory query를 기록합니다. 코드는
first-chance fault에서 child memory를 읽기만 하며 원본 context, code, slot을
변경하지 않습니다.

실제 `4thTrax.chd` 실행에서 다음 event를 확인했습니다.

```text
{"event":"av_indirect_call","stack_index":0,"return_address":"0x00aef7fe","call_address":"0x00aef7f8","kind":"absolute_memory","bytes":"ff15f40caf0083c4","register":"","slot":"0x00af0cf4","slot_readable":true,"target":"0x00000000","target_region_readable":false,"target_region":"0x00000000","target_allocation":"0x00000000","target_protect":"0x00000000","target_state":"0x00000000","target_type":"0x00000000","target_section":"","target_symbol":""}
```

이 결과는 `CALL DWORD PTR [0x00AF0CF4]`가 zero target을 사용한 뒤
`EIP=0x00000000`, `0xc0000005` execute fault로 이어졌음을 **확인**합니다.
이는 `CreateFileA` 동적 반환값의 ABI가 정상이라는 증거가 아니며, slot이
왜 0으로 남았는지와 그 이전 protected continuation은 여전히
**미확정**입니다. 정상 VFS wrapper request와 protection response도
관찰되지 않았습니다.

## 검증

* 브랜치: `task-113-ez2dj4th-chd`
* Windows x86 Debug launcher build: 통과
* Unit tests: `checks: 999, failures: 0`
* Product-loader probe: `windows-product-loader-probe: profile-defaults=ok unsupported-target=ok`
* 실제 CHD bounded trace:

```text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
```

진단 로그는
`logs/windows_x86_launcher_probe/ez2dj4th/20260901-111821-687.jsonl`이며,
원본 실행 파일·CHD·HDD 자산은 저장소에 추가하지 않았습니다.

## English

## Result

The bounded x86 call attribution was added to `RecordAccessViolationContext`.
It conservatively observes only `E8 rel32`, `FF 15 addr32`, and register
indirect `FF /2`. Absolute memory-indirect calls record the current pointer-slot
value and target memory query. The code reads child memory at the first-chance
fault and does not change the original context, code, or slot.

The real `4thTrax.chd` run produced:

```text
{"event":"av_indirect_call","stack_index":0,"return_address":"0x00aef7fe","call_address":"0x00aef7f8","kind":"absolute_memory","bytes":"ff15f40caf0083c4","register":"","slot":"0x00af0cf4","slot_readable":true,"target":"0x00000000","target_region_readable":false,"target_region":"0x00000000","target_allocation":"0x00000000","target_protect":"0x00000000","target_state":"0x00000000","target_type":"0x00000000","target_section":"","target_symbol":""}
```

This confirms that `CALL DWORD PTR [0x00AF0CF4]` used a zero target before the
child reached `EIP=0` and an `0xc0000005` execute fault. It does not establish
that the dynamic `CreateFileA` return ABI is correct. Why the slot remains zero
and what protected continuation precedes it remain unresolved. No normal VFS
wrapper request or protection response was observed.

## Verification

* Branch: `task-113-ez2dj4th-chd`
* Windows x86 Debug launcher build: passed
* Unit tests: `checks: 999, failures: 0`
* Product-loader probe: `windows-product-loader-probe: profile-defaults=ok unsupported-target=ok`
* Real CHD bounded trace:

```text
build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd C:\Users\nworkers\AppData\Local\Temp\re2dj\chd\ez2dj4th --chd C:\workspace\git\re2DJ\roms\ez2dj4th\4thTrax.chd --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE --hle-vfs --api-trace --trace
```

The diagnostic log is
`logs/windows_x86_launcher_probe/ez2dj4th/20260901-111821-687.jsonl`.
The original executable, CHD, and HDD assets were not added to the repository.
