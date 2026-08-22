# 보호 실행 파일 fault reference scan

## 목적

protected `ez2dj.exe`가 illegal instruction으로 멈춘 순간, fault target 또는 그 page base를 저장한 메모리 위치를 찾는다. instruction single-step은 보호 stub의 긴 루프와 debugger overhead 때문에 fault까지 도달하지 못했으므로, fault 시점의 정지된 process memory를 한 번만 조사한다.

## 설계

`--break-exit-process --scan-fault-references`는 기존 `ExitProcess` observation trace를 사용한다. `EXCEPTION_ILLEGAL_INSTRUCTION`을 받으면 launcher는 child를 계속하기 전에 `VirtualQueryEx`로 committed `MEM_PRIVATE`와 `MEM_IMAGE` region을 순회한다. 읽을 수 있는 region을 bounded block으로 읽고 다음 두 32-bit little-endian 값을 검색한다.

1. fault instruction address
2. fault instruction address의 page-aligned base

발견된 위치·값·해당 region 속성은 JSONL에 남긴다. 결과 수에는 상한을 두어 손상되었거나 우연히 많은 값이 존재하는 memory에서 로그가 무한히 커지지 않게 한다.

```mermaid
flowchart LR
    E[illegal-instruction debug event] --> F[fault target과 page base 결정]
    F --> Q[committed private/image region 순회]
    Q --> S[32-bit reference 검색]
    S --> L[JSONL candidate records]
    L --> N[다음 caller/data-watch 분석 후보]
```

## 해석 경계

발견된 참조는 target을 저장한 위치의 후보일 뿐, 해당 값이 indirect jump/call에 실제로 사용됐다는 증명은 아니다. 참조가 없다고 해서 register 계산이나 unscanned protection region을 배제할 수 없다. 모든 결과는 confirmed observation과 inferred caller candidate로 구분한다.

## 검증

Windows x86 Debug build 및 CTest를 실행한다. canonical `ez2dj.exe`를 software entry breakpoint, ExitProcess breakpoint, reference scan과 함께 실행하고 `0xC000001D` fault에서 scan summary와 candidate records가 생성되는지 확인한다.

---

# Protected Executable Fault Reference Scan

## Purpose

At the moment protected `ez2dj.exe` stops on an illegal instruction, locate memory locations that store the fault target or its page base. Instruction single-stepping could not reach the fault because of the protected stub's long loop and debugger overhead, so this inspects the stopped process memory once at the fault.

## Design

`--break-exit-process --scan-fault-references` uses the existing `ExitProcess` observation trace. On `EXCEPTION_ILLEGAL_INSTRUCTION`, before continuing the child, the launcher walks committed `MEM_PRIVATE` and `MEM_IMAGE` regions with `VirtualQueryEx`. It reads accessible regions in bounded blocks and searches for two 32-bit little-endian values: the fault instruction address and its page-aligned base.

It records each found location, value, and containing region properties to JSONL. A result cap prevents unbounded logging from corrupted memory or coincidental matches.

## Interpretation boundary

A found reference is only a candidate location that stored the target; it does not prove that an indirect jump or call used it. No result cannot exclude register computation or an unscanned protection region. Record all results as confirmed observations and inferred caller candidates separately.

## Verification

Run the Windows x86 Debug build and CTest. Run canonical `ez2dj.exe` with the software-entry breakpoint, ExitProcess breakpoint, and reference scan, then verify scan summary and candidate records are produced at the `0xC000001D` fault.
