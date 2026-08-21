# TODO

현재 열려 있는 항목입니다. 완료된 단계는 [포팅 계획](WIN32_HLE_PORTING_PLAN.md)에서 확인하십시오.

*Currently open items. Completed stages are tracked in the [porting plan](WIN32_HLE_PORTING_PLAN.md).*

## 알려진 결함 / Known defects

* [ ] **기본 타깃 선택이 틀렸다.** 1st SE 덤프에서 `re2dj --hdd`가 `Test.exe`를 고른다. 후보 순위가 크기 내림차순이고 서비스 도구가 게임보다 크기 때문이다. 경로가 확인되었으므로 내장 타깃 프로파일 추가로 해결한다. 근거: [HDD 레이아웃 분석 5절](analysis/ez2dj-hdd-layout.md)
* [ ] **비ASCII 경로 출력이 깨진다.** `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환한다. 경로 해석 자체는 정상이고 출력만 깨진다. 콘솔 출력 경로를 `u8string()` 기반으로 바꾸거나 Windows에서 wide 출력을 쓴다.

## 바로 필요한 것 / Immediate

* [ ] 내장 타깃 프로파일 추가 — 1st SE(`ez2dj1.exe` 우선)와 3rd(`EZ2DJ.EXE`). 설계 문서 먼저
* [ ] Linux 호스트에서 빌드·테스트 검증 — 대소문자 무시 해석의 실제 fallback 경로는 여기서만 동작한다
* [ ] Web(Emscripten) 빌드 검증
* [ ] GitHub Actions 워크플로 첫 실행 확인

## 다음 단계 / Next stage

* [ ] Stage 2 — 이미지 적재 설계 문서
* [ ] `re2dj::runtime::GuestAddress`와 `AddressSpace`
* [ ] PE32 섹션 매핑과 기준 재배치
* [ ] import 해석과 gate 주소 배정 — 대상 API 목록은 [import 표면 분석](analysis/ez2dj-import-surface.md)에 이미 확정되어 있다
* [ ] Stage 3 — x86-32 인터프리터. **멀티스레드 게스트**를 전제해야 한다

## 분석 / Analysis

* [ ] `Songs/` 아래 자산 파일 형식
* [ ] 실행 중 `CreateFileA` 경로 추적 — `\.\` 드라이버 경로가 있는지, `Tdsd.vxd111`이 실제로 쓰이는지
* [ ] DirectDraw 표면 구성: 개수, 픽셀 포맷, 해상도
* [ ] 보호된 빌드가 런타임에 `GetProcAddress`로 가져오는 API 목록

## 결정이 필요한 것 / Decisions pending

* [ ] 첫 목표 버전 — 정적 분석은 1st SE의 `ez2dj1.exe`를 가리킨다. 보호되지 않은 유일한 빌드다
* [ ] 테스트 프레임워크 도입 여부 — 라이선스 확인과 빌드 시간 비용을 별도 설계로 다룬다
* [ ] 그래픽·오디오 backend 라이브러리 선정 — zlib/BSD 계열만 검토한다
