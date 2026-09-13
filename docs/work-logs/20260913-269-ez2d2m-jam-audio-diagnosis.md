# 작업 로그: EZ2Dancer JAM 음원 파일시스템 진단
# Work Log: Diagnose EZ2Dancer JAM Audio Filesystem Boundary

## 한국어

### 결과

`ez2d2m`에서 `JAM` 실행 시 소리가 나지 않는 현상을 파일시스템 경계부터 점검했습니다.
현재 증거로는 `jam.ezw` 누락, CHD 손상, FAT32 파일 절단, 또는 `ez2d2m` 프로파일의
제품 루트 매핑 오류를 확인하지 못했습니다. 소스 코드는 수정하지 않았습니다.

### 확인 내용

- CHD 내부 `ez2dancer/Songs/Jam/jam.ezw`의 존재와 `17,753,894` bytes 크기를 확인했습니다.
- 파일 header의 2채널/44,100 Hz/16-bit PCM 정보와 data length가 파일 크기와 일치했습니다.
- CHD에서 직접 dump한 파일과 기존 추출본의 크기 및 SHA-256이 일치했습니다.
- 추출 트리의 `.ezw` 252개 모두 header data length와 파일 크기가 일치했습니다.
- 프로파일의 guest `C:\\ez2dancer`에서 CHD 내부 `ez2dancer`로의 매핑을 확인했습니다.
- 기존 `ez2d2m` 로그에는 실제 곡 음원인 `Songs\\Jam\\jam.ezw`가 아니라 데모 자산인
  `Songs\\DEMO1\\jam.abm`만 기록되어 있었습니다.
- 실제 JAM 재생을 포함한 `ez2d2m` `.audio.log`는 아직 확보되지 않았습니다.

### 판단 및 다음 측정

현재는 파일시스템 문제보다 원본 EZW 처리 또는 DirectSound/SDL3 HLE 경계가 남은
후보입니다. 다만 실제 `JAM` 시작 시점의 VFS와 오디오 trace 없이는 어느 쪽인지 확정할 수
없습니다. 다음 명령으로 실행한 뒤 JAM을 시작하고 10초 이상 재생해 주시면 됩니다.

```powershell
.\\build\\windows-x86\\bin\\Debug\\re2dj.exe ez2d2m --audio-volume-trace --io-config .\\config\\ez2dancer-io.example.ini
```

게임 종료 후 `logs\\windows_x86_launcher_probe\\ez2d2m`의 최신 `.vfs.log`와 같은 이름의
`.audio.log`를 대조합니다. 이번 진단 중 CHD dump용 임시 파일은 확인 후 작업공간에서
삭제했습니다.

### 검증

문서와 분석만 갱신했으므로 코드 빌드 및 테스트는 실행하지 않았습니다.

## English

### Result

The `JAM` silence in `ez2d2m` was investigated from the filesystem boundary first.
The current evidence shows no missing `jam.ezw`, CHD corruption, FAT32 truncation, or
incorrect `ez2d2m` product-root mapping. No source code was changed.

### Checks performed

- Confirmed `ez2dancer/Songs/Jam/jam.ezw` inside the CHD with size `17,753,894` bytes.
- Confirmed that its 2-channel/44,100 Hz/16-bit PCM header and data length match the file size.
- Compared a direct CHD dump with the existing extracted copy by size and SHA-256; they match.
- Checked all 252 extracted `.ezw` files; every header data length matches its file size.
- Confirmed the profile mapping from guest `C:\\ez2dancer` to CHD-internal `ez2dancer`.
- Existing `ez2d2m` logs contain the demo asset `Songs\\DEMO1\\jam.abm`, not the actual
  track audio path `Songs\\Jam\\jam.ezw`.
- No `ez2d2m` `.audio.log` covering actual JAM playback has been collected yet.

### Assessment and next measurement

The remaining candidates are the original EZW handling and the DirectSound/SDL3 HLE
boundary, rather than the filesystem. This cannot be resolved conclusively without VFS and
audio traces covering the actual JAM start. Run the command below, start JAM, and let it play
for at least ten seconds.

```powershell
.\\build\\windows-x86\\bin\\Debug\\re2dj.exe ez2d2m --audio-volume-trace --io-config .\\config\\ez2dancer-io.example.ini
```

After closing the game, compare the newest `.vfs.log` with its same-named `.audio.log` under
`logs\\windows_x86_launcher_probe\\ez2d2m`. The temporary CHD dump used for this diagnosis
was removed after verification.

### Verification

Only documentation and analysis were updated, so no code build or test was run.
