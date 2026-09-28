# 작업 411 설계 — GetPrivateProfileSectionNamesA / Task 411 design — GetPrivateProfileSectionNamesA

선행: [작업 410 설계](20260927-410-private-profile-string.md)

## 배경 / Background

작업 410 뒤 Linux의 EZ2DJ 1st는 `kernel32!GetPrivateProfileSectionNamesA`에서 멈췄다. Windows 기록에서 1st는 이 함수로 `Songs\music.ini`의 섹션 이름(곡 목록)을 읽고, 결과는 167이다.

*After Task 410, EZ2DJ 1st on Linux stopped at `kernel32!GetPrivateProfileSectionNamesA`, with which, per the Windows log, it reads the section names (the song list) of `Songs\music.ini`, getting 167.*

### 측정 / Measurements

Windows 11(32비트)에서 측정했다.

*Measured on Windows 11 (32-bit).*

| 경우 / Case | 결과 / Result |
| --- | --- |
| 목록 / listing | `GetPrivateProfileStringA(NULL, ...)`와 같음: 모든 섹션(같은 이름이 또 나와도), 앞뒤 공백 없이, 이름마다 NUL과 끝 NUL, last error 0 / *as `GetPrivateProfileStringA(NULL, ...)`: every section, repeats included, without surrounding spaces, NUL-ended, one more NUL, last error 0* |
| 버퍼가 모자람 / short buffer | `size − 2`, 234. 크기 1이면 NUL 하나와 0 / *`size − 2`, 234; a size of 1 gets one NUL and 0* |
| 파일 없음 / no file | NUL 하나만 쓰고 0, 오류 2(디렉터리 없음은 3) / *one NUL and 0, error 2 (3 for a missing directory)* |

## 결정 / Decisions

kernel32 `GetPrivateProfileSectionNamesA`는 작업 410의 `ReadProfileFile`, `ListPrivateProfileSections`, `CopyPrivateProfileList`를 쓴다. 파일을 열 수 없을 때만 NUL 하나를 쓴다. NULL 버퍼와 NULL 파일 이름은 멈춘다.

*kernel32 `GetPrivateProfileSectionNamesA` uses Task 410's `ReadProfileFile`, `ListPrivateProfileSections`, and `CopyPrivateProfileList`, writing a single NUL only when the file cannot be opened. A null buffer or file name stops.*
