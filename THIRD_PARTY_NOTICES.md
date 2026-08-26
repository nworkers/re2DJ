# 서드파티 고지 / Third-Party Notices

현재 `third_party/` 아래에 vendoring된 의존성은 없습니다. Windows x86 audio build는 아래 소스를 CMake FetchContent로 고정 commit에서 내려받으며 source distribution의 license notice를 그대로 유지합니다.

`AGENTS.md`의 라이선스 정책에 따라 GPL, LGPL, AGPL 같은 전염성 라이선스 코드는 도입하지 않습니다.

*No dependencies are vendored under `third_party`. The Windows x86 audio build fetches the sources below from pinned commits through CMake FetchContent and preserves each source distribution's license notice. Per the license policy in `AGENTS.md`, copyleft-licensed code such as GPL, LGPL, or AGPL is not introduced.*

| 이름 / Name | 버전 / Version | 라이선스 / License | 경로 / Path |
| --- | --- | --- | --- |
| SDL | 3.4.14 (`147a8ee3`) | zlib | [upstream license](https://github.com/libsdl-org/SDL/blob/release-3.4.14/LICENSE.txt) |
| SDL_mixer | 3.2.4 (`72a81869`) | zlib | [upstream license](https://github.com/libsdl-org/SDL_mixer/blob/release-3.2.4/LICENSE.txt) |
