# 작업 093 — Win32 오디오 종료 교착 분석 결과

## 결과

현재 제품 실행 `20260829-230321-304`의 살아 있는 process를 비파괴적으로 조사했다.

- `re2dj.exe` PID 13068은 자식 종료를 기다리는 `UserRequest` 대기 상태였다.
- `ez2dj.exe` PID 21140은 CPU 증가량 0, thread 1개, handle 416개였고 유일한 thread 5964는 `Wait:Unknown`이었다.
- WOW64 context의 EIP는 `ntdll!NtWaitForAlertByThreadId`였다.
- raw stack은 `WaitOnAddress`에서 SDL semaphore, WASAPI management proxy, `SDL_QuitAudio`, 정적 `Sdl3MixerAudioBackend` 소멸자, CRT atexit, `LdrShutdownProcess`, `ExitProcess`까지 이어졌다.
- 현재 `.vfs.log`의 fallback asset open은 성공했고 `.ddraw.log`의 마지막 `DrawPrimitive`도 성공했다.

따라서 화면과 입력이 함께 멈춘 직접 원인은 게임 loop의 정지가 아니라 process 종료 중 injected runtime의 audio singleton 소멸자가 이미 사라진 WASAPI 관리 thread의 응답을 기다리는 교착이다. 작업 090에서 추정으로 남았던 detach lock의 정확한 위치를 확인했다. 원본 코드가 `ExitProcess`에 진입한 최초 조건과 exit code는 이번 detached trace만으로는 미확정이다.

## 후속 구현 후보

다음 작업에서는 process-exit atexit 단계에서 SDL audio 정리를 시작하지 않도록 backend 수명 정책을 바꾸거나, 다른 thread가 살아 있을 때 명시적으로 audio를 종료하는 경계를 설계해야 한다. 실제 수정 전에는 정상 종료, 창 닫기 hard termination, 반복 실행을 각각 검증해야 한다.

---

# Task 093 — Win32 audio shutdown deadlock analysis result

## Result

The live product run `20260829-230321-304` was inspected without terminating either process.

- re2dj.exe PID 13068 was in a `UserRequest` wait for child completion.
- ez2dj.exe PID 21140 had zero CPU growth, one thread, and 416 handles; its only thread 5964 reported `Wait:Unknown`.
- The WOW64 context EIP was `ntdll!NtWaitForAlertByThreadId`.
- The raw stack continued through `WaitOnAddress`, an SDL semaphore, the WASAPI management proxy, `SDL_QuitAudio`, the static `Sdl3MixerAudioBackend` destructor, CRT atexit, `LdrShutdownProcess`, and `ExitProcess`.
- Fallback asset opens in the current VFS trace succeeded, and the final Direct3D `DrawPrimitive` calls also succeeded.

The direct cause of the simultaneous display and input stop is therefore not a stopped game loop. During process shutdown, the injected runtime's audio singleton destructor waits for a WASAPI management thread that Windows has already removed. This identifies the exact detach lock left inferred by task 090. The initial original-code condition that entered `ExitProcess`, and its exit code, remain unresolved from this detached trace.

## Follow-up implementation candidate

A subsequent task should either change backend lifetime policy so SDL audio teardown is not initiated from process-exit atexit, or introduce an explicit audio-shutdown boundary while other threads are still alive. Verification should separately cover normal exit, host-close hard termination, and repeated launches.
