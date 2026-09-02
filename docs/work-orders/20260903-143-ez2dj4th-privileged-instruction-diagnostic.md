# ez2dj4th privileged-instruction 진단 작업 지시

관련 설계: [ez2dj4th privileged-instruction 진단 설계](../design/20260903-143-ez2dj4th-privileged-instruction-diagnostic.md)

*Related design: [ez2dj4th privileged-instruction diagnostic design](../design/20260903-143-ez2dj4th-privileged-instruction-diagnostic.md).*

## 작업 내용 / Work items

1. attached-debugger exception loop에 privileged-instruction context recorder를 추가합니다.
2. opcode, first-chance 상태, EAX/EDX/EIP/ESP와 port/value를 bounded JSONL로 기록합니다.
3. 설계와 분석 문서를 새 계측 결과로 갱신합니다.
4. Windows x86 build, unit test, product-loader probe와 실제 4th diagnostic 실행을 검증합니다.

*Add a privileged-instruction context recorder to the attached-debugger exception
loop; record opcode, first-chance state, EAX/EDX/EIP/ESP, and port/value in bounded
JSONL; update design and analysis documents with the new evidence; and verify the
Windows x86 build, unit tests, product-loader probe, and a real 4th diagnostic run.*

## 완료 조건 / Completion criteria

- `0xc0000096` 예외에 대해 전용 JSONL event가 기록됩니다.
- event에 fault address, opcode bytes, first-chance 상태, EDX low word와 EAX low byte가 포함됩니다.
- 기존 Hardlock/VFS 관찰 결과가 회귀하지 않습니다.
- privileged exception을 자동으로 삼키거나 4th I/O HLE를 추측해 켜지 않습니다.
- 작업 로그와 관련 분석 문서가 남고 작업 커밋을 생성합니다.

*Completion requires a dedicated JSONL event for `0xc0000096` containing fault
address, opcode bytes, first-chance state, EDX's low word, and EAX's low byte;
no regression in Hardlock/VFS observations; no automatic swallowing of the
privileged exception or guessed 4th I/O HLE; and a work log, updated analysis, and
task commit.*

## 검증 명령 / Verification commands

```powershell
cmake --build build\windows-x86 --config Debug
.\build\windows-x86\bin\Debug\re2dj_unit_tests.exe
.\build\windows-x86\bin\Debug\re2dj_windows_product_loader_probe.exe
.\build\windows-x86\bin\Debug\re2dj_windows_x86_launcher_probe.exe --hdd <staging-root> --chd <4thTrax.chd> --target ez2dj4th --target-executable EZ2DJ\EZ2DJ.EXE --device-mock-lptdi --device-mock-wts-console-session --slot-writer-trace
```

*The same build, unit-test, product-loader, and 4th diagnostic commands are used,
with the CHD staging root and user-supplied CHD path substituted.*
