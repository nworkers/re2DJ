# 가이드 색인 / Guides Index

이 디렉터리는 **사용자가 직접 수행하는** 반복 가능한 검증·측정·운영 절차를 둔다. 특정 작업의 일회성 증거는 [`docs/work-logs/`](../work-logs/)에 둔다.

*This directory holds repeatable verification, measurement, and operational procedures that **the user runs themselves**. One-off evidence from a specific task belongs in [`docs/work-logs/`](../work-logs/).*

각 가이드는 근거가 되는 설계 문서와 작업 로그를 링크한다.

*Each guide links the design document and work log it rests on.*

## 문서 / Documents

| 문서 | 내용 |
| --- | --- |
| [hdd-directory-setup.md](hdd-directory-setup.md) | 원본 HDD 덤프를 디렉터리로 준비하고 확인하는 절차 |
| [ez2dj4th-hardlock-config.md](ez2dj4th-hardlock-config.md) | 3rd·4th Hardlock 재료를 저장소 밖 `cfg/` 설정으로 전달하는 절차 |
| [hardlock-descriptor-extraction.md](hardlock-descriptor-extraction.md) | 프로파일 제작에 필요한 Hardlock descriptor ID와 module address 추출 절차 |
| [hardlock-seed-recovery-walkthrough.md](hardlock-seed-recovery-walkthrough.md) | 별도 생성기로 Hardlock seed 후보를 복구해 re2DJ에서 판별하는 절차 |
| [linux-sdl3-build.md](linux-sdl3-build.md) | Ubuntu/WSL의 SDL3 X11·Wayland·OpenGL 개발 패키지와 빌드 절차 |
| [post-process-shaders.md](post-process-shaders.md) | 화면 후처리 셰이더 고르기(`--post-shader`·`RE2DJ_POST_SHADER`·OSD), 직접 쓰기, 확인 절차 |
| [launcher-update.md](launcher-update.md) | 런처 자동 업데이트, 스팀덱 설치, 끄기, 문제 해결과 업데이트 경로 시험(#17) |
| [windows-x86-runtime.md](windows-x86-runtime.md) | Windows x86 제품의 in-process 실행, 시작 시 재실행, 진단 옵션, OSD |
| [decrypted-image-dump.md](decrypted-image-dump.md) | 보호 빌드가 실행 중 복호화한 주 이미지를 떠서 정적 분석에 쓰는 절차 |
