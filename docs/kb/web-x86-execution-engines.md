# Web x86 실행 엔진 후보

## 조사 범위

2026년 8월 22일 기준으로, 브라우저에서 IA-32 코드를 실행할 수 있거나 CPU 코어로 재사용할 가능성이 있는 프로젝트를 공식 저장소·공식 문서·라이선스 원문으로 비교했다. 이 문서는 기술 후보 조사이며 어떤 프로젝트도 re2DJ 의존성으로 도입하지 않는다.

## 후보 비교

| 후보 | 라이선스 | Web/IA-32 근거 | 프로젝트 적합성 | 판단 |
| --- | --- | --- | --- | --- |
| [v86](https://github.com/copy/v86) | [BSD-2-Clause](https://github.com/copy/v86/blob/master/LICENSE) | 브라우저용 x86-to-Wasm JIT와 인터프리터가 있고 Pentium 4 수준, SSE3, FPU를 명시한다. [동작 설명](https://github.com/copy/v86/blob/master/docs/how-it-works.md)은 생성된 Wasm 모듈에서 메인 루프로 빠져나오는 경계도 설명한다. | CPU-only build 경계가 없고 CPU memory·run loop가 PC 장치와 browser service에 결합되어 있으며 기본 gate 주소도 실행 불가 MMIO 영역이다. | **spike 결과 부적합**. 채택 중단. |
| [TinyEMU](https://bellard.org/tinyemu/) / [JSLinux](https://bellard.org/jslinux/) | TinyEMU는 MIT | TinyEMU 공식 페이지는 x86 system emulator와 JavaScript 버전을 설명하지만 네이티브 x86 경로를 KVM 기반으로 명시한다. [JSLinux 기술 문서](https://bellard.org/jslinux/tech)는 현재 x86 브라우저 엔진이 C에서 Emscripten으로 변환된다고 설명한다. | 공식 다운로드에서 현재 Web x86 CPU 구현을 재사용 가능한 소스 경계로 확인하지 못했다. TinyEMU의 공개성과 JSLinux에서 실행되는 x86 엔진의 공개 범위를 같은 것으로 가정할 수 없다. | **보류**. 재현 가능한 x86 Web 소스와 라이선스 범위가 확인될 때 재평가한다. |
| [libx86emu](https://github.com/wfeldt/libx86emu) | [허용형 X11 계열](https://github.com/wfeldt/libx86emu/blob/master/LICENSE) | 작은 재진입 가능 x86 라이브러리이며 메모리·I/O·명령 hook과 기본 보호 모드를 제공한다. | 공식 README가 FPU, MMX와 기타 확장 명령을 지원하지 않는다고 명시한다. Web 빌드 근거도 없다. 현재 게임 실행 엔진으로는 기능 범위가 부족하다. | **참고 후보**. import gate 구조 참고 외에는 채택하지 않는다. |
| [Blink](https://github.com/jart/blink) | ISC | i386, x87, SSE2를 포함하지만 주 실행 목표는 x86-64 Linux 사용자 공간 바이너리와 Linux syscall이다. 공식 지원 호스트에 WebAssembly가 없다. | Win32 IA-32 메모리·호출 경계를 제공하는 Web backend로 쓰려면 제품 중심 구조를 크게 바꿔야 한다. | **제외**. 라이선스는 맞지만 실행 모델과 호스트가 맞지 않는다. |

## 라이선스로 제외한 프로젝트

- [QEMU](https://github.com/qemu/qemu/blob/master/LICENSE)는 전체 emulator가 GPL-2.0이다. 일부 TCG 파일이 BSD/MIT여도 파일별 라이선스와 결합 관계가 섞여 있어 QEMU/TCG 통합 후보로 취급하지 않는다.
- [Unicorn](https://github.com/unicorn-engine/unicorn)은 GPL-2.0이며 QEMU 코드를 포함한다.
- [Bochs](https://github.com/bochs-emu/Bochs)는 LGPL-2.1이다.
- [Halfix](https://github.com/nepx/halfix)는 WebAssembly와 분리 가능한 CPU 가능성을 설명하지만 GPL-3.0이다.
- [MAME](https://github.com/mamedev/mame/blob/master/COPYING)는 개별 BSD 파일이 있어도 프로젝트 전체가 GPL-2.0이다.

이 프로젝트들은 공개 문서로 동작 방식을 참고할 수 있지만 소스나 실행 코어를 re2DJ에 통합하지 않는다.

## 결정

현재 후보 가운데 제품 의존성으로 바로 채택할 엔진은 없다. v86은 전체 PC를 붙이지 않고 다음 질문만 답하는 격리된 spike로 추가 검증했다.

1. CPU·메모리·실행 루프에 필요한 최소 소스 집합을 PC 장치 모델 없이 빌드할 수 있는가.
2. 호스트가 EIP, 범용 레지스터와 평탄한 게스트 메모리를 설정할 수 있는가.
3. synthetic import gate 주소에서 실행을 반환하고 `ExecutionBackend::CompleteImport` 결과로 재개할 수 있는가.
4. 필요한 최소 소스와 전이 구성요소가 모두 허용 라이선스인가.
5. 한 페이지를 수정한 뒤 번역 캐시를 무효화할 수 있어 보호된 빌드의 self-modifying code를 지원할 수 있는가.

### v86 CPU 분리성 spike 결과 — 2026-08-22

**확인됨:** 공식 v86 commit `847e34d5499b17b90d2783d5342ddd243c753497`의 Rust crate에는 CPU-only feature 또는 build target이 없다. `src/rust/cpu/memory.rs`는 APIC, IOAPIC, VGA와 Wasm `mmap_*` import를 직접 참조하며, `cpu.rs`의 `main_loop()`은 browser timer와 IRQ 처리를 호출한다. 따라서 현재 공개 build는 CPU·메모리·실행 루프만 독립 빌드하는 경계를 제공하지 않는다.

**확인됨:** re2DJ의 기본 synthetic gate 영역 `0xF0000000`은 v86의 일반 RAM 범위 밖이다. v86은 물리 주소가 `memory_size` 이상이면 mapped/MMIO 범위로 처리하고, instruction fetch·JIT 경로는 그 범위를 실행 가능 코드로 허용하지 않는다. 별도의 gate memory model 또는 gate trap을 추가하지 않으면 기존 import thunk 주소로 진입할 수 없다.

**확인됨:** `cycle_internal()`과 JIT 경로에는 특정 EIP에서 host로 반환하는 공개 stop hook이 없다. 이를 추가하려면 interpreter와 생성 Wasm code 경로 모두에 gate 검사를 넣어야 한다. CPU 상태도 Wasm linear memory의 고정 offset과 전역 JIT/TLB 상태에 놓여 있어 re2DJ의 독립 guest context 경계와 바로 맞지 않는다.

**확인됨:** 라이선스 자체는 차단 사유가 아니다. v86은 BSD-2-Clause, CPU의 x87 경로가 쓰는 SoftFloat은 BSD-3-Clause, 상태 압축 모듈 zstd는 BSD 또는 GPLv2 선택 제공이다. 다만 실제 도입 시 필요한 파일만 다시 확정하고 BSD 선택 고지를 포함해 재감사해야 한다.

**판단:** v86은 이 프로젝트가 허용한 제한된 spike 범위에서는 **부적합**이다. PC 장치·메모리 모델·실행 제어를 분리하는 대규모 fork 없이는 `ExecutionBackend`로 연결할 수 없다. v86 채택을 중단한다. TinyEMU 계열은 현재 Web x86 소스 공개 범위가 확인될 때만 다시 평가하며, 직접 인터프리터는 사용자의 기존 결정에 따라 계속 후순위로 둔다.

---

# Web x86 Execution Engine Candidates

## Research Scope

As of August 22, 2026, projects capable of running IA-32 code in a browser, or potentially reusable as CPU cores, were compared using official repositories, official documentation, and original license texts. This is a technical candidate survey; none of these projects is introduced as a re2DJ dependency.

## Candidate Comparison

| Candidate | License | Web/IA-32 evidence | Project fit | Decision |
| --- | --- | --- | --- | --- |
| [v86](https://github.com/copy/v86) | [BSD-2-Clause](https://github.com/copy/v86/blob/master/LICENSE) | It provides a browser x86-to-Wasm JIT and interpreter and documents a roughly Pentium 4 instruction set, SSE3, and an FPU. Its [implementation notes](https://github.com/copy/v86/blob/master/docs/how-it-works.md) also describe exits from generated Wasm modules to the main loop. | It lacks a CPU-only build boundary, couples CPU memory/run control to PC devices and browser services, and treats the default gate address as non-executable MMIO. | **Unsuitable after the spike**. Adoption stopped. |
| [TinyEMU](https://bellard.org/tinyemu/) / [JSLinux](https://bellard.org/jslinux/) | TinyEMU is MIT | The TinyEMU site describes an x86 system emulator and JavaScript version but identifies the native x86 path as KVM-based. [JSLinux technical notes](https://bellard.org/jslinux/tech) say the current browser x86 engine is C compiled through Emscripten. | The official downloads did not establish a reusable source boundary for the current Web x86 implementation. TinyEMU's published source and the x86 engine running in JSLinux cannot be assumed to have identical publication scope. | **Deferred** until reproducible Web x86 source and its license scope are confirmed. |
| [libx86emu](https://github.com/wfeldt/libx86emu) | [Permissive X11-style](https://github.com/wfeldt/libx86emu/blob/master/LICENSE) | It is a small, re-entrant x86 library with memory, I/O, and instruction hooks plus basic protected mode. | Its README explicitly excludes FPU, MMX, and other instruction extensions, and provides no Web build evidence. Its coverage is insufficient for the current game execution engine. | **Reference only**, not an adoption candidate beyond possible import-gate design ideas. |
| [Blink](https://github.com/jart/blink) | ISC | It includes i386, x87, and SSE2, but primarily targets x86-64 Linux userspace binaries and Linux system calls. WebAssembly is not an officially listed host. | Turning it into a Web backend exposing Win32 IA-32 memory and call boundaries would require major changes to its product-centered architecture. | **Excluded**. The license fits, but the execution model and host do not. |

## Projects Excluded by License

- [QEMU](https://github.com/qemu/qemu/blob/master/LICENSE) is GPL-2.0 as a whole. Even though some TCG files use BSD/MIT terms, mixed per-file licensing and coupling prevent treating QEMU/TCG as an integration candidate.
- [Unicorn](https://github.com/unicorn-engine/unicorn) is GPL-2.0 and includes QEMU code.
- [Bochs](https://github.com/bochs-emu/Bochs) is LGPL-2.1.
- [Halfix](https://github.com/nepx/halfix) documents WebAssembly operation and possible CPU isolation but is GPL-3.0.
- [MAME](https://github.com/mamedev/mame/blob/master/COPYING) is GPL-2.0 as a whole even though individual files may use BSD terms.

Their public documentation may be consulted, but their source or execution cores are not integrated into re2DJ.

## Decision

No current candidate is ready for immediate adoption as a product dependency. v86 was additionally checked in an isolated spike rather than through full-PC integration.

1. Can the minimum CPU, memory, and execution-loop source set build without PC device models?
2. Can the host set EIP, general registers, and flat guest memory?
3. Can execution return at a synthetic import-gate address and resume with an `ExecutionBackend::CompleteImport` result?
4. Are all required source files and transitive components under permitted licenses?
5. Can translation state be invalidated after a page write to support self-modifying code in protected builds?

### v86 CPU Separability Spike Result — 2026-08-22

**Confirmed:** The official v86 commit `847e34d5499b17b90d2783d5342ddd243c753497` has no CPU-only Cargo feature or build target. `src/rust/cpu/memory.rs` directly references APIC, IOAPIC, VGA, and Wasm `mmap_*` imports, while `cpu.rs` `main_loop()` calls browser timers and IRQ handling. The published build therefore provides no boundary that builds only CPU, memory, and execution-loop code.

**Confirmed:** re2DJ's default synthetic gate region, `0xF0000000`, lies outside v86's normal RAM. v86 treats physical addresses at or above `memory_size` as mapped/MMIO and its instruction-fetch and JIT paths do not allow that range as executable code. The current import-thunk address cannot be entered without a separate gate-memory model or a gate trap.

**Confirmed:** `cycle_internal()` and its JIT path expose no stop hook that returns to the host at a chosen EIP. Adding one requires gate checks in both the interpreter and generated Wasm-code paths. CPU state is also held at fixed Wasm-linear-memory offsets with global JIT/TLB state, which does not directly match re2DJ's independent guest-context boundary.

**Confirmed:** Licensing is not the blocking reason. v86 is BSD-2-Clause, SoftFloat used by the CPU x87 path is BSD-3-Clause, and the zstd state-compression module offers BSD or GPLv2 at the user's option. An actual adoption would still need a fresh audit of only the files required and the BSD-choice notices.

**Decision:** v86 is **unsuitable** within this project's bounded-spike scope. Connecting it as an `ExecutionBackend` would require a substantial fork to separate PC devices, its memory model, and execution control. v86 adoption stops. A TinyEMU-family engine is reconsidered only if the publication scope of its current Web x86 source is confirmed. A custom interpreter remains deferred in accordance with the user's previous decision.
