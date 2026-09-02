# Hardlock 우회 스텁 작업 로그

관련 설계: [Hardlock 우회 스텁](../design/20260901-131-hardlock-bypass-stub.md)
관련 작업 지시: [Hardlock 우회 스텁](../work-orders/20260901-131-hardlock-bypass-stub.md)

*Related design: [Hardlock bypass stub](../design/20260901-131-hardlock-bypass-stub.md). Related work order: [Hardlock bypass stub](../work-orders/20260901-131-hardlock-bypass-stub.md).*

## 결과

- Hardlock HLE 구현은 보류를 유지했습니다. Function `0x0e` 변환과 유효한 `0x450` driver response는 여전히 근거가 없어 추측 구현을 넣지 않았습니다.
- 플랫폼 중립 `re2dj::device::HardlockStubDevice`를 추가했습니다. 확인된 vendor framing(`0/0`, `6/6`, `256/256`, `256 + block_count × 8`)에 맞는 요청만 완료 처리하고 나머지는 거절합니다. `0x468`은 성공만, `0x450`은 설정된 replay 또는 요청 보존, `0x44c`/`0x458`은 요청 보존과 status word `0` 정리로 응답하며 Function `0x0e` payload는 변환 없이 통과합니다.
- 기존 `HardlockProtocolTracker`의 control-code 분류를 `ClassifyHardlockRequest`로 노출해 스텁과 tracker가 같은 분류를 씁니다.
- Windows injected runtime에 `g_re2dj_hardlock_bypass_enabled` export를 추가하고 `DeviceIoControl` thunk를 스텁으로 연결했습니다. 기존 `--device-mock-hardlock-450-response`와 `--device-mock-hardlock-44c-tail` 실험값은 스텁 옵션으로 전달되어 분기 실험 결과가 유지됩니다. 우회가 꺼져 있으면 기존 경로가 그대로 실행됩니다.
- launcher에 `--hardlock-bypass`를 추가했습니다. export RVA를 찾아 원격 프로세스에 값을 쓰고, 활성화를 `{"event":"hardlock_bypass","stage":1,"synthetic":true}` JSONL 이벤트로 한 번 보고합니다.
- 제품 CLI에 `--hardlock-bypass`를 추가했습니다. Hardlock 설정을 요구하는 profile에서만 허용하며, CHD shortcut 경로와 일반 경로 양쪽에서 검사합니다. `OriginalProcessOptions`와 인자 생성기도 같은 정책을 강제합니다.
- runtime trace는 `re2dj:vfs:hardlock-bypass:request=...:outcome=...` 줄에 요청 종류, 결과, 바이트 수와 boolean만 남깁니다. 키, descriptor ID, block payload는 기록하지 않습니다.
- 사용자 확인에 따른 2단계(게스트 보호 분기 강제)는 구현하지 않았습니다. 막히는 분기를 계측으로 특정한 뒤 별도 작업에서 다룹니다.

*The Hardlock HLE stays deferred: no guessed Function `0x0e` transform or `0x450` driver response was added. A platform-neutral `re2dj::device::HardlockStubDevice` now completes only requests matching the confirmed vendor framing (`0/0`, `6/6`, `256/256`, `256 + block_count × 8`) and rejects the rest, answering `0x468` with success alone, `0x450` with a configured replay or the preserved request, and `0x44c`/`0x458` with the preserved request plus a cleared status word while passing the Function `0x0e` payload through untransformed. The tracker's control-code classification is now the shared `ClassifyHardlockRequest`. The Windows injected runtime exports `g_re2dj_hardlock_bypass_enabled` and routes its `DeviceIoControl` thunk through the stub, feeding the existing `--device-mock-hardlock-450-response` and `--device-mock-hardlock-44c-tail` experiment values in as stub options so earlier branch results still hold; with the bypass off the existing paths run unchanged. The launcher gained `--hardlock-bypass`, which writes the export in the remote process and reports activation once as `{"event":"hardlock_bypass","stage":1,"synthetic":true}`. The product CLI gained the same option, allowed only for profiles requiring a Hardlock configuration and checked on both the CHD-shortcut and general paths, with `OriginalProcessOptions` and the argument builder enforcing the same policy. The runtime trace records only request kind, outcome, byte count, and Booleans — never keys, descriptor IDs, or block payloads. Stage 2, forcing the guest protection branch, was deliberately not implemented and waits until instrumentation identifies the blocking branch.*

## 검증

- Windows x86 Debug build: 전체 타겟 통과. unit tests, injected runtime, Hardlock descriptor probe, product-loader probe, VFS runtime probe, launcher, 제품 CLI
- Unit tests: `1122` checks, `0` failures (이전 `1070`에서 52개 증가)
- 선택 CTest(`re2dj_unit_tests`, `re2dj_windows_hardlock_descriptor_probe`, `re2dj_windows_product_loader_probe`): `3/3` 통과
- `dumpbin /exports`로 `g_re2dj_hardlock_bypass_enabled`가 injected runtime DLL에 export됨을 확인
- 제품 CLI `--help`와 launcher usage에 새 옵션이 표시됨을 확인
- 전체 CTest는 `re2dj_windows_vfs_runtime_probe`가 이 sandbox에서 종료되지 않아 중단했습니다. 이 test는 이번 변경과 무관하며 build만 확인했습니다.
- 실제 CHD bounded 실행 2회로 우회 경로를 검증했습니다. 아래 "실제 실행 결과"를 참고하십시오. 절차는 [ez2dj4th Hardlock 설정 가이드](../guides/ez2dj4th-hardlock-config.md)에 남겼습니다.

*The Windows x86 Debug build passes for every target — unit tests, injected runtime, Hardlock descriptor probe, product-loader probe, VFS runtime probe, launcher, and product CLI — with unit tests at 1,122 checks and zero failures, up 52 from 1,070, and selected CTest passing 3/3. `dumpbin /exports` confirms `g_re2dj_hardlock_bypass_enabled` in the injected-runtime DLL, and the new option appears in both the product CLI `--help` and the launcher usage. The full CTest run was stopped because `re2dj_windows_vfs_runtime_probe` does not terminate in this sandbox; that test is unrelated to this change and was verified only to build. Two bounded real-CHD runs verified the bypass path; see "Real run results" below. The procedure is recorded in the [ez2dj4th Hardlock configuration guide](../guides/ez2dj4th-hardlock-config.md).*


## 실제 실행 결과

제품 CLI `re2dj.exe ez2dj4th --run --hardlock-bypass`는 우회를 무장하지만(`hardlock_bypass` 이벤트 기록) 실행이 진행되지 않았습니다. `--hle-vfs`의 handoff 기대 메시지가 `re2dj:vfs:CreateFileA`이고, launcher는 그 메시지를 처음 받으면 handoff 성공으로 보고 `run_detached`가 아닌 경우 원본을 종료합니다. `ez2dj4th` profile에는 `run_detached`가 없어 첫 `CreateFileA` 하나 만에 종료되었고 `DeviceIoControl`은 한 번도 호출되지 않았습니다. 이는 이번 우회 구현의 문제가 아니라 제품 실행 경로의 별개 정책 문제입니다.

launcher bounded 실행 두 번으로 우회를 검증했습니다. 두 실행 모두 `--device-mock-wts-console-session --hardlock-bypass --slot-writer-trace`를 사용했습니다. `--break-exit-process`만으로는 4th에 없는 정적 `KERNEL32!ExitProcess` IAT slot이 필요해 실패하므로 bounded trace 옵션과 함께 써야 합니다.

1. **분기 실험값 없이.** initialize 1회, handshake 3회가 모두 `outcome=completed`로 처리되었고 `replayed=0`, 즉 요청 buffer를 보존했습니다. 실행은 `0x44c`에 도달하지 못하고 child가 `0x00000008`로 종료했습니다. 기존 미확정 항목인 "buffer-preserving `0x450` success는 `0x44c`로 진행하지 못한다"를 재확인합니다.
2. **기존 분기 실험값과 함께.** `--device-mock-hardlock-450-response 0100fafa0010 --device-mock-hardlock-44c-tail 0001`을 더한 실행은 initialize 1회, handshake 2회, descriptor 37회, transform 36회를 기록했습니다. 76개 요청 전부 `outcome=completed`이고 `rejected-shape`는 0회입니다. 이 횟수는 [Task 127 작업 로그](20260901-127-ez2dj4th-hardlock-runtime.md)가 두 개의 개별 분기로 관찰한 값과 정확히 일치하므로, 스텁의 단일 계약이 기존 동작을 그대로 재현합니다.

두 번째 실행의 종료는 `0xc0000005` write fault이며 `eip=0x004c440b`(`.text`), `esp=0x75295d4d`, `ebp=0xc415ff50`입니다. 기록된 fault 주소는 `esp - 4`이고, `esp`는 `kernel32` 주소 범위 안입니다.

*Real run results: the product CLI arms the bypass but does not exercise it. With `--hle-vfs` the launcher's handoff message is `re2dj:vfs:CreateFileA`, and on the first such message a non-detached run is treated as handed off and the original is terminated; `ez2dj4th` carries no `run_detached`, so the process ended after one `CreateFileA` and never issued a `DeviceIoControl`. That is a separate product-path policy gap, not a defect in this bypass. Two bounded launcher runs verified the stub, both using `--device-mock-wts-console-session --hardlock-bypass --slot-writer-trace`; `--break-exit-process` alone fails because it needs a static `KERNEL32!ExitProcess` IAT slot that 4th does not have, so it must be paired with a bounded trace option. Without branch-experiment values, one initialize and three handshakes all completed with `replayed=0` — the request buffer preserved — and the child exited `0x00000008` without reaching `0x44c`, reconfirming the existing unresolved item that buffer-preserving `0x450` success does not advance. Adding `--device-mock-hardlock-450-response 0100fafa0010 --device-mock-hardlock-44c-tail 0001` produced one initialize, two handshakes, 37 descriptors, and 36 transforms, with all 76 requests `outcome=completed` and zero `rejected-shape`; those counts match exactly what [the Task 127 work log](20260901-127-ez2dj4th-hardlock-runtime.md) observed through two separate branches, so the stub's single contract reproduces the earlier behavior. The second run ended in a `0xc0000005` write fault with `eip=0x004c440b` in `.text`, `esp=0x75295d4d`, and `ebp=0xc415ff50`; the faulting address is `esp - 4` and `esp` lies inside the `kernel32` address range.*


## 인과 실험 — transform 출력 소비 여부

정적 이미지와 실행 중 memory를 대조했습니다. `.text`는 `vaddr 0x1000 / raw 0x1000`이라 RVA와 file offset이 같습니다.

- 파일의 `.text` 901,120바이트는 Shannon 엔트로피 `7.9967` bits/byte이고, `55 8b ec` prologue와 `cc` padding이 각각 0회입니다. 즉 파일에서 이미 암호화되어 있습니다.
- 같은 위치(VA `0x004c4400`, file offset `0x000c4400`)의 파일 32바이트와 실행 중 32바이트는 서로 다릅니다. 보호 코드가 실행 중에 이 영역을 다시 씁니다.
- fault 지점 `0x004c440b`의 실행 중 바이트는 `0x55`(`push ebp`)이며, 이것이 기록된 `esp - 4` write fault와 일치합니다.

이어서 스텁의 진단용 `--hardlock-transform-xor ff` probe로 Function `0x0e` 출력 8바이트만 뒤집은 대조 실행을 했습니다. 두 실행은 그 외 모든 옵션이 같고 IOCTL 처리 결과도 같습니다(initialize 1, handshake 2, descriptor 37, transform 36, 전부 `outcome=completed`).

| 항목 | A: identity | B: XOR `0xff` |
| --- | --- | --- |
| transform payload | `payload_preserved=1` | `payload_perturbed=1` |
| fault `eip` | `0x004c440b` (`.text`) | `0x0024d000` (image 밖) |
| fault 접근 | write `0x75295d49` (`esp - 4`) | write `0x00000000` |
| `esp` | `0x75295d4d` | `0x001beb34` |
| `ebp` | `0xc415ff50` | `0x75295d49` |

**확인됨.** 게스트는 Function `0x0e` 출력을 소비하며, 그 값이 이후 실행 경로를 결정합니다. 출력 8바이트만 바꾸면 fault 지점과 종류가 모두 달라집니다.

**추정.** `.text`가 파일에서 암호화되어 있고 실행 중 다시 쓰이며, transform 출력이 실행 경로를 바꾼다는 두 사실을 함께 보면 transform 출력이 보호 복호화의 key material로 쓰인다고 보는 것이 자연스럽습니다. 다만 key 유도 경로를 직접 관찰하지는 않았습니다.

**결론.** 뒤집을 보호 분기 하나가 막고 있는 구조가 아니므로, 계획했던 2단계 우회(게스트 분기 강제)로는 올바른 transform을 대체할 수 없습니다. 2단계는 진행하지 않습니다.

*Causality experiment — does the guest consume the transform output? Comparing the static image with process memory (for `.text`, RVA equals file offset), the file's 901,120-byte `.text` has Shannon entropy `7.9967` bits/byte with zero `55 8b ec` prologues and zero `cc` padding, so it is already encrypted on disk; the 32 bytes at VA `0x004c4400` differ between file and process, so the protection rewrites that region at runtime; and the runtime byte at the fault point `0x004c440b` is `0x55` (`push ebp`), which matches the recorded `esp - 4` write fault. A controlled run then used the stub's diagnostic `--hardlock-transform-xor ff` probe to invert only the eight Function `0x0e` output bytes, with every other option identical and identical IOCTL results (one initialize, two handshakes, 37 descriptors, 36 transforms, all `outcome=completed`). The identity run faults at `eip=0x004c440b` in `.text` writing `esp - 4` with `esp=0x75295d4d`; the perturbed run faults at `eip=0x0024d000` outside any image writing null with `esp=0x001beb34`. **Confirmed:** the guest consumes the Function `0x0e` output and that value determines the downstream execution path, since changing eight bytes changes both the fault site and its kind. **Inferred:** taken with the encrypted-on-disk `.text` that is rewritten at runtime, the transform output most likely serves as key material for the protection's decryption, though the key-derivation path was not directly observed. **Conclusion:** the blocker is not a single protection branch, so the planned stage-2 bypass of forcing a guest branch cannot substitute for a correct transform, and stage 2 will not proceed.*

## 다음 단계

1. 2단계 우회(게스트 보호 분기 강제)는 취소합니다. 인과 실험이 전제를 부정했습니다.
2. 남은 경로는 원래의 증거 조건과 같습니다. 물리 Hardlock 관찰, 독립적으로 검증된 Function `0x0e` input/output vector, 또는 허용 가능한 clean-room 복원 중 하나가 필요합니다.
3. 그때까지 우회 스텁은 장치 경계 계약 검증과 이후 경로 관찰 수단으로 유지합니다. 우회 실행에서 얻은 관찰은 **추정** 이하로만 기록합니다.
4. `ez2dj4th` profile의 `run_detached`와 active-console 정책은 제품 실행 경로의 별도 결정 사항으로 분리해 다룹니다.

*Next: stage 2 is cancelled because the causality experiment refuted its premise. The remaining path is the original evidence condition — physical-Hardlock observation, independently verified Function `0x0e` input/output vectors, or an allowed clean-room reconstruction. Until then the bypass stub stays as a device-boundary contract check and a way to observe later paths, with observations from bypassed runs recorded no higher than **inferred**. The `run_detached` and active-console policies for the `ez2dj4th` profile remain a separate product-path decision.*
