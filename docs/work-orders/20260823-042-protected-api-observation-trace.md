# 보호 stub API 관찰 trace 작업 지시

관련 설계: [보호 stub API 관찰 trace](../design/20260823-042-protected-api-observation-trace.md)

## 목표

protected `ez2dj.exe`의 post-entry Win32 API 사용과 caller를 관찰하고, illegal-instruction fault 컨텍스트를 강화해 invalid target으로 control을 넘긴 guest caller의 증거를 수집한다.

## 작업

1. launcher probe에 `--api-trace` 옵션을 추가한다. 기존 `--break-exit-process` 경로를 재사용하고 runtime 주입 없이 동작하게 한다.
2. `LOAD_DLL_DEBUG_EVENT`에서 `kernel32.dll`·`kernelbase.dll` base를 기록하고, child memory의 PE32 export directory를 해석하는 `remote_module_exports` 헬퍼를 추가한다. forwarded export는 건너뛴다.
3. 관찰 대상 API(`LoadLibraryA/W`, `GetProcAddress`, `VirtualAlloc`, `VirtualProtect`, `VirtualFree`, `GetVersion`, `GetVersionExA`, `CreateFileA`, `FreeLibrary`)에 software breakpoint를 설치하고, hit에서 caller·args·ANSI 문자열을 JSONL로 남긴 뒤 삼키고 재무장한다. `CreateFileA`는 첫 인자 문자열을 디코딩한다.
4. first-chance illegal-instruction에서 full register, 64 dword stack과 main image section 분류, page 128 byte dump, allocation region walk를 JSONL로 남긴다. 이 dump는 `--api-trace` 없이도 동작한다.
5. 확장: entry 이후 동적으로 적재된 모듈의 unload event에서 primary thread에 TF를 설정해 언로드 종반 구간을 single-step 수집하고, illegal instruction에서 history와 함께 기록한다.
6. Windows x86 build·CTest·canonical HDD 실행으로 검증하고, 결과와 해석 한계를 분석 문서와 작업 로그에 기록한다.

---

# Protected Stub API Observation Trace Work Order

Related design: [Protected Stub API Observation Trace](../design/20260823-042-protected-api-observation-trace.md)

## Goal

Observe the protected `ez2dj.exe` post-entry Win32 API usage and callers, and enrich the illegal-instruction fault context to collect evidence about the guest caller that transfers control to the invalid target.

## Tasks

1. Add `--api-trace` to the launcher probe, reusing the `--break-exit-process` path without runtime injection.
2. Record `kernel32.dll`/`kernelbase.dll` bases from `LOAD_DLL_DEBUG_EVENT` and add a `remote_module_exports` helper that parses the PE32 export directory from child memory, skipping forwarded exports.
3. Install software breakpoints on the watched APIs (`LoadLibraryA/W`, `GetProcAddress`, `VirtualAlloc`, `VirtualProtect`, `VirtualFree`, `GetVersion`, `GetVersionExA`, `CreateFileA`, `FreeLibrary`), record caller, args, and ANSI strings to JSONL on hits, then swallow and rearm. Decode the `CreateFileA` first-argument string as well.
4. On first-chance illegal instruction, record full registers, the 64-dword stack with main-image section classification, a 128-byte page dump, and an allocation region walk to JSONL. This dump also works without `--api-trace`.
5. Extension: on an unload event for a module loaded dynamically after entry, set TF on the primary thread and collect the unload-tail instructions single-stepped, recording the history at the illegal instruction.
6. Verify with the Windows x86 build, CTest, and canonical HDD execution, then record results and interpretation limits in the analysis document and work log.
