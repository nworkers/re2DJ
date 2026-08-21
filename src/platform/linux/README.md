# src/platform/linux

Linux x86-64 전용 backend가 들어갈 자리입니다. 아직 비어 있습니다.

*Linux x86-64 backend. Empty for now.*

여기서만 POSIX 헤더를 포함할 수 있습니다. 대소문자를 구분하는 파일 시스템 때문에 게스트 경로 해석은 `re2dj::hdd::HddRoot`를 반드시 거쳐야 합니다.

*This is the only place that may include POSIX headers. Because the file system is case-sensitive, guest path resolution must always go through `re2dj::hdd::HddRoot`.*
