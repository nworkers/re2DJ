# 작업 지시 331: Linux x86 in-process runner probe / Work order 331: Linux x86 in-process runner probe

Linux x86 전용 probe가 synthetic PE를 `NativeInProcessRunner`로 실행해 두 import completion과 SIGILL fault 전달을 확인하게 합니다. 기존 fixture의 PE generator를 재사용하고, x86 product preset 빌드에서 probe를 실행합니다.

*Add a Linux x86-only probe that runs synthetic PE through `NativeInProcessRunner` and checks two import completions plus SIGILL fault propagation. Reuse the existing fixture PE generator and run the probe from the x86 product preset build.*
