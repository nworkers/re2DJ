# 작업 로그: EZ2Dancer JAM 실행 trace 분석
# Work Log: Analyze EZ2Dancer JAM Runtime Trace

## 한국어

### 입력

사용자가 제공한 실행 결과:

`logs/windows_x86_launcher_probe/ez2d2m/20260913-225937-078.jsonl`

같은 실행의 `.vfs.log`, `.audio.log`, `.ddraw.log`를 함께 분석했습니다.

### 확인 결과

- VFS mount는 CHD 내부 `ez2dancer` 루트와 guest `C:\\ez2dancer`에 대해 정상입니다.
- VFS에는 CHD 파일 open 성공 492건과 read failure 기록 0건이 있습니다.
- `.ezw` 요청 34건은 모두 system 자산이며, `Songs\\Jam\\jam.ezw` 요청은 0건입니다.
- `songs/jam`에서 확인된 30건은 `bassintro.str`, `ifyou.str` 등 시각 자산입니다.
- DirectSound는 12개 buffer를 생성하고 21회 Play를 수행했으며, first-play PCM peak/RMS가
  모두 0이 아닌 값으로 기록됐습니다.
- 44.1 kHz streaming buffer `00ADBE80`은 peak `0.997924805`, RMS `0.248371771`로
  시작했습니다.

### 결론

이번 로그는 CHD 파일시스템 또는 일반적인 오디오 HLE 무음 문제를 보여주지 않습니다.
핵심은 원본이 이 실행에서 `jam.ezw`를 요청하지 않았다는 점입니다. 따라서 실제 곡
플레이 진입 전 단계에서 종료되었거나, 곡 로더의 다른 파일/API 경로가 아직 trace되지
않은 것으로 판단합니다. 소스 코드는 수정하지 않았습니다.

`jam.ezw` open/read와 그 직후 DirectSound buffer를 연결하려면 실제 게임플레이 시작이
확실한 실행 trace가 추가로 필요합니다. 이번 문서는 새 실행 증거를 기존 CHD 분석 문서에
반영한 보충 작업 로그입니다.

### 검증

진단 및 문서 갱신만 수행했으므로 코드 빌드와 테스트는 실행하지 않았습니다.

## English

### Input

The user provided:

`logs/windows_x86_launcher_probe/ez2d2m/20260913-225937-078.jsonl`

The matching `.vfs.log`, `.audio.log`, and `.ddraw.log` were analyzed together.

### Findings

- The VFS mount is correct for CHD-internal `ez2dancer` and guest `C:\\ez2dancer`.
- The VFS records 492 successful CHD file opens and zero read-failure records.
- All 34 `.ezw` requests are system assets; there are zero requests for
  `Songs\\Jam\\jam.ezw`.
- The 30 `songs/jam` matches are visual assets such as `bassintro.str` and `ifyou.str`.
- DirectSound creates 12 buffers and performs 21 Play calls, with non-zero first-play PCM
  peak/RMS measurements.
- The 44.1 kHz streaming buffer `00ADBE80` starts with peak `0.997924805` and RMS
  `0.248371771`.

### Conclusion

This trace does not show a CHD filesystem failure or a generally silent audio HLE. The key
fact is that the original does not request `jam.ezw` during this run. The run likely ended
before actual track gameplay, or the song loader uses another file/API path that is not yet
covered by the trace. No source code was changed.

A trace that definitely enters gameplay is still needed to connect a `jam.ezw` open/read with
the following DirectSound buffer. This is a supplemental work log recording the new execution
evidence in the existing CHD analysis.

### Verification

Only diagnosis and documentation were performed, so no code build or test was run.
