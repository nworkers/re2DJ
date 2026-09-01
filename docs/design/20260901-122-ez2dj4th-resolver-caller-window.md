# ez2dj4th resolver caller instruction window 설계

## 상태

구현 완료. 작업 121에서 반환 포인터와 원본 caller 주소는 확인되었지만,
반환 직후 protected code가 그 포인터를 어떻게 소비하는지는 미확정입니다.
실제 runtime memory window를 readable 상태로 확인했고,
<code>CreateFileA</code> 반환 직후의 EAX 저장 instruction을 관찰했습니다.

## 한국어

### 목적과 근거

4th trace의 <code>CreateFileA</code> resolver caller return address는
<code>0x00af09f6</code>입니다. 이 주소에서 실제 실행된 code window를
관찰하면 반환값 저장, 조건 분기, indirect call 후보를 확인할 수 있습니다.
protected section은 self-modifying 가능성이 있으므로 디스크 파일을 다시
해석하지 않고 child process의 현재 memory를 읽어야 합니다.

### 설계 결정

1. resolver 내부에서 캡처한 caller return address를 기준으로 앞 8바이트와
   뒤 16바이트, 총 24바이트를 <code>ReadProcessMemory</code>로 읽습니다.
2. VFS log에 window base, caller address, readable 여부, 읽힌 hex bytes를
   bounded event로 기록합니다.
3. 별도 caller-window budget을 최대 32개로 두어 resolver log가 무한히
   늘어나지 않게 합니다.
4. 메모리 읽기만 수행하며 breakpoint, instruction pointer, 반환값, 원본
   process memory를 변경하지 않습니다.
5. 이 window를 정적 disassembly로 확정하지 않습니다. bytes는 해당 실행의
   runtime observation이며, branch·indirect-call 의미는 추정 또는 미확정으로
   기록합니다.

~~~mermaid
sequenceDiagram
    participant G as 4th protected code
    participant R as resolver thunk
    participant M as child memory
    participant L as VFS log
    G->>R: GetProcAddress
    R->>M: read caller-8..caller+15
    M-->>R: current code bytes
    R->>L: caller window event
    R-->>G: unchanged resolver result
~~~

### 검증 전략

* Windows x86 injected runtime을 Debug로 빌드합니다.
* unit tests와 product-loader probe를 실행합니다.
* 실제 <code>4thTrax.chd</code> bounded trace에서
  <code>dynamic-resolver-caller</code> event를 확인합니다.
* <code>CreateFileA</code> caller window는
  <code>base=0x00af09ee</code>, <code>caller=0x00af09f6</code>,
  <code>readable=1</code>로 확인되었습니다.
* caller offset 8의 <code>89 45 dc</code>는 반환 EAX를
  <code>[EBP-0x24]</code>에 저장하는 instruction으로 확인되었습니다.
* window bytes로 정상 호출 또는 보호 응답을 확정하지 않습니다.

## English

### Status

Implementation complete. Task 121 confirmed the returned pointer and
original caller address, but how the protected code consumes that result remains
unresolved.
The real runtime memory window was readable and showed the immediate
<code>CreateFileA</code> return-value store.

### Purpose and evidence

The 4th trace reports <code>0x00af09f6</code> as the
<code>CreateFileA</code> resolver caller return address. Observing the current
code window at that address may identify result storage, conditional branches,
or an indirect-call candidate. Because the protected section may be
self-modifying, read the child process's current memory rather than reinterpreting
on-disk bytes.

### Design decisions

1. Read 8 bytes before and 16 bytes after the resolver-caller return address,
   for a 24-byte window, with <code>ReadProcessMemory</code>.
2. Record the window base, caller address, readability, and hex bytes as a
   bounded VFS-log event.
3. Use a separate caller-window budget capped at 32 events.
4. Only read memory; do not change breakpoints, the instruction pointer, return
   values, or original process memory.
5. Do not treat the window as static disassembly. It is runtime observation
   from one execution; branch and indirect-call meanings remain inferred or
   unresolved.

### Verification strategy

* Build the Windows x86 injected runtime in Debug.
* Run the unit tests and product-loader probe.
* Check the real <code>4thTrax.chd</code> bounded trace for a
  <code>dynamic-resolver-caller</code> event.
* The <code>CreateFileA</code> window was readable at
  <code>base=0x00af09ee</code> around
  <code>caller=0x00af09f6</code>.
* The bytes at caller offset 8, <code>89 45 dc</code>, confirmed an
  instruction that stores the returned EAX at <code>[EBP-0x24]</code>.
* Do not use the window bytes to claim normal execution or a protection
  response.
