# Win32 오디오 master gain 작업 로그

## 결과

일반 제품 실행에 기본 `+6 dB` SDL mixer master gain을 추가하고 `--audio-gain-db -24..+18`로 조절할 수 있게 했다. DirectSound buffer별 1/100 dB 변환, pan, frequency와 duplicate buffer 상태는 변경하지 않았다. 제품 CLI의 dB 값은 original-process engine에서 linear gain으로 변환되어 injected runtime export로 전달되고, DirectSound backend가 생성될 때 최종 mixer에 한 번 적용된다.

사용자 실행으로 일반 `re2dj --run`에서 보호된 원본과 오디오 출력이 동작하되 체감 음량이 지나치게 작다는 사실을 확인했다. 원본의 WINMM mixer 호출 구조 전체는 아직 runtime trace로 확인되지 않았으므로 이번 작업에서 추정 HLE하지 않았다.

## 검증

- Win32 Release 전체 build: 성공
- Win32 Release CTest: 3/3 성공
- product loader probe: 기본 `+6 dB`, 사용자 `+12 dB`, 범위 밖 값 거절과 기존 target policy 성공
- dummy-audio runtime probe: 설정값 `2.0`이 실제 SDL mixer master gain으로 적용됨을 확인
- CLI `--help`, 잘못된 숫자와 `+18 dB` 초과 거절 확인
- Linux headless `re2dj` 증분 build와 CTest: 1/1 성공

검증 당시 사용자의 기존 Debug `re2dj.exe`와 `ez2dj.exe`가 실행 중이어서 Debug output 파일은 잠겨 있었다. 실행 중 process를 임의 종료하지 않고 충돌하지 않는 Release 구성으로 전체 build와 CTest를 수행했다. 사용자 청취 검증에서는 기본 `+6 dB`와 `+12 dB`를 비교하고 clipping이나 왜곡이 있으면 값을 낮춰야 한다.

---

# Win32 Audio Master Gain Work Log

## Result

Ordinary product execution now applies `+6 dB` SDL mixer master gain by default and accepts `--audio-gain-db -24..+18`. Per-buffer DirectSound hundredths-of-a-decibel conversion, pan, frequency, and duplicate-buffer state are unchanged. The original-process engine converts product dB to linear gain, writes an injected-runtime export, and applies it once to the final mixer when the DirectSound backend is created.

A user run confirmed that the protected original and audio output work through ordinary `re2dj --run`, while perceived volume is much too quiet. The complete original WINMM mixer call structure has not yet been captured in a runtime trace and was not guessed in this task.

## Verification

- Full Win32 Release build: passed.
- Win32 Release CTest: 3/3 passed.
- Product-loader probe: default `+6 dB`, custom `+12 dB`, out-of-range rejection, and existing target policy passed.
- Dummy-audio runtime probe: confirmed configured value `2.0` is applied as actual SDL mixer master gain.
- CLI help, invalid-number rejection, and rejection above `+18 dB` passed.
- Linux headless incremental `re2dj` build and CTest: 1/1 passed.

The user's existing Debug `re2dj.exe` and `ez2dj.exe` were still running during verification, locking the Debug output files. The live processes were left untouched, and the complete build plus CTest ran in the non-conflicting Release configuration. User listening validation should compare the default `+6 dB` with `+12 dB` and reduce gain if clipping or distortion is audible.
