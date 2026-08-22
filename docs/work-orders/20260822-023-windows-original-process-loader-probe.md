# Windows Original-Process Loader Probe

## 한국어

1. target profile과 HDD root로 `ez2dj1.exe`를 찾고 PE32 image base를 파싱합니다.
2. x64 Windows probe가 원본 EXE를 `CREATE_SUSPENDED`로 생성합니다.
3. `ProcessWow64Information`과 child WOW64 PEB의 image-base field로 main module base를 읽어 parsed PE image base와 비교합니다.
4. guest thread를 resume하지 않고 child를 종료합니다.
5. Windows build, 실제 HDD probe, 결과 작업 로그를 남깁니다.

## English

1. Find `ez2dj1.exe` through the target profile and HDD root, then parse its PE32 image base.
2. Have the x64 Windows probe create the original EXE with `CREATE_SUSPENDED`.
3. Read the main-module base through `ProcessWow64Information` and the child WOW64 PEB image-base field, then compare it with the parsed PE image base.
4. Terminate the child without resuming its guest thread.
5. Run the Windows build and live HDD probe, then write the result log.
