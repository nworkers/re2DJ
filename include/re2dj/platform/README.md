# include/re2dj/platform

플랫폼 backend가 구현해야 할 **인터페이스 헤더**가 들어갈 자리입니다.

*Interface headers that every platform backend implements.*

창, 렌더 표면, 오디오 출력, 입력, 시간 소스의 추상 인터페이스를 여기에 두고, 구현은 `src/platform/<플랫폼>/`에 둡니다. 이 헤더에는 호스트 OS 타입이 등장하지 않습니다.

*Abstract interfaces for the window, render surface, audio output, input, and time source live here; implementations live under `src/platform/<platform>/`. No host OS type appears in these headers.*
