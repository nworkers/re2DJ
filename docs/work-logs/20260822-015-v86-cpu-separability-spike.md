# 작업 로그: v86 CPU 분리성 spike

## 결과

공식 v86 commit `847e34d5499b17b90d2783d5342ddd243c753497`을 임시 shallow checkout으로 조사했다. re2DJ에는 v86 소스나 binary를 추가하지 않았다.

- CPU-only Cargo feature 또는 build target이 없다.
- CPU memory는 APIC, IOAPIC, VGA와 Wasm MMIO import를 직접 사용하고, main run loop는 browser timer와 IRQ를 처리한다.
- 기본 synthetic gate `0xF0000000`은 configured RAM 밖이며 v86에서는 mapped/MMIO로 처리된다. instruction fetch/JIT는 이 주소를 실행 코드로 취급하지 않는다.
- 선택 EIP에서 실행을 멈추고 host로 반환하는 hook이 없다. 구현하려면 interpreter와 JIT codegen 양쪽을 변경해야 한다.
- CPU 상태와 JIT/TLB는 Wasm linear memory의 고정 offset 및 전역 상태에 있어 독립 guest context에 바로 맞지 않는다.
- v86 BSD-2-Clause, SoftFloat BSD-3-Clause, zstd BSD 선택 가능으로 라이선스는 차단 사유가 아니다.

따라서 v86은 PC 모델과 실행 제어를 큰 규모로 fork하지 않고는 re2DJ `ExecutionBackend`에 연결할 수 없어 제한된 재사용 후보에서 제외했다. 직접 인터프리터는 사용자 결정에 따라 계속 후순위다.

## 검증

- 임시 checkout commit ID: `847e34d5499b17b90d2783d5342ddd243c753497`
- `LICENSE`, `lib/softfloat/softfloat.c`, `lib/zstd/zstddeclib.c`의 라이선스 고지를 직접 확인했다.
- `src/rust/cpu/memory.rs`, `src/rust/cpu/cpu.rs`, `src/rust/cpu/global_pointers.rs`, `src/cpu.js`의 build·memory·run-loop 연결을 확인했다.
- `cargo check --target wasm32-unknown-unknown`는 호스트에 Cargo가 설치되지 않아 실행하지 못했다. 이 실패는 소스 구조 판단과 무관하다.
- re2DJ 변경은 문서뿐이며 v86 파일·원본 게임 자산을 포함하지 않는다.

---

# Work Log: v86 CPU Separability Spike

## Result

Official v86 commit `847e34d5499b17b90d2783d5342ddd243c753497` was inspected through a temporary shallow checkout. No v86 source or binary was added to re2DJ.

- There is no CPU-only Cargo feature or build target.
- CPU memory directly uses APIC, IOAPIC, VGA, and Wasm MMIO imports, and the main run loop processes browser timers and IRQs.
- The default synthetic gate, `0xF0000000`, lies outside configured RAM and is treated as mapped/MMIO. Instruction fetch and JIT do not accept it as executable code.
- No hook stops at a selected EIP and returns to the host. Adding one requires changes to both interpreter and JIT code generation.
- CPU and JIT/TLB state use fixed Wasm-linear-memory offsets and global state, which does not directly match independent guest contexts.
- v86 is BSD-2-Clause, SoftFloat is BSD-3-Clause, and zstd has a BSD option, so licensing is not the blocker.

v86 is excluded from bounded reuse candidates because it cannot connect to re2DJ `ExecutionBackend` without a substantial fork of PC-model and execution-control code. The custom interpreter remains deferred according to the user's decision.

## Verification

- Temporary checkout commit: `847e34d5499b17b90d2783d5342ddd243c753497`
- Directly checked license notices in `LICENSE`, `lib/softfloat/softfloat.c`, and `lib/zstd/zstddeclib.c`.
- Inspected build, memory, and run-loop connections in `src/rust/cpu/memory.rs`, `src/rust/cpu/cpu.rs`, `src/rust/cpu/global_pointers.rs`, and `src/cpu.js`.
- `cargo check --target wasm32-unknown-unknown` could not run because Cargo is not installed on the host. This does not affect the source-structure conclusion.
- re2DJ changes are documentation only and include no v86 file or original game asset.
