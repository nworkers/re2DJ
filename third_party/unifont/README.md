# GNU Unifont (ASCII subset)

Linux HLE의 `DrawTextA`가 DC의 기본 글꼴로 글자를 그릴 때 쓰는 8×16 비트맵 글리프입니다. Windows는 한글 `System` 비트맵 글꼴로 그리지만, 그 글리프는 Microsoft 자산이라 넣을 수 없습니다. 그래서 모양이 비슷한 공개 글꼴을 씁니다.

*8×16 bitmap glyphs that the Linux HLE's `DrawTextA` draws a DC's default font with. Windows draws with the Korean `System` bitmap font, whose glyphs are Microsoft's and cannot be included, so a similar open font stands in.*

| 항목 / Item | 값 / Value |
| --- | --- |
| 이름 / Name | GNU Unifont |
| 버전 / Version | 15.1.05 |
| 출처 / Source | <https://unifoundry.com/pub/unifont/unifont-15.1.05/font-builds/unifont-15.1.05.hex.gz> |
| SHA-256 (`unifont-15.1.05.hex.gz`) | `e2b2e2c3c85a26e76afec499d27be66f2ebb356be6634cc2f3339e6a41026eeb` |
| 라이선스 / License | SIL Open Font License 1.1 ([`OFL-1.1.txt`](OFL-1.1.txt)) |
| 범위 / Subset | U+0020–U+007E (95 glyphs), unmodified |
| 만든 사람 / Authors | Roman Czyborra (creator), Paul Hardy (maintainer), and the Unifont contributors |

Unifont의 글꼴 파일은 SIL OFL 1.1과 GPL 2.0 이상(GNU 글꼴 임베딩 예외 포함)의 이중 라이선스입니다. 이 저장소는 OFL 1.1 조건을 택합니다. 프로젝트 정책상 GPL 코드는 들이지 않습니다. 글리프 데이터만 가져오며 Unifont의 프로그램 소스(GPL)는 쓰지 않습니다.

*Unifont's font files are dual-licensed under the SIL OFL 1.1 and GPL 2.0 or later with the GNU font embedding exception; this repository takes them under the OFL 1.1 terms, since project policy admits no GPL code. Only the glyph data is used, none of Unifont's (GPL) program sources.*

## 파일 / Files

- `unifont-15.1.05-ascii.hex`: 원본 hex 파일에서 U+0020–U+007E 줄만 그대로 옮긴 것입니다. / *The U+0020–U+007E lines of the upstream hex file, copied as they are.*
- `OFL-1.1.txt`: Unifont 배포본의 라이선스 원문입니다. / *The license text from the Unifont distribution.*

`src/hle/unifont_ascii_glyphs.inc`는 `scripts/generate_unifont_glyphs.py`가 이 hex 파일에서 만듭니다.

*`src/hle/unifont_ascii_glyphs.inc` is generated from this hex file by `scripts/generate_unifont_glyphs.py`.*
