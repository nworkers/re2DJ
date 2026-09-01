# 서드파티 고지 / Third-Party Notices

`third_party/libchdr/`에는 MAME CHD 읽기를 위해 vendoring한 `libchdr`와 허용된 codec dependency가 있습니다. 공용 graphics build와 Windows x86 audio build는 아래 소스를 CMake FetchContent로 고정 commit에서 내려받으며 source distribution의 license notice를 그대로 유지합니다.

`AGENTS.md`의 라이선스 정책에 따라 GPL, LGPL, AGPL 같은 전염성 라이선스 코드는 도입하지 않습니다.

*`third_party/libchdr/` vendors `libchdr` and its permissively licensed codec dependencies for MAME CHD reads. The shared graphics build and Windows x86 audio build fetch the sources below from pinned commits through CMake FetchContent and preserve each source distribution's license notice. Per the license policy in `AGENTS.md`, copyleft-licensed code such as GPL, LGPL, or AGPL is not introduced.*

| 이름 / Name | 버전 / Version | 라이선스 / License | 경로 / Path |
| --- | --- | --- | --- |
| SDL | 3.4.14 (`147a8ee3`) | zlib | [upstream license](https://github.com/libsdl-org/SDL/blob/release-3.4.14/LICENSE.txt) |
| SDL_mixer | 3.2.4 (`72a81869`) | zlib | [upstream license](https://github.com/libsdl-org/SDL_mixer/blob/release-3.2.4/LICENSE.txt) |
| libchdr | current master snapshot (`rtissera/libchdr`) | BSD-3-Clause; codec dependencies listed below | [upstream license](https://github.com/rtissera/libchdr/blob/master/LICENSE.txt) |
| LZMA decoder (libchdr dependency) | 26.02 | Public domain | [`third_party/libchdr/deps/lzma-26.02/LICENSE`](third_party/libchdr/deps/lzma-26.02/LICENSE) |
| miniz (libchdr dependency) | 3.1.2 | Public domain / Unlicense terms in source | [`third_party/libchdr/deps/miniz-3.1.2/miniz.h`](third_party/libchdr/deps/miniz-3.1.2/miniz.h) |
| Zstandard decoder (libchdr dependency) | 1.5.7 | BSD-3-Clause terms selected from the upstream dual-license notice | [`third_party/libchdr/deps/zstd-1.5.7/zstddeclib.c`](third_party/libchdr/deps/zstd-1.5.7/zstddeclib.c) |
| dr_flac (libchdr dependency) | 0.13.3 | Public domain / MIT-0 terms in source | [`third_party/libchdr/include/dr_libs/dr_flac.h`](third_party/libchdr/include/dr_libs/dr_flac.h) |
