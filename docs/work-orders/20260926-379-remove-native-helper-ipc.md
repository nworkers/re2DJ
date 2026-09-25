# 작업 379 작업 지시서 — native helper IPC 제거 / Task 379 work order — remove the native helper IPC

설계: [20260926-379-remove-native-helper-ipc.md](../design/20260926-379-remove-native-helper-ipc.md)

## 절차 / Steps

1. helper 계열 파일을 두 host에서 지운다. 남는 코드에서 참조가 없는지 확인한다.
   *Delete the helper family on both hosts and confirm nothing left references it.*
2. protocol header를 지우고, Linux import thunk가 쓰던 상수를 옮긴다.
   *Delete the protocol header and move the constants the Linux import thunks used.*
3. `original_runner`와 CLI에서 helper 경로를 지운다.
   *Remove the helper path from `original_runner` and the CLI.*
4. CMake 옵션·조건·target과 preset, script를 정리한다.
   *Clean up the CMake options, conditions, and targets, the presets, and the script.*
5. 두 host build·CTest, Linux probe·진단·실제 4th 회귀, 문서.
   *Build and CTest on both hosts, the Linux probe, diagnostic, and real-4th regressions, and documentation.*

## 완료 조건 / Done when

- helper·IPC·protocol 참조가 code·CMake·preset에 없다.
  *No code, CMake, or preset references the helper, IPC, or protocol.*
- Windows x86과 Linux 두 폭이 build·CTest를 통과하고, Linux probe·진단·실제 4th 결과가 전과 같다.
  *Windows x86 and both Linux widths build and pass CTest, and the Linux probes, diagnostics, and real 4th behave as before.*
