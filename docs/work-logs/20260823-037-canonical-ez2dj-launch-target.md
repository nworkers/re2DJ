# 정식 ez2dj.exe launcher 대상 결과

## 결과

Windows x86 launcher probe의 기본 target을 `ez2dj1stse`로 바꿨습니다. 따라서 `--hdd roms/ez2dj1stse`는 이제 `System.ini`의 `shell=` 값으로 확인된 canonical `ez2dj.exe`를 선택합니다. `bring_up_target`은 실행 금지가 아니라 개발용 관찰 결과를 구분하는 metadata로 유지하므로, canonical profile도 launcher가 허용합니다.

초기 hardware breakpoint 관찰은 child가 `0x001affcc`로 종료했으나, 이 breakpoint 전달 경로는 신뢰할 수 없었습니다. 이후 software `INT3` breakpoint로 재검증해 static entry VA `0x01ed23cf`에 정상 도달하고 runtime injection도 성공함을 확인했습니다. 따라서 종료는 header entry 이후 경로에서 발생하며, 종료 code의 의미와 보호 기법은 아직 확인하지 않았습니다.

## English

The Windows x86 launcher probe now defaults to `ez2dj1stse`. Therefore `--hdd roms/ez2dj1stse` selects canonical `ez2dj.exe`, confirmed by the `shell=` value in `System.ini`. `bring_up_target` remains metadata that distinguishes development observations rather than an execution ban, so the launcher also permits the canonical profile.

The initial hardware-breakpoint observation ended with child exit `0x001affcc`, but this breakpoint-delivery path was not reliable. A later software `INT3` breakpoint confirmed normal arrival at static-entry VA `0x01ed23cf` and successful runtime injection. The exit therefore occurs on a path after the header entry; the exit code's meaning and protection method remain unconfirmed.
