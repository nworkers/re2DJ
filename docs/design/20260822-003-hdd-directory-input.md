# HDD 디렉터리 입력 설계

## 배경

사용자 요구사항은 "원본 HDD 내용을 디렉터리 경로로 입력받는다"이다. rePIU가 MAME ROM ZIP과 CHD 이미지를 다루는 것과 달리, re2DJ는 이미 파일 시스템 형태로 풀린 내용을 받는다.

디스크 이미지 마운트를 구현하지 않는 이유는 세 가지다. 첫째, 이미지 형식이 여러 가지이고 파티션·파일 시스템 파서가 필요하다. 둘째, 그 코드는 게임 실행과 무관한 순수 비용이다. 셋째, 사용자가 이미 추출한 디렉터리를 갖고 있는 경우가 대부분이다.

## 문제: 대소문자

원본은 Windows에서 동작했으므로 게임 코드가 `DATA\Song01.EZ`처럼 실제 파일명과 대소문자가 다른 문자열로 파일을 열어도 성공했다. Linux와 Web 호스트의 파일 시스템은 대소문자를 구분하므로 그대로 넘기면 열리지 않는다.

따라서 게스트 상대 경로를 호스트 실제 경로로 바꾸는 계층이 필요하다.

## 설계: HddRoot

`re2dj::hdd::HddRoot`가 사용자 디렉터리를 소유한다.

* `Open()`이 경로 존재와 디렉터리 여부를 검증하고 `weakly_canonical`로 정규화한다.
* `Resolve()`가 `/`로 구분된 상대 경로를 구성 요소별로 해석한다.
* 각 구성 요소는 **부모 디렉터리 나열 결과와 대조한다.** 정확히 일치하는 항목이 있으면 그것을 쓰고, 없으면 대소문자 무시로 처음 일치한 항목을 쓴다.
* 나열 결과는 디렉터리 단위로 캐시한다.

요청한 철자를 먼저 그대로 시도하는 편이 빠르지만 그렇게 하지 않는다. Windows에서는 파일 시스템이 대소문자를 구분하지 않아 성공하고 요청한 철자를 그대로 돌려주므로, 같은 덤프에 대해 Windows와 Linux가 서로 다른 경로 문자열을 내놓는다. 호스트마다 로그와 비교 결과가 달라지는 것은 공용 코어에 플랫폼 가정을 넣지 않는다는 규칙과 정면으로 어긋난다.

대소문자 접기는 ASCII 범위에만 적용한다. 원본이 한국어 Windows에서 동작했으므로 파일명에 CP949 바이트열이 들어 있을 수 있고, 0x80 이상 바이트를 접으면 서로 다른 음절이 같은 것으로 취급될 수 있다.

## 설계: GuestPath

게스트가 넘기는 경로는 Win32 문법이다. `re2dj::storage::GuestPath`가 다섯 가지 형태를 구분한다: 드라이브 절대, 드라이브 상대, 루트 상대, 순수 상대, UNC. UNC는 자체 완결적인 HDD 덤프에 의미가 없으므로 거부한다.

정규화는 `.`을 제거하고 `..`을 접으며 Win32처럼 끝의 점과 공백을 떼어낸다. 루트를 벗어나는 `..`은 **클램프하지 않고 실패로 처리한다.** 조용히 루트로 붙이면 게스트가 의도한 것과 다른 파일을 열게 되고, 실패로 처리하면 그 시도가 로그에 남는다.

## 설계: 쓰기 정책

게스트의 파일 쓰기는 원본 디렉터리를 변경하지 않는다. 쓰기는 overlay 디렉터리로 향하고, 읽기는 overlay를 먼저 조회한 뒤 원본으로 내려간다. 사용자의 덤프가 실행으로 훼손되지 않고, 실행 상태를 지우려면 overlay만 지우면 된다.

이번 작업에서는 정책만 정하고 구현은 Stage 5로 미룬다. 파일 핸들 계층이 아직 없다.

## 설계: 타깃 프로파일

`DetectTargetProfiles()`가 스캔 결과에서 프로파일을 동적으로 구성한다. 내장 프로파일 목록은 의도적으로 비워 둔다. 원본 덤프를 확인하기 전에 버전별 경로를 추측해 넣으면 그것이 나중에 사실처럼 인용된다.

## Background

The requirement is that original HDD contents arrive as a directory path. Unlike rePIU, which handles MAME ROM ZIPs and CHD images, re2DJ receives content already extracted to a file system. Disk image mounting is not implemented because image formats are many and would need partition and file-system parsers, that code is pure cost unrelated to running the game, and users usually already have an extracted directory.

## The case problem

The original ran on Windows, so game code could open `DATA\Song01.EZ` with a case that does not match the real file name. Linux and Web hosts are case-sensitive and the open would fail, so a translation layer is required.

## HddRoot

`HddRoot` owns the user directory: `Open()` validates and canonicalises it, and `Resolve()` walks a `/`-separated relative path component by component, matching each against the parent directory's cached listing — exact match first, then the first case-insensitive match.

Probing the requested spelling first would be faster but is not done. On Windows the case-insensitive file system would succeed and return the requested spelling, so one dump would produce different path strings on Windows and Linux. Host-dependent logs and comparisons contradict the rule against platform assumptions in the shared core.

Case folding applies to ASCII only. The original ran on Korean Windows, so file names may hold CP949 byte sequences, and folding bytes at or above 0x80 could make different syllables compare equal.

## GuestPath

`GuestPath` distinguishes drive-absolute, drive-relative, root-relative, plain relative, and UNC forms, rejecting UNC as meaningless for a self-contained dump. Normalisation drops `.`, folds `..`, and strips trailing dots and spaces as Win32 does. A `..` that escapes the root **fails rather than being clamped**: silently pinning it to the root would open a different file than the guest meant, while failing leaves the attempt in the log.

## Write policy

Guest writes never modify the original directory. Writes go to an overlay and reads consult it first, so a user's dump is never damaged by a run and clearing run state means deleting only the overlay. This task fixes the policy; implementation waits for Stage 5, since no file handle layer exists yet.

## Target profiles

`DetectTargetProfiles()` builds profiles from the scan. The built-in list is deliberately empty, because a per-version path guessed before a dump is inspected would later be cited as fact.
