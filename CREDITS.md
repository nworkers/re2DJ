# Credits

## 한국어

re2DJ는 아래의 게임, 프로젝트, 사람들 위에 서 있습니다. 법적 고지와 라이선스 전문
위치는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에, re2DJ 자체의 라이선스는
[LICENSE](LICENSE)에 있습니다.

### 원작

* **EZ2DJ**는 1999년 Amuse World가 처음 선보인 아케이드 리듬 게임이며, **EZ2Dancer**는
  같은 계보의 댄스 게임입니다. EZ2DJ·EZ2Dancer와 관련된 모든 상표와 저작물의 권리는
  각 권리자에게 있습니다. re2DJ는 비공식 보존·연구 프로젝트로 권리자와 아무 관계가
  없으며, 원본 실행 파일·HDD 덤프·CHD를 포함하거나 배포하지 않습니다. 이 게임들을
  만들고 지금까지 이어온 모든 분들께 감사드립니다.

### 참조한 프로젝트

* **MAME** (https://www.mamedev.org/) — re2DJ가 읽는 CHD 디스크 이미지 포맷과, 이
  게임들이 지금까지 남아 있게 한 수십 년의 아케이드 보존 작업에 감사드립니다. MAME
  코드는 이식하지 않았으며, CHD 읽기는 아래 libchdr로 합니다.

### 사용하는 오픈소스

실행 파일에 포함되거나 링크됩니다.

* **SDL** — Sam Lantinga와 기여자들 (zlib)
* **SDL_mixer** — Sam Lantinga와 기여자들 (zlib)
* **Dear ImGui** — Omar Cornut와 기여자들 (MIT)
* **spdlog** — Gabi Melman과 기여자들 (MIT)
* **libchdr** — Romain Tisserand와 기여자들, MAME CHD 포맷 (BSD 3-Clause; 내장 LZMA SDK,
  miniz, zstd, dr_flac 포함)
* **GNU Unifont** — Roman Czyborra, Paul Hardy와 기여자들 (SIL OFL 1.1; ASCII 글리프
  데이터만 사용)

### 개발·분석 도구

* **Capstone** — 분석 스크립트의 x86 디코딩에 사용 (BSD 3-Clause; 저장소에는 포함되지
  않는 스크립트 의존성)

### 사이트와 문서

* **Galmuri** — quiple의 한글 픽셀 폰트 (SIL OFL 1.1)
* **Jinja** — Pallets (BSD 3-Clause)
* **markdown-it-py** — Executable Books (MIT)
* **Mermaid** — Knut Sveidqvist와 기여자들 (MIT)

### 도구

이 프로젝트의 분석과 구현 전반에 Anthropic의 Claude가 함께했습니다.

---

## English

re2DJ stands on the games, projects and people below. The legal notices and the locations
of the full licence texts are in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); re2DJ's
own licence is [LICENSE](LICENSE).

### The original games

* **EZ2DJ** is the arcade rhythm game first released by Amuse World in 1999, and
  **EZ2Dancer** is the dance game of the same lineage. All trademarks and works related to
  EZ2DJ and EZ2Dancer belong to their respective rights holders. re2DJ is an unofficial
  preservation and research project with no affiliation to the rights holders, and neither
  contains nor distributes original executables, HDD dumps or CHDs. Our thanks to everyone
  who made these games and has carried them this far.

### Referenced projects

* **MAME** (https://www.mamedev.org/) — our thanks for the CHD disc-image format that
  re2DJ reads, and for decades of arcade preservation that kept these games alive. No MAME
  code is adapted; CHD reading is done by libchdr below.

### Open source in use

Included in or linked into the executables:

* **SDL** — Sam Lantinga and contributors (zlib)
* **SDL_mixer** — Sam Lantinga and contributors (zlib)
* **Dear ImGui** — Omar Cornut and contributors (MIT)
* **spdlog** — Gabi Melman and contributors (MIT)
* **libchdr** — Romain Tisserand and contributors, the MAME CHD format (BSD 3-Clause;
  bundles the LZMA SDK, miniz, zstd and dr_flac)
* **GNU Unifont** — Roman Czyborra, Paul Hardy and contributors (SIL OFL 1.1; ASCII glyph
  data only)

### Development and analysis tools

* **Capstone** — x86 decoding in the analysis scripts (BSD 3-Clause; a script dependency
  not included in the repository)

### Site and documentation

* **Galmuri** — quiple's pixel font with Hangul (SIL OFL 1.1)
* **Jinja** — Pallets (BSD 3-Clause)
* **markdown-it-py** — Executable Books (MIT)
* **Mermaid** — Knut Sveidqvist and contributors (MIT)

### Tools

Anthropic's Claude worked alongside the analysis and implementation throughout this
project.
