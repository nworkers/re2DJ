# include/re2dj/config

설정 파싱의 **공개 헤더**가 들어갈 자리입니다.

*Public headers for configuration parsing.*

INI 문서 파서, 키 바인딩, 타깃별 실행 옵션이 들어옵니다. 사용자 설정 파일은 `cfg/`에 생성되며 저장소에는 커밋되지 않습니다.

*An INI document parser, key bindings, and per-target run options. User configuration files are generated under `cfg/` and are never committed.*

`hardlock_secret_config.h`는 선택 프로파일의 Hardlock module address와 세 seed를 기본 `cfg/hardlock.ini` 또는 명시적인 외부 INI에서 읽는 공개 경계를 제공합니다. parser는 Git work tree 내부에서 `cfg/` 밖의 경로를 거부하며 오류에 설정값을 포함하지 않습니다.

*`hardlock_secret_config.h` exposes the boundary that reads a selected profile's Hardlock module address and three seeds from default `cfg/hardlock.ini` or an explicit external INI. Inside a Git work tree, the parser rejects paths outside `cfg/` and never includes configuration values in errors.*
