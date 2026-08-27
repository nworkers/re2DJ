# Win32 오디오 음량 추적 작업 지시

## 한국어

### 목표

`--audio-volume-trace`로 DirectSound buffer 음량, PCM 레벨, WINMM mixer 제어를 한 실행에서 관찰하여 작은 출력 음량의 원인을 식별한다.

### 작업 범위

1. 별도 audio trace writer와 제한 정책을 injected runtime에 추가한다.
2. DirectSound facade에 buffer 생성, `SetVolume`, 최초 `Play` PCM peak/RMS 추적을 추가한다.
3. WINMM mixer API pass-through wrapper를 추가하고 원본 실행 파일의 해당 IAT를 교체한다.
4. 제품 CLI, launcher option, `OriginalProcessOptions`를 연결한다.
5. 자산 없는 probe, Win32 빌드, CTest로 검증한다.
6. 실제 HDD 실행 추적을 분석하고 analysis 문서 및 작업 로그를 갱신한다.

### 완료 조건

- trace를 끄면 기존 실행 동작과 로그가 바뀌지 않는다.
- trace를 켜면 원본 자산 바이트 없이 DirectSound dB/linear gain, PCM peak/RMS와 사용된 WINMM mixer 값을 확인할 수 있다.
- 실제 실행 결과를 **확인됨 / 추정 / 미확정**으로 구분하여 문서화한다.

## English

### Goal

Use `--audio-volume-trace` to observe DirectSound buffer volume, PCM level, and WINMM mixer controls in one run and identify the cause of low output volume.

### Scope

1. Add a dedicated bounded audio trace writer to the injected runtime.
2. Trace DirectSound buffer creation, `SetVolume`, and first-`Play` PCM peak/RMS in the facade.
3. Add pass-through WINMM mixer wrappers and replace the corresponding imports in the original executable.
4. Connect the product CLI, launcher option, and `OriginalProcessOptions`.
5. Verify with an asset-free probe, Win32 build, and CTest.
6. Analyze one real-HDD execution and update analysis documentation and the work log.

### Completion criteria

- Existing execution and logging remain unchanged when tracing is disabled.
- When enabled, the trace exposes DirectSound dB/linear gain, PCM peak/RMS, and used WINMM mixer values without logging original asset bytes.
- Real-run conclusions are documented as **confirmed, inferred, or unresolved**.
