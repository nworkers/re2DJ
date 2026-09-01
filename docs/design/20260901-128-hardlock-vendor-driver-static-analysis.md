# Hardlock vendor driver 정적 분석 설계

## 목적

`haspnt64` 프로젝트 릴리스가 안내하는 legacy Hardlock driver package에서 공식 vendor binary를 비실행 방식으로 확보하고, ez2dj4th의 `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458` 및 Function `0x0e` 동작을 독립적으로 복원할 근거가 있는지 확인합니다.

*Acquire the official vendor binary referenced by the `haspnt64` release without executing it, then determine whether it provides an independent basis for ez2dj4th IOCTLs `0x9c402468`, `0x9c402450`, `0x9c40244c`, `0x9c402458`, and Function `0x0e`.*

## 격리와 출처 정책

- 릴리스와 추출 binary는 시스템 임시 directory에만 둡니다.
- installer, service, driver와 batch file은 실행하지 않습니다.
- Authenticode signer, file version, SHA-256, PE machine과 import/export metadata를 먼저 확인합니다.
- 저장소에는 binary, archive, dump, raw disassembly와 key material을 추가하지 않습니다.
- `haspnt64` source는 라이선스와 provenance가 불명확하므로 코드 구현 근거로 복사·번역하지 않습니다.
- 공식 vendor binary에서 독립적으로 확인한 control flow와 구조만 주소·offset·동작 사양으로 기록합니다.

*Keep release and extracted binaries only in a system temporary directory; never execute installers, services, drivers, or batch files; verify Authenticode, file version, SHA-256, PE metadata, imports, and exports first; do not add binaries, archives, dumps, raw disassembly, or key material to the repository; do not copy or translate the provenance-unclear `haspnt64` source; and record only independently observed vendor-binary control flow, structures, offsets, and behavior.*

```mermaid
flowchart LR
    A[GitHub release metadata] --> B[temporary download]
    B --> C[hash and signature verification]
    C --> D[non-executing archive extraction]
    D --> E[official vendor PE identification]
    E --> F[IOCTL dispatch static analysis]
    F --> G[clean-room behavior specification]
    G --> H[future re2DJ implementation]
```

## 분석 순서

1. GitHub API에서 release asset의 이름, 크기, content type과 download URL을 확인합니다.
2. 필요한 최소 archive 또는 installer만 임시 경로에 다운로드합니다.
3. archive 형식이면 `tar` 등 비실행 extractor를 사용합니다. 실행 파일 자체를 실행해 추출하지 않습니다.
4. `HARDLOCK.SYS` 또는 관련 vendor driver를 식별하고 signature/version/hash를 기록합니다.
5. PE section, import, string과 IOCTL constant reference를 검색합니다.
6. 네 control code의 dispatch와 Function `0x0e` 경로가 없으면 이 package가 pass-through 전용임을 확정하고 다른 vendor version을 찾습니다.
7. 경로가 있으면 bit-level 동작을 clean-room 사양으로 복원하고 독립 test vector를 구성합니다.

*Inspect release metadata; download only the minimum asset; extract archives without executing embedded programs; identify and fingerprint `HARDLOCK.SYS` or related vendor drivers; search PE sections, imports, strings, and IOCTL constants; classify the package as pass-through-only if the four control paths are absent; otherwise recover a clean-room bit-level specification and independent test vector.*

## 완료 조건

- 다운로드·추출 자산이 Git status에 나타나지 않습니다.
- 실행이나 driver 설치 없이 출처와 signature 상태를 확인합니다.
- 대상 IOCTL/Function `0x0e`의 구현 위치 또는 부재를 근거와 함께 판정합니다.
- 실제 ModAd와 Seed는 계속 `cfg/hardlock.ini`에만 존재합니다.

*Completion requires all downloaded assets to remain outside Git, provenance and signature verification without execution or driver installation, an evidence-backed decision on the location or absence of the target IOCTL/Function `0x0e` implementation, and continued confinement of real ModAd/Seed values to `cfg/hardlock.ini`.*
