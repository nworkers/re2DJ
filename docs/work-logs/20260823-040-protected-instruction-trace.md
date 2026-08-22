# 보호 실행 파일 instruction trace 작업 로그

관련 작업 지시: [보호 실행 파일 instruction trace 작업 지시](../work-orders/20260823-040-protected-instruction-trace.md)  
관련 설계: [보호 실행 파일 instruction trace](../design/20260823-040-protected-instruction-trace.md)

## 구현 결과

`re2dj_windows_x86_launcher_probe`에 `--instruction-trace <max-steps>`를 추가했습니다. 이 옵션은 software entry breakpoint를 사용하고, 원래 entry byte를 복원한 뒤 EIP를 entry로 되돌리고 TF를 설정합니다. primary thread의 모든 debugger event 뒤 TF를 다시 설정해 single-step trace가 DLL load 같은 중간 event에서 끊기지 않게 했습니다.

single-step마다 원본 코드 바이트 최대 16개와 instruction 주소를 32개 고정 길이 history에 보관합니다. intermediate step는 로그에 하나씩 쓰지 않으며, illegal instruction 또는 step limit에서만 history를 JSONL 진단 로그에 기록합니다.

## 검증 결과

1. `cmake --build --preset windows-x86-debug --config Debug` 성공.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` 성공: 2/2 통과.
3. canonical `ez2dj.exe`에 `--instruction-trace 200000` 실행.
   - 200,000 step를 실제 수집하고 `instruction_trace`와 32개 마지막 sample을 로그에 남겼습니다.
   - 마지막 sample은 protected image `0x01ed2dce`–`0x01ed2e67`의 비교/loop 코드였습니다.
   - configured limit 전에 `0xC000001D` fault에는 도달하지 못했습니다.

## 결론

instruction trace 기능과 TF 재설정은 검증됐지만, 이 방식은 debugger overhead가 커서 현재 protected stub의 원래 종료 시점까지 충분히 빠르게 도달하지 못합니다. 따라서 invalid target caller를 찾았다고 결론 내릴 수 없습니다. 다음 분석은 allocation/protection event 또는 target-producing data watch처럼 덜 침습적인 관찰점을 검증해야 합니다.

---

# Protected Executable Instruction Trace Work Log

Related work order: [Protected Executable Instruction Trace Work Order](../work-orders/20260823-040-protected-instruction-trace.md)  
Related design: [Protected Executable Instruction Trace](../design/20260823-040-protected-instruction-trace.md)

## Implementation result

Added `--instruction-trace <max-steps>` to `re2dj_windows_x86_launcher_probe`. It uses the software entry breakpoint, restores the original entry byte, resets EIP to entry, and enables TF. TF is rearmed after every primary-thread debugger event so DLL-load and similar intermediate events do not end the single-step trace.

Each step retains up to 16 original-code bytes and its instruction address in a fixed 32-entry history. Intermediate steps are not individually written to the log; history is written to JSONL only on an illegal instruction or step limit.

## Verification result

1. `cmake --build --preset windows-x86-debug --config Debug` succeeded.
2. `ctest --test-dir build\\windows-x86 -C Debug --output-on-failure` succeeded: 2/2 passed.
3. Ran canonical `ez2dj.exe` with `--instruction-trace 200000`.
   - Collected 200,000 actual steps and recorded `instruction_trace` plus the final 32 samples.
   - Final samples were comparison/loop code in protected-image range `0x01ed2dce`–`0x01ed2e67`.
   - It did not reach the `0xC000001D` fault before the configured limit.

## Conclusion

The instruction-trace function and TF rearming are verified, but debugger overhead prevents this method from reaching the protected stub's original termination point quickly enough. It cannot establish the invalid-target caller. The next analysis must validate a less intrusive observation point, such as allocation/protection events or a data watch after identifying target-producing storage.
