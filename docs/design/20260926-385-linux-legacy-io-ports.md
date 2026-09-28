# 작업 385 설계 — Linux IO 보드 포트 입출력 / Task 385 design — I/O board port access on Linux

선행: [작업 384 설계](20260926-384-directinput-entry.md)

## 배경 / Background

작업 384 뒤 Linux 실행은 게임 코드 `0x004c3817`의 `in al, dx`(port `0x0103`)에서 SIGSEGV로 멈췄다. user mode의 `in`/`out`은 일반 보호 예외를 낸다.

Windows runtime(`HandleLegacyIoPortException`)은 이 예외를 가로채 공용 `LegacyIoPortBus`(EZ2DJ 보드)나 `Ez2DancerIoPortBus`로 답하고, EIP를 명령 다음으로 넘긴다. 어떤 예외가 보드 접근인지는 다음 규칙으로 판정한다.

- 명령의 폭이 프로필과 같아야 한다. word 보드면 0x66 접두가 있어야 하고, byte 보드면 없어야 한다.
- 확인된 helper 주소의 명령은 그 helper의 방향이다.
- helper를 모르는 방향은 opcode로 판단한다(`0xEC/0xED` 읽기, `0xEE/0xEF` 쓰기).
- 마지막으로 opcode가 방향과 폭에 맞아야 한다.

읽기는 EAX 중 해당 폭의 부분만 바꾼다. 4th 프로필은 입력 helper RVA `0xc3817`과 출력 helper RVA `0xc384b`를 확인해 두었다.

*After Task 384 a Linux run stopped with SIGSEGV at the game's `in al, dx` (port `0x0103`) at `0x004c3817`: user-mode `in`/`out` raise a general protection fault.*

*The Windows runtime (`HandleLegacyIoPortException`) catches that exception, answers it through the shared `LegacyIoPortBus` (the EZ2DJ board) or `Ez2DancerIoPortBus`, and steps EIP past the instruction. It decides whether an exception is a board access by these rules:*

- *The instruction's width must be the profile's: a 0x66 prefix for a word board, none for a byte board.*
- *An instruction at a confirmed helper's address takes that helper's direction.*
- *A direction whose helper is unknown is judged by opcode (`0xEC/0xED` read, `0xEE/0xEF` write).*
- *Last, the opcode must match the direction and width.*

*A read replaces only the operand's width of EAX. The 4th's profile has confirmed its input helper RVA `0xc3817` and output helper RVA `0xc384b`.*

## 결정 / Decisions

1. **core (`re2dj/input/legacy_io_trap.h`).** 위 판정 규칙을 core로 옮긴다.
   - `LegacyIoTrapPolicy`: 사용 여부, 폭, image base, helper RVA.
   - `DecodeLegacyIoAccess`: 방향, 폭, 명령 길이를 돌려준다.
   - `MergeLegacyIoRead`: 읽은 값을 EAX에 합친다.

   Windows handler는 이 core를 쓴다. keyboard polling과 trace는 Windows에 남긴다.

   ***Core (`re2dj/input/legacy_io_trap.h`).** The rules above move into the core:*
   - *`LegacyIoTrapPolicy`: whether the trap is on, the width, the image base, and the helper RVAs.*
   - *`DecodeLegacyIoAccess`: returns the direction, width, and instruction length.*
   - *`MergeLegacyIoRead`: merges the value read into EAX.*

   *The Windows handler uses this core; its keyboard polling and trace stay on Windows.*
2. **Linux (`native_legacy_io.h/.cpp`).**
   - run이 시작할 때 프로필의 정책을 로드된 main image 위치에 맞춰 설정하고, 전원 켠 상태의 `LegacyIoPortBus`를 준비한다.
   - 두 폭의 guest signal handler는 SIGSEGV를 받으면 `HandleNativeLegacyIoTrap`을 먼저 부른다. 보드 접근이면 bus로 답하고, 읽기면 AL을 설정한 뒤 EIP를 넘기고 guest로 돌아간다.
   - 판정이나 답을 못 하면 지금처럼 fault로 남는다.
   - 이 처리는 async-signal-safe다. bus는 배열 연산뿐이고, guest 명령 바이트는 host 프로세스에 그대로 매핑되어 있다.
   - word 폭 보드(EZ2Dancer)는 아직 Linux에서 켜지 않는다.
   - 읽기·쓰기·미응답 수와 첫 접근을 실행 결과로 보고한다.

   ***Linux (`native_legacy_io.h/.cpp`):***
   - *When a run starts, it places the profile's policy at the loaded main image and prepares a `LegacyIoPortBus` in its power-on state.*
   - *On SIGSEGV, both widths' guest signal handlers first call `HandleNativeLegacyIoTrap`. For a board access it answers through the bus, sets AL for a read, steps EIP past the instruction, and returns to the guest.*
   - *A fault it cannot decode or answer stays a fault, as before.*
   - *This is async-signal-safe: the bus does only array work, and the guest's instruction bytes are mapped as they are in the host process.*
   - *A word-wide board (EZ2Dancer) is not enabled on Linux yet.*
   - *The run reports the read, write, and unanswered counts and the first access.*
3. **CLI.** Linux run에 프로필의 legacy I/O 계약(`legacy_io_ports`와 `legacy_io_ports_default`, 폭, RVA)을 넘긴다. Windows가 기본으로 적용하는 조건과 같다.
   ***CLI:** the Linux run receives the profile's legacy I/O contract (`legacy_io_ports` with `legacy_io_ports_default`, the width, and the RVAs), under the same conditions Windows applies by default.*

## 범위 밖 / Out of scope

- Linux host 키보드를 IO 보드 입력(`Ez2DjKeyboardInput`)에 연결하기. / *Connecting the Linux host keyboard to the I/O board input (`Ez2DjKeyboardInput`).*
- `winmm!mixerGetNumDevs`부터 이어지는 mixer API: 다음 작업. / *The mixer API from `winmm!mixerGetNumDevs`: the next task.*
