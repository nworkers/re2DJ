# 작업 094 — Win32 SDL 오디오 process-lifetime 결과

## 구현

`Sdl3MixerAudioBackend::Instance()`를 함수 정적 객체에서 process-lifetime allocation으로 변경했다. pointer 자체만 정적으로 보존되며 backend 소멸자는 C++ atexit에 등록되지 않는다. 따라서 Windows process shutdown의 DLL detach 단계에서는 `MIX_Quit`과 `SDL_QuitSubSystem(SDL_INIT_AUDIO)`가 호출되지 않는다. 원본 `ExitProcess` import와 exit code는 바꾸지 않았다.

Windows runtime probe에는 `--audio-exit-child`를 추가했다. child는 `SDL_AUDIODRIVER=wasapi`를 선택하고 DirectSound HLE를 초기화·release한 뒤 native `ExitProcess(0)`을 호출한다. parent는 5초 timeout과 exit code 0을 검증하며 timeout일 때 해당 test child만 종료한다.

## 검증

- 수정 전 Debug runtime probe: 5.23초 뒤 실패, `audio backend blocked native process exit`
- 수정 후 같은 probe: 0.56초, 통과
- Debug build 성공, CTest 3/3 통과
- Release build 성공, CTest 3/3 통과
- 제품 실행 `20260829-233725-840`: PID 20768은 약 100초 뒤 thread 1개 교착 없이 종료되고 parent PID 27820도 종료
- launcher 결과: `runtime_detached_exit` code `0xc0000005`, outcome success
- Windows Application Error: fault module unknown, fault offset `0x00000000`; WER dump `ez2dj.exe.20768.dmp`

오디오 종료 교착은 해결됐다. 새 exit code는 교착 뒤에 가려져 있던 별도 execute-at-zero access violation이며 작업 095에서 dump로 귀속한다. 원본 자산과 dump·runtime trace는 커밋하지 않았다. 이전 교착 process PID 21140/13068은 진단·수정 중 종료하지 않았으며 최종 확인 시에는 더 이상 존재하지 않았다.

---

# Task 094 — Win32 SDL audio process-lifetime result

## Implementation

`Sdl3MixerAudioBackend::Instance()` now uses a process-lifetime allocation instead of a function-static object. Only the pointer is static; the backend destructor is not registered with C++ atexit. DLL detach during Windows process shutdown therefore does not call `MIX_Quit` or `SDL_QuitSubSystem(SDL_INIT_AUDIO)`. The original `ExitProcess` import and exit code remain unchanged.

The Windows runtime probe adds `--audio-exit-child`. The child selects `SDL_AUDIODRIVER=wasapi`, initializes and releases the DirectSound HLE, then calls native `ExitProcess(0)`. The parent requires exit code zero within five seconds and terminates only that test child on timeout.

## Verification

- Pre-fix Debug runtime probe: failed after 5.23 seconds with `audio backend blocked native process exit`
- Post-fix identical probe: passed in 0.56 seconds
- Debug build passed; CTest 3/3 passed
- Release build passed; CTest 3/3 passed
- Product run `20260829-233725-840`: PID 20768 exits after roughly 100 seconds without the former one-thread deadlock, and parent PID 27820 also exits
- Launcher result: `runtime_detached_exit` code `0xc0000005`, outcome success
- Windows Application Error: unknown module, fault offset `0x00000000`; WER dump `ez2dj.exe.20768.dmp`

The audio shutdown deadlock is fixed. The new exit code is a separate execute-at-zero access violation previously hidden behind the deadlock; task 095 will attribute it from the dump. Original assets, the dump, and runtime traces were not committed. The old deadlocked processes PID 21140/13068 were not terminated during diagnosis or implementation and were no longer present at the final check.
