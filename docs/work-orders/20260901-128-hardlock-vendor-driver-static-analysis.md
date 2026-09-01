# Hardlock vendor driver 정적 분석 작업 지시

관련 설계: [Hardlock vendor driver 정적 분석](../design/20260901-128-hardlock-vendor-driver-static-analysis.md)

*Related design: [Hardlock vendor driver static analysis](../design/20260901-128-hardlock-vendor-driver-static-analysis.md).*

## 범위

1. `haspnt64` repository와 release의 라이선스·출처·자산 metadata를 확인합니다.
2. 최소 release asset을 시스템 임시 경로에 다운로드합니다.
3. 어떤 installer, driver, service도 실행하지 않고 archive와 PE를 정적으로 검사합니다.
4. 공식 `HARDLOCK.SYS` 후보의 signature, version, hash와 architecture를 확인합니다.
5. 4th에서 확인된 네 IOCTL과 Function `0x0e` dispatch를 추적합니다.
6. 분석 결과를 `docs/analysis/`와 작업 로그에 확인됨/추정/미확정으로 분리합니다.

*Verify repository/release licensing, provenance, and asset metadata; download the minimum asset to a system temporary directory; inspect archives and PE files without executing installers, drivers, or services; identify and fingerprint an official `HARDLOCK.SYS` candidate; trace the four 4th IOCTLs and Function `0x0e`; and record confirmed, inferred, and unresolved results in analysis and work-log documents.*

## 금지 사항

- release binary 또는 archive를 저장소로 복사하지 않습니다.
- driver를 설치·로드하거나 test-signing mode를 변경하지 않습니다.
- 라이선스 불명 source를 제품 코드로 복사·번역하지 않습니다.
- 실제 key material이나 raw transform block을 문서·로그·명령행에 남기지 않습니다.

*Do not copy release binaries or archives into the repository, install or load a driver, change test-signing mode, copy or translate unlicensed source into product code, or place real key material or raw transform blocks in documentation, logs, or command lines.*
