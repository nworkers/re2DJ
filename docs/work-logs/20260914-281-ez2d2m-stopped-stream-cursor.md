# 작업 로그: EZ2Dancer 정지 streaming cursor 동기화

## 한국어

### 분석 결과

`20260914-004813-801` 로그에서 재생 중 `Stop` 후 `SetCurrentPosition(0)`이 호출되며,
적용 위치는 0이지만 정지된 SDL track의 이전 mixer frame이 `cursor-after`로 계속
보고되는 문제를 확인했습니다.

### 변경 사항

- 정지된 SDL streaming voice의 `PositionBytes`가 DirectSound buffer cursor를 반환하도록
  수정
- `requested=12`, `applied=12`, `cursor-after=12` 회귀 검사 추가
- 정지 상태 cursor 동기화 설계·분석 문서 추가 및 누적 분석 문서 갱신

### 검증

- `cmd /c scripts\build_win32.bat` 성공
- `re2dj_ez2dj_keyboard_input_test` 통과
- `re2dj_ez2dancer_keyboard_input_test` 통과
- `re2dj_windows_product_loader_probe` 통과
- `re2dj_unit_tests` 통과
- `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` 종료 검증 통과
- 전체 VFS runtime probe의 기존 GUI/audio lifecycle 대기는 별도 미해결 항목으로 남아
  있습니다.

### 결론

이번 수정은 로그로 확인된 cursor 보고 오류를 해결합니다. JAM 무음의 최종 해결 여부는
새 빌드로 곡을 실행하고 `directsound:set-position`의 `cursor-after` 및 실제 청취를
확인해야 합니다.

## English

### Analysis result

Run `20260914-004813-801` calls `SetCurrentPosition(0)` after a playing `Stop`. The
requested position is applied to the DirectSound buffer, but the previous mixer frame of
the stopped SDL track remains visible as `cursor-after`.

### Changes

- Make stopped SDL streaming voices return the DirectSound buffer cursor from
  `PositionBytes`.
- Add a regression check for `requested=12`, `applied=12`, and `cursor-after=12`.
- Add the stopped-cursor design and update the cumulative analysis document.

### Verification

- `cmd /c scripts\\build_win32.bat` succeeded.
- `re2dj_ez2dj_keyboard_input_test` passed.
- `re2dj_ez2dancer_keyboard_input_test` passed.
- `re2dj_windows_product_loader_probe` passed.
- `re2dj_unit_tests` passed.
- `re2dj_windows_vfs_runtime_probe.exe --audio-exit-child` exited successfully.
- The existing full VFS runtime probe GUI/audio lifecycle wait remains a separate unresolved
  item.

### Conclusion

This change fixes the cursor-reporting error confirmed by the log. Final confirmation of
the JAM silence requires a fresh build run, the corrected `cursor-after` trace, and audible
playback verification.
