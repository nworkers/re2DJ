# TODO

현재 열려 있는 항목입니다. 완료된 단계는 [포팅 계획](WIN32_HLE_PORTING_PLAN.md)에서 확인하십시오.

*Currently open items. Completed stages are tracked in the [porting plan](WIN32_HLE_PORTING_PLAN.md).*

## 알려진 결함 / Known defects

* [ ] **비ASCII 경로 출력이 깨진다.** `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환한다. 경로 해석 자체는 정상이고 출력만 깨진다. 콘솔 출력 경로를 `u8string()` 기반으로 바꾸거나 Windows에서 wide 출력을 쓴다.
* [x] ~~기본 타깃 선택이 틀렸다~~ — [내장 타깃 프로파일](design/20260822-005-built-in-target-profiles.md)로 해결. 두 덤프 모두 정확한 기본 타깃을 고른다

## 바로 필요한 것 / Immediate

* [ ] Linux 호스트에서 빌드·테스트 검증 — 대소문자 무시 해석의 실제 fallback 경로는 여기서만 동작한다
* [ ] Web(Emscripten) 빌드 검증
* [ ] GitHub Actions 워크플로 첫 실행 확인

## 다음 단계 / Next stage

* [ ] Stage 2 — 이미지 적재 설계 문서
* [ ] `re2dj::runtime::GuestAddress`와 `AddressSpace`
* [ ] PE32 섹션 매핑과 기준 재배치
* [ ] import 해석과 gate 주소 배정 — 대상 API 목록은 [import 표면 분석](analysis/ez2dj-import-surface.md)에 이미 확정되어 있다
* [ ] Stage 3 — x86-32 인터프리터. **멀티스레드 게스트**를 전제해야 한다

첫 적재 대상은 `ez2dj1stse_unpacked`(`ez2dj1.exe`)입니다. 보호되지 않은 유일한 빌드이므로 언패킹 스텁 없이 진짜 게임 코드에 도달합니다. 최종 목표는 `ez2dj1stse`(`ez2dj.exe`)입니다.

*The first load target is `ez2dj1stse_unpacked` (`ez2dj1.exe`), the only unprotected build. The final target is `ez2dj1stse` (`ez2dj.exe`).*

## 분석 / Analysis

* [ ] `Songs/` 아래 자산 파일 형식
* [ ] 3rd의 게스트 드라이브 문자와 작업 디렉터리 — 3rd 덤프에는 `System.ini`가 없다
* [ ] 실행 중 `CreateFileA` 경로 추적 — `\.\` 드라이버 경로가 있는지, `Tdsd.vxd111`이 실제로 쓰이는지. 3rd의 `"UseIOCard" = 1`이 어느 경로로 이어지는지
* [ ] DirectDraw 표면 구성: 개수, 픽셀 포맷, 해상도. 3rd `EZ2DJ.INI`는 640x480 전체 화면
* [ ] 보호된 빌드가 런타임에 `GetProcAddress`로 가져오는 API 목록
* [ ] 더 많은 버전의 덤프 — 내장 프로파일은 실제로 확인한 덤프에 대해서만 추가한다

## 결정이 필요한 것 / Decisions pending

* [ ] 테스트 프레임워크 도입 여부 — 라이선스 확인과 빌드 시간 비용을 별도 설계로 다룬다
* [ ] 그래픽·오디오 backend 라이브러리 선정 — zlib/BSD 계열만 검토한다
