# 작업 410 설계 — GetPrivateProfileStringA / Task 410 design — GetPrivateProfileStringA

선행: [작업 405 설계](20260927-405-private-profile-int.md)

## 배경 / Background

작업 409 뒤 Linux의 EZ2DJ 1st는 `bookkeeping.ini`를 정수로 읽은 다음 `kernel32!GetPrivateProfileStringA`에서 멈췄다. Windows 기록에서 1st는 이 함수로 `.\ez2dj.ini`의 `[DIFFICULTY]` 값들을 읽는다.

*After Task 409, EZ2DJ 1st on Linux read `bookkeeping.ini`'s numbers and stopped at `kernel32!GetPrivateProfileStringA`, with which, per the Windows log, it reads `[DIFFICULTY]` from `.\ez2dj.ini`.*

### 측정 / Measurements

Windows 11(32비트)에서 측정했다. 섹션·키를 찾는 규칙은 `GetPrivateProfileIntA`와 같다(작업 405).

*Measured on Windows 11 (32-bit); finding sections and keys follows `GetPrivateProfileIntA`'s rules (Task 405).*

| 경우 / Case | 결과 / Result |
| --- | --- |
| 값을 찾음 / found | 양끝이 같은 따옴표(`"` 또는 `'`)면 벗김. 한쪽만 있으면 그대로. 길이 반환, last error 0 / *one pair of matching quotes removed, a lone one kept; the length, last error 0* |
| 버퍼보다 긺 / longer than the buffer | `size − 1`로 자름, `ERROR_MORE_DATA`(234). 정확히 `size − 1`이어도 234 / *cut to `size − 1`, 234, also when exactly that long* |
| 없음 / missing | 기본값의 끝 공백을 떼고 복사. 기본값의 따옴표는 그대로, NULL 기본값은 빈 문자열. 오류 2(디렉터리 없음은 3). 잘려도 234가 아님 / *the default without trailing spaces, quotes kept, empty when null; 2 (3 for a missing directory), not 234 when cut* |
| NULL 키 / null key | 섹션의 키 목록. 각 이름 뒤 NUL, 끝에 NUL 하나 더. 끝 NUL을 뺀 길이 반환. 잘리면 `size − 2`와 234 / *the section's keys, NUL after each and one more; the length without the last NUL; cut: `size − 2` and 234* |
| NULL 섹션 / null section | 섹션 이름 목록, 같은 형식 / *the section names, in the same form* |

## 결정 / Decisions

1. 공용 INI core(`private_profile.h`)에 키·섹션 목록, 값과 기본값의 문자열 규칙, 버퍼 복사(문자열·목록)를 더한다.
   *The shared INI core (`private_profile.h`) gains the key and section lists, the text rules for values and defaults, and the buffer copies for a string and a list.*
2. kernel32의 파일 읽기를 `ReadProfileFile`로 떼어 `GetPrivateProfileIntA`와 함께 쓴다. `GetPrivateProfileIntA`의 동작은 바뀌지 않는다.
   *kernel32's file reading moves into `ReadProfileFile`, shared with `GetPrivateProfileIntA`, whose behaviour does not change.*
3. NULL 버퍼나 NULL 파일 이름은 멈춘다. 없는 섹션이나 파일을 목록으로 요청하는 경우도 측정하지 않았으므로 멈춘다.
   *A null buffer or file name, and a list asked of a missing section or file (not measured), stop.*
