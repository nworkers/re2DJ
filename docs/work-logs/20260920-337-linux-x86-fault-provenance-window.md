# 작업 로그 337: Linux x86 fault provenance window / Work log 337: Linux x86 fault provenance window

## 결과 / Result

Linux x86 `NativeFaultObservation`의 live instruction window를 EIP 전 64 bytes와 후 16 bytes, 합계 80 bytes로 확장했습니다. CLI는 window 시작 주소와 EIP offset을 출력합니다. 이 변경은 guest 실행을 재개하거나 HLE 반환값을 바꾸지 않습니다.

*Expanded the Linux x86 `NativeFaultObservation` live instruction window to 64 bytes before EIP and 16 after it, 80 bytes total. The CLI prints the window start address and EIP offset. This change neither resumes guest execution nor changes HLE return values.*

실제 4th CHD window의 EIP 직전 확정 흐름은 `XOR ECX, ECX`, `JB 0x00aef590`, `MOV CL, byte ptr [EAX]`입니다. captured EFLAGS `0x00010246`의 CF가 clear이므로 `JB`는 미분기하고 EAX=0 null read가 발생했습니다. 확장된 window의 앞부분만으로는 EAX를 zero로 만든 명령의 경계와 실행을 확인할 수 없었습니다.

*The confirmed immediate flow before EIP in the real 4th CHD window is `XOR ECX, ECX`, `JB 0x00aef590`, `MOV CL, byte ptr [EAX]`. CF is clear in captured EFLAGS `0x00010246`, so `JB` was not taken and the EAX-zero null read occurred. The earlier portion of the expanded window does not establish the boundary or execution of the instruction that made EAX zero.*

## 검증 / Validation

WSL Linux x86에서 product build와 synthetic probe를 실행했습니다. `UD2` fixture는 80-byte window에서 EIP offset 64의 `0f 0b` marker와 guarded stack word를 검증했습니다. 실제 4th CHD의 `--linux-in-process-getversion-call` 실행은 EIP `0x00af0c22`, window start `0x00af0be2`, EIP offset 64를 반복 출력했습니다. 수집한 80 bytes만 i386 `objdump`로 bounded disassembly했습니다.

*Ran the product build and synthetic probe under WSL Linux x86. The `UD2` fixture verifies the `0f 0b` marker at EIP offset 64 in the 80-byte window and a guarded-stack word. The real 4th CHD `--linux-in-process-getversion-call` run repeatedly printed EIP `0x00af0c22`, window start `0x00af0be2`, and EIP offset 64. Only the collected 80 bytes were bounded-disassembled with i386 `objdump`.*

Linux x64 product build도 통과했으며, i386 전용 diagnostic은 x64에서 명시적 오류로 거부됩니다.

*The Linux x64 product build also passed, and the i386-only diagnostic is explicitly rejected on x64.*