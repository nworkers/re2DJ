# 작업 로그 124 — ez2dj4th zero pointer-slot 참조 추적

## 결과

fault-call attribution이 확인한 absolute pointer slot을 child의 live main
image에서 bounded scan하는 진단을 추가했습니다. committed이며 guard 또는
no-access가 아닌 memory만 읽고, 64 KiB block 사이에 3-byte overlap을 두며,
최대 64개 일치 위치의 section과 24-byte runtime window를 JSONL로
기록합니다. child context, code, pointer slot은 변경하지 않습니다.

정식 PE import table은 <code>0x00B16A60</code>,
<code>0x00AE0F90</code>, <code>0x00B193D0</code> 등의 범위에 있고
<code>0x00AF0CF4</code>를 포함하지 않았습니다. 또한 CHD VFS와 injected
runtime 없이 실행한 native baseline log
<code>20260901-113000-720.jsonl</code>에서도 native
<code>CreateFileA</code> 주소 <code>0x774533A0</code>가 반환됐지만 slot은
0이었고 동일한 <code>EIP=0</code> fault가 발생했습니다. 따라서 zero slot은
현재 HLE가 직접 만든 결과가 아닙니다.

실제 <code>4thTrax.chd</code> trace는 main image 7,446,528 bytes를 검사해
12개 참조를 찾았으며 cap에는 도달하지 않았습니다. 명령 경계가 분명한
참조는 다음과 같습니다.

| 분류 | 명령 주소 |
|---|---|
| <code>MOV [slot], EAX</code> | <code>0x00AEF5F0</code>, <code>0x00AEFE62</code>, <code>0x00AF061A</code> |
| <code>CALL DWORD PTR [slot]</code> | <code>0x00AEF5C8</code>, <code>0x00AEF7F8</code>, <code>0x00AEFD68</code>, <code>0x00AEFFE7</code>, <code>0x00AF0500</code>, <code>0x00AF06A4</code> |
| <code>CMP DWORD PTR [slot], 0</code> | <code>0x00AEF645</code>, <code>0x00AEF90F</code>, <code>0x00AF0954</code> |

세 writer 명령의 존재는 **확인됨**입니다. 다만 이 scan은 fault 시점의
runtime bytes만 읽으므로 어느 writer가 실제 실행됐는지, 실행 당시 EAX가
0이었는지, 이 slot이 동적 <code>CreateFileA</code> 반환값과 연결되는지는
**미확정**입니다.

## 검증

* 브랜치: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug launcher build: 통과, warning/error 0
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe: <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* native baseline diagnostic: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-113000-720.jsonl</code>
* 실제 CHD diagnostic: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-113406-920.jsonl</code>

실제 CHD summary는 다음과 같습니다.

```text
{"event":"av_slot_reference_summary","slot":"0x00af0cf4","image_base":"0x00400000","scanned_bytes":7446528,"matches":12,"capped":false}
```

원본 실행 파일·CHD·HDD 자산과 실행 로그는 저장소에 추가하지 않았습니다.

## English

## Result

A bounded live-main-image scan was added for absolute pointer slots identified
by fault-call attribution. It reads only committed memory that is neither
guarded nor no-access, uses a three-byte overlap across 64 KiB blocks, and
records the section and a 24-byte runtime window for at most 64 matches. It
does not modify child context, code, or the pointer slot.

Formal PE import tables occupy ranges such as <code>0x00B16A60</code>,
<code>0x00AE0F90</code>, and <code>0x00B193D0</code>; they do not include
<code>0x00AF0CF4</code>. Native baseline log
<code>20260901-113000-720.jsonl</code>, produced without CHD VFS or the injected
runtime, returned native <code>CreateFileA</code> at
<code>0x774533A0</code>, yet retained a zero slot and produced the same
<code>EIP=0</code> fault. The current HLE therefore did not directly create the
zero value.

The real <code>4thTrax.chd</code> trace scanned 7,446,528 main-image bytes and
found 12 references without reaching the cap. References with clear
instruction boundaries are:

| Class | Instruction addresses |
|---|---|
| <code>MOV [slot], EAX</code> | <code>0x00AEF5F0</code>, <code>0x00AEFE62</code>, <code>0x00AF061A</code> |
| <code>CALL DWORD PTR [slot]</code> | <code>0x00AEF5C8</code>, <code>0x00AEF7F8</code>, <code>0x00AEFD68</code>, <code>0x00AEFFE7</code>, <code>0x00AF0500</code>, <code>0x00AF06A4</code> |
| <code>CMP DWORD PTR [slot], 0</code> | <code>0x00AEF645</code>, <code>0x00AEF90F</code>, <code>0x00AF0954</code> |

The three writer instructions are **confirmed to exist**. This scan only reads
runtime bytes at fault time, so which writer executed, whether EAX was zero at
execution, and whether the slot is connected to the dynamic
<code>CreateFileA</code> result remain **unresolved**.

## Verification

* Branch: <code>task-113-ez2dj4th-chd</code>
* Windows x86 Debug launcher build: passed with zero warnings and errors
* Unit tests: <code>checks: 999, failures: 0</code>
* Product-loader probe: <code>windows-product-loader-probe: profile-defaults=ok unsupported-target=ok</code>
* Native baseline diagnostic: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-113000-720.jsonl</code>
* Real CHD diagnostic: <code>logs/windows_x86_launcher_probe/ez2dj4th/20260901-113406-920.jsonl</code>

The real CHD summary was:

```text
{"event":"av_slot_reference_summary","slot":"0x00af0cf4","image_base":"0x00400000","scanned_bytes":7446528,"matches":12,"capped":false}
```

The original executable, CHD, HDD assets, and runtime logs were not added to
the repository.
