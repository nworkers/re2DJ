# third_party

vendoring한 서드파티 의존성이 들어 있는 디렉터리입니다. 현재 `libchdr/`가 MAME CHD read-only 판독을 제공합니다.

*Vendored third-party dependencies. `libchdr/` currently provides read-only MAME CHD access.*

GPL, LGPL, AGPL 같은 전염성 라이선스 코드는 도입하지 않습니다. 추가할 때는 [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)에 이름·버전·라이선스·경로를 같은 작업에서 기록합니다.

*Copyleft-licensed code such as GPL, LGPL, or AGPL is not introduced. When something is added, record its name, version, license, and path in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md) in the same task.*

## Current contents

`libchdr/` is the vendored BSD-3-Clause CHD reader used by the platform-neutral
storage adapter. Its LZMA, miniz, Zstandard, and dr_flac decoder dependencies
are kept under the same directory with their upstream license notices.

*`libchdr/` is the vendored BSD-3-Clause CHD reader used by the platform-neutral
storage adapter. Its LZMA, miniz, Zstandard, and dr_flac decoder dependencies
are kept under the same directory with their upstream license notices.*
