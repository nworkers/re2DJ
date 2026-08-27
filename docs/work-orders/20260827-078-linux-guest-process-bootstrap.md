# Linux guest process bootstrap 작업 지시

## 상태

**완료.** 설계는 [Linux guest process bootstrap](../design/20260827-078-linux-guest-process-bootstrap.md)을 따른다.

## 작업

1. guest stack·TEB/PEB·FS·signal context를 소유하는 Linux i386 전용 subsystem을 추가한다.
2. TLS callback과 PE entry를 전용 guest stack에서 실행하고 helper control stack으로 안전하게 복귀시킨다.
3. guest fault의 signal, EIP와 ESP를 기존 protocol v3 execution event로 전송한다.
4. synthetic TLS callback에 FS self와 stack bounds 검증을 추가하고 기존 result 51을 유지한다.
5. Linux 전용 invalid-instruction fixture로 구조화된 fault event를 검증한다.
6. warnings-as-errors Linux x64/i386 build, CTest와 IPC integration을 실행한다.
7. architecture, porting plan, TODO와 작업 로그를 갱신하고 커밋한다.

## 완료 기준

- guest TLS callback과 entry가 Linux helper control stack이 아닌 guard-page guest stack에서 실행된다.
- `FS:[0x18]`이 TEB self를 가리키고 TEB stack base/limit가 실제 guest stack 범위와 일치한다.
- 최소 PEB의 image base가 선택된 mapped image base와 일치한다.
- invalid instruction이 helper의 무구조 종료가 아니라 `SIGILL`, 정확한 EIP와 guest ESP가 포함된 `kFault` event가 된다.
- 기존 relocation, TLS, named/ordinal import, stack memory IPC와 result 51 회귀가 유지된다.

---

# Linux Guest Process Bootstrap Work Order

## Status

**Complete.** Follow the [Linux guest process bootstrap design](../design/20260827-078-linux-guest-process-bootstrap.md).

## Tasks

1. Add a Linux-i386-only subsystem owning the guest stack, TEB/PEB, FS, and signal context.
2. Execute TLS callbacks and PE entry on the guest stack and return safely to the helper control stack.
3. Send guest fault signal, EIP, and ESP through the existing protocol-v3 execution event.
4. Extend the synthetic TLS callback with FS-self and stack-bounds checks while preserving result 51.
5. Verify a structured fault event with a Linux-only invalid-instruction fixture.
6. Run warnings-as-errors Linux x64/i386 builds, CTest, and IPC integration.
7. Update architecture, the porting plan, TODO, and the work log, then commit.

## Completion Criteria

Guest TLS callbacks and entry execute on a guarded guest stack rather than the helper control stack. `FS:[0x18]` identifies the TEB, its stack bounds match the mapping, the minimal PEB contains the mapped image base, and an invalid instruction becomes a `kFault` event with `SIGILL`, exact EIP, and guest ESP. Existing relocation, TLS, named/ordinal imports, stack-memory IPC, and result 51 remain green.
