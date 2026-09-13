# 작업 로그: EZ2Dancer 다른 곡 EZW trace 확인
# Work Log: Check EZ2Dancer EZW Trace with Another Song

## 한국어

### 입력

사용자가 다른 곡을 실행한 launcher probe 결과와 다음 로그를 제공했습니다.

`logs/windows_x86_launcher_probe/ez2d2m/20260913-230853-991.jsonl`

동일 timestamp의 `.vfs.log`, `.audio.log`, `.ddraw.log`를 함께 확인했습니다.

### 수행 및 결과

- VFS에서 `*.ezw` 요청 34건을 추출했습니다.
- 34건은 모두 `system` 아래의 opening/title/mode/soundFX 자산이었고, CHD 매핑은 모두 성공했습니다.
- 실제 다른 곡은 `songs/arcturus`로 확인되었습니다.
- `songs/arcturus`의 `.str` 시각 자산들은 `CreateFileA`를 통해 성공적으로 열렸습니다.
- `songs/arcturus/arcturus.ezw` 요청은 확인되지 않았습니다.
- DirectSound는 13개 buffer 생성, 27건의 Play, 7건의 first-play, 10건의 streaming-start를 기록했습니다.
- PCM 측정값은 0이 아니므로 이번 실행에서도 오디오 HLE 전체 무음이나 CHD read 실패 증거는 없습니다.

### 결론

EZW 로거는 시스템 EZW 요청을 실제로 기록하고 있으므로 로거 전체가 고장난 상태는 아닙니다.
다만 곡 시각 자산 로딩 이후에도 곡별 EZW 요청이 없어서, 곡 음원이 이번 실행에서 요청되지
않았거나 현재 추적하지 않는 CRT·메모리 매핑 파일 경로를 사용했을 가능성이 남습니다.
이 결과만으로 파일시스템 오류나 DirectSound 오류를 확정하지 않았으며, 코드 수정도 하지 않았습니다.

### 검증

진단 및 문서 갱신만 수행했으므로 빌드와 테스트는 실행하지 않았습니다.

## English

### Input

The user provided a launcher probe result for another song:

`logs/windows_x86_launcher_probe/ez2d2m/20260913-230853-991.jsonl`

The matching `.vfs.log`, `.audio.log`, and `.ddraw.log` files were analyzed together.

### Work and findings

- 34 `*.ezw` requests were extracted from the VFS log.
- All 34 requests were system opening/title/mode/soundFX assets, and every CHD mapping succeeded.
- The other song is identified as `songs/arcturus`.
- Its `.str` visual assets were opened successfully through `CreateFileA`.
- No request for `songs/arcturus/arcturus.ezw` was observed.
- DirectSound recorded 13 buffer creations, 27 Play calls, seven first-play measurements, and 10 streaming starts.
- PCM measurements were non-zero, so this run provides no evidence of globally silent audio HLE or failed CHD reads.

### Conclusion

The EZW logger is not globally broken because it captures system EZW requests. However, no
per-song EZW request appears after the song visual assets load. The original may not have
requested song audio during this run, or it may use a CRT or memory-mapped file path that is
outside the currently traced boundary. This evidence does not establish a filesystem or
DirectSound defect, and no source code was changed.

### Verification

Only diagnosis and documentation were performed; no build or test was run.

### 사용자 재생 확인에 따른 정정

사용자는 이 실행에서 `arcturus` 음악이 실제로 재생되었다고 확인했습니다. 소스와 로그를
추가 대조한 결과, 일반 VFS open trace는 전체 `CreateFileA` 요청·결과를 합쳐 1,024개
이벤트까지만 기록하며, 해당 `.vfs.log`의 `create-file` 기록은 998줄에서 끝났습니다.
그 이후 `songs/arcturus`의 `.str`가 기록된 것은 별도 script trace budget이 남아 있었기
때문입니다. 따라서 `arcturus.ezw`가 로그에 없다는 사실은 파일이 요청되지 않았다는
증거가 아니라, bounded logger가 후반 곡 음원 요청을 누락했을 가능성이 큽니다.

`ReportVfsAssetOpen()`은 일반 모드에서 `.bmp`·`.str` 이외 확장자를 별도 기록하지 않으며,
`--graphics-draw-diagnostics`가 complete diagnostics를 활성화하는 경로입니다. 다음
재현에서는 이 옵션으로 곡 EZW open/read와 DirectSound stream을 함께 포착해야 합니다.
코드 수정은 아직 수행하지 않았습니다.

### Correction after user playback confirmation

The user confirmed that the `arcturus` music actually played during this run. A further
source/log comparison shows that the bounded VFS open trace records only 1,024 combined
`CreateFileA` request/result events, while this `.vfs.log` stops at 998 `create-file` lines.
The later `songs/arcturus` `.str` records remain visible because they use a separate script
trace budget. Therefore, the absence of `arcturus.ezw` is not proof that it was not requested;
the bounded logger likely omitted the later song-audio request.

`ReportVfsAssetOpen()` does not separately report non-`.bmp`/`.str` extensions in normal mode,
and `--graphics-draw-diagnostics` is the path that enables complete diagnostics. The next
reproduction should use that option to capture the song EZW open/read and its DirectSound
stream together. No source code was changed yet.
