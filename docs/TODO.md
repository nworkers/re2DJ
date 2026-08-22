# TODO

현재 열려 있는 항목입니다. 완료된 단계는 [포팅 계획](WIN32_HLE_PORTING_PLAN.md)에서 확인하십시오.

*Currently open items. Completed stages are tracked in the [porting plan](WIN32_HLE_PORTING_PLAN.md).*

## 알려진 결함 / Known defects

* [ ] **비ASCII 경로 출력이 깨진다.** `std::filesystem::path::string()`이 Windows에서 활성 ANSI 코드 페이지로 변환한다. 경로 해석 자체는 정상이고 출력만 깨진다. 콘솔 출력 경로를 `u8string()` 기반으로 바꾸거나 Windows에서 wide 출력을 쓴다.
* [x] ~~기본 타깃 선택이 틀렸다~~ — [내장 타깃 프로파일](design/20260822-005-built-in-target-profiles.md)로 해결. 두 덤프 모두 정확한 기본 타깃을 고른다

## 바로 필요한 것 / Immediate

* [x] Linux 호스트에서 빌드·테스트 검증 — WSL Ubuntu 24.04 x86-64 warnings-as-errors build 및 unit CTest 통과
* [ ] Web(Emscripten) 빌드 검증
* [ ] GitHub Actions 워크플로 첫 실행 확인

## 다음 단계 / Next stage

* [x] Stage 3 — `ExecutionBackend` event/reply 경계 정의
* [x] Windows x64의 별도 Win32 x86 프로세스에서 네이티브 x86→HLE gate 호출 probe
* [x] synthetic PE32 mapping과 32/64비트 IPC를 포함한 Windows native helper prototype
* [x] Windows native helper protocol을 `ExecutionBackend` adapter로 캡슐화
* [x] Windows helper의 import별 범용 native thunk와 gate metadata 전달
* [x] Windows native helper의 base relocation과 TLS callback 실행
* [x] Linux x86-64의 별도 32비트 프로세스에서 import gate HLE를 연결하는 최소 prototype
* [x] Linux native helper의 PE32 mapping과 `ExecutionBackend` adapter — relocation, TLS, named/ordinal import probe까지 검증
* [x] Web에서 사용할 수 있는 BSD/MIT/zlib 계열 x86 실행 엔진 조사와 라이선스 확인 — v86 조사·분리성 spike까지 완료했으나 구조적으로 부적합하여 채택 중단
* [x] v86 CPU·메모리·실행 루프를 PC 장치 모델 없이 분리할 수 있는지 검증하는 Web `ExecutionBackend` spike — CPU-only build 경계·gate stop/resume hook이 없어 부적합; 채택 중단
* [ ] 직접 x86-32 인터프리터 — 적합한 공용 실행 엔진이 없을 때 Web/fallback 단계에서 착수. **멀티스레드 게스트**를 전제한다

첫 적재 대상은 `ez2dj1stse_unpacked`(`ez2dj1.exe`)입니다. 보호되지 않은 유일한 빌드이므로 언패킹 스텁 없이 진짜 게임 코드에 도달합니다. 최종 목표는 `ez2dj1stse`(`ez2dj.exe`)입니다.

다음 구현은 Windows를 기준으로 진행합니다. Stage 4~7은 64-bit Windows와 Win32 x86 helper에서 원본 `ez2dj1.exe`의 실제 import 호출을 검증한 뒤 Linux/Web으로 확장합니다.

*The next implementation work is Windows-first. Stage 4 through Stage 7 validate real import calls from `ez2dj1.exe` on 64-bit Windows with the Win32 x86 helper before expanding to Linux or Web.*

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
