# Hardlock vendor driver 정적 분석 작업 로그

관련 설계: [Hardlock vendor driver 정적 분석](../design/20260901-128-hardlock-vendor-driver-static-analysis.md)

*Related design: [Hardlock vendor driver static analysis](../design/20260901-128-hardlock-vendor-driver-static-analysis.md).*

## 결과

- [`haspnt64` 2022-10-21 release](https://github.com/leecher1337/haspnt64/releases/tag/20221021)가 안내한 `haspdinst.exe` 8.31만 시스템 임시 디렉터리에 내려받았습니다. Installer는 실행하지 않았으며 Authenticode 상태는 유효, signer는 `Thales DIS CPL USA, Inc.`, SHA-256은 `B3CEEF35024FE9E5FC6D8B2F658CB80F56ABCE35CF435AAAB97C35F498D437CA`였습니다.
- Installer 안의 두 CAB을 byte range로 분리한 뒤 Windows archive extractor로 비실행 추출했습니다. x86 `hardlock.sys` 3.92는 605,624 bytes, SHA-256 `DC80FB2F7E27C8C9EB9E30DF9E2553EB00288644E658095038297AC234566D11`, PE machine `0x014c`입니다. x64 3.93은 1,970,104 bytes, SHA-256 `1D3E3DC116EB68C6A22EF06D92C06CE9F650CB8FA772C623545D2B974F87520A`, PE machine `0x8664`입니다. 두 driver의 `Gemalto, Inc.` Authenticode는 모두 유효했습니다.
- x64 driver의 직접 dispatch에서 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458`를 각각 확인했습니다. `0x450`은 정확히 6-byte input/output, `0x44c`는 정확히 256-byte input/output을 요구합니다. `0x458`은 256 bytes 이상인 동일 크기 input/output을 요구하고 descriptor block count와 8-byte payload 단위를 처리합니다.
- `0x458`은 packet validation 이후 descriptor와 block payload를 내부 device transport로 넘깁니다. host driver가 profile별 세 seed를 입력받거나 그 세 seed로 8-byte 응답을 계산하는 경로는 확인되지 않았습니다. 공식 driver는 framing과 transport boundary의 독립 근거이지만 Function `0x0e` transform 사양이나 test vector는 제공하지 않습니다.
- `haspnt64` repository에는 명시적인 root license가 보이지 않고 README가 별도 UCLHASP 유래 material을 언급하므로 source를 복사·번역·link하지 않았습니다. 공개 source는 driver 위임 구조를 교차 확인하는 데만 사용했습니다.
- 다운로드 asset, 추출 driver와 raw disassembly는 분석 중 저장소 밖 시스템 임시 디렉터리에만 유지했고 작업 완료 후 해당 임시 디렉터리를 삭제했습니다. Installer, driver, service와 batch file은 실행·설치·load하지 않았습니다. 실제 module address, seed와 transform block은 문서·로그·명령행에 기록하지 않았습니다.

*Only the Thales 8.31 `haspdinst.exe` referenced by the `haspnt64` 2022-10-21 release was downloaded to a system temporary directory. It was never executed; its Authenticode signature is valid, its signer is `Thales DIS CPL USA, Inc.`, and its SHA-256 is `B3CEEF35024FE9E5FC6D8B2F658CB80F56ABCE35CF435AAAB97C35F498D437CA`. Two embedded CAB byte ranges were separated and extracted without running the installer. The validly signed Gemalto x86 3.92 driver is 605,624 bytes, SHA-256 `DC80FB2F7E27C8C9EB9E30DF9E2553EB00288644E658095038297AC234566D11`, machine `0x014c`; x64 3.93 is 1,970,104 bytes, SHA-256 `1D3E3DC116EB68C6A22EF06D92C06CE9F650CB8FA772C623545D2B974F87520A`, machine `0x8664`. Direct x64 dispatch contains all four observed IOCTLs. It requires exact 6-byte input/output for `0x450`, exact 256-byte input/output for `0x44c`, and equal input/output sizes of at least 256 bytes for `0x458`, whose path processes block count and eight-byte payload units. After validation, `0x458` transfers the descriptor and blocks through an internal device-transport boundary; no host path accepts three per-profile seeds or computes the eight-byte response from them. The driver independently confirms framing and transport, not the Function `0x0e` transform or a test vector. Because the repository exposes no clear root license and mentions separate UCLHASP-derived material, no `haspnt64` source was copied, translated, or linked. Downloaded assets, drivers, and raw disassembly remained outside the repository and the task-specific temporary directory was deleted after analysis; no installer, driver, service, or batch file was run, installed, or loaded; and no real module address, seed, or transform block was written to documents, logs, or command lines.*

## 검증

- Authenticode, file version, PE machine, file size와 SHA-256을 추출 전후로 확인했습니다.
- x64 `.text`의 네 IOCTL 상수 reference와 각 dispatch target을 정적 역어셈블로 교차 확인했습니다.
- `0x450`, `0x44c`, `0x458`의 size check와 `0x458` block-count access를 control flow에서 확인했습니다.
- `git status`에 다운로드·추출 asset이 없고 `cfg/hardlock.ini`가 ignore됨을 확인했습니다.
- 문서 diff에서 실제 key material과 raw packet/disassembly가 없음을 확인했습니다.

*Verification covered Authenticode, file version, PE machine, size, and SHA-256; all four x64 `.text` IOCTL references and dispatch targets; size checks for `0x450`, `0x44c`, and `0x458`; block-count access on the `0x458` path; absence of downloaded or extracted assets from Git status; continued ignore coverage for `cfg/hardlock.ini`; and absence of real key material, raw packets, or raw disassembly from the documentation diff.*
