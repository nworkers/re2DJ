# 작업 로그 125 — ez2dj4th pointer-slot writer 실행 추적

## 결과

<code>--slot-writer-trace</code>를 추가해 4th의 세 slot writer RVA에 x86
DR0–DR2 local execution breakpoint를 설정했습니다. primary thread와 이후
생성 thread를 모두 arm하며, 최대 64개 hit에서 실행 직전 EAX, pre-store
slot, instruction bytes, DR6를 기록합니다. 원본 code와 slot은 수정하지
않고 DR6 clear와 EFLAGS resume flag만 적용합니다.

실제 CHD/VFS 실행에서 다음 hit를 확인했습니다.

```text
writer=0x00AEFE62 rva=0x006EFE62
eax=0x00B17B00 slot_before=0x00000000
bytes=a3f40caf000f8151
```

5초 idle boundary의 slot 현재 값도 <code>0x00B17B00</code>이었습니다. HLE와
injected runtime이 없는 native baseline에서도 같은 writer, EAX, pre-store
slot이 확인됐습니다.

CHD/VFS log는 writer 이후 실제 wrapper request를 기록했습니다.

```text
\\.\NTICE    -> unmapped, error=123
\\.\NTICE    -> unmapped, error=123
\\.\FEnteDev -> unmapped, error=123
```

따라서 실제 <code>CreateFileA</code> wrapper 호출과 첫 보호 device-open
경계가 **확인됨**입니다. 현재 VFS는 device path를 일반 path로 처리하므로
device handle과 IOCTL 응답은 **미확정**입니다.

대조군으로 broad API trace의 software watch 40개를 함께 설치하면 writer
hit은 0회였고 slot 0 상태의 기존 <code>EIP=0</code> fault가 재현됐습니다.
이는 이전 fault가 broad API breakpoint 관찰에 의해 실행 경로가 달라진
조건임을 확인합니다. 정확히 어느 watch가 영향을 주는지는 미확정입니다.

## 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug launcher build: 통과, warning/error 0
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe: <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* CHD/VFS hardware trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131127-749.jsonl</code>
* native baseline trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131142-385.jsonl</code>
* API-watch control trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131259-001.jsonl</code>

원본 EXE·CHD·HDD 자산과 runtime log는 저장소에 추가하지 않았습니다.

## English

## Result

The new <code>--slot-writer-trace</code> option sets x86 DR0–DR2 local
execution breakpoints on the three 4th slot-writer RVAs. It arms the primary
and later-created threads and records up to 64 hits with pre-instruction EAX,
the pre-store slot, instruction bytes, and DR6. It does not modify original code
or slot data; hit handling only clears DR6 and sets the EFLAGS resume flag.

The real CHD/VFS run produced:

```text
writer=0x00AEFE62 rva=0x006EFE62
eax=0x00B17B00 slot_before=0x00000000
bytes=a3f40caf000f8151
```

The slot still held <code>0x00B17B00</code> at the five-second idle boundary.
A native baseline without HLE or the injected runtime produced the same writer,
EAX, and pre-store slot.

After the writer, the CHD/VFS log recorded actual wrapper requests:

```text
\\.\NTICE    -> unmapped, error=123
\\.\NTICE    -> unmapped, error=123
\\.\FEnteDev -> unmapped, error=123
```

This **confirms** actual <code>CreateFileA</code> wrapper invocation and the
first protected device-open boundary. The VFS currently treats device paths as
ordinary paths, so device handles and IOCTL responses remain **unresolved**.

In the control run with all 40 broad API software watches enabled, no writer
hit occurred and the earlier <code>EIP=0</code> fault was reproduced with a zero
slot. The previous fault is therefore confirmed as a condition where broad API
breakpoint observation changes the execution path. Which watch causes the
change remains unresolved.

## Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug launcher build: passed with zero warnings and errors
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe: <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* CHD/VFS hardware trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131127-749.jsonl</code>
* Native baseline trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131142-385.jsonl</code>
* API-watch control trace: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-131259-001.jsonl</code>

The original executable, CHD, HDD assets, and runtime logs were not added to
the repository.
