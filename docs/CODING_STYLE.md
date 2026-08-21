# 코딩 스타일

## 기본 원칙

* C++ 코드는 C++20을 기준으로 작성한다.
* 기본 기준은 Google C++ Style Guide를 따른다.
* 프로젝트 예외 규칙은 아래 항목으로 명시한다.
* 원본 실행 파일의 동작 보존을 우선하며, 편의를 위한 게임 로직 재작성은 피한다.
* 플랫폼 공용 코어와 플랫폼 종속 구현을 분리한다.
* 큰 구조 변경은 설계 문서에 근거를 남긴다.
* 새 기능에는 최소 검증 절차를 함께 문서화한다.
* 독립된 책임을 갖는 큰 기능은 전용 header/source pair로 분리한다.
* 공용 data/state와 플랫폼 backend를 섞지 않으며, 실행 진입점에는 ABI 변환과 orchestration만 둔다.

## Basic Principles

* Write C++ code against C++20.
* Follow the Google C++ Style Guide as the baseline.
* Project-specific exceptions are defined below.
* Prioritize preserving original executable behavior and avoid rewriting game logic for convenience.
* Separate the platform-neutral core from platform-specific implementations.
* Document the rationale for large structural changes in design documents.
* Document a minimum verification procedure with each new feature.
* Give major independently responsible features dedicated header/source pairs.
* Keep shared data/state separate from platform backends, leaving only ABI adaptation and orchestration in execution entry points.

---

## 프로젝트 예외 규칙

* 여는 중괄호 `{` 는 같은 줄 끝에 두지 않고 다음 줄에 둔다.
* 탭 문자는 사용하지 않는다.
* 들여쓰기는 공백 4칸을 사용한다.
* 함수명은 Google 스타일에 맞춰 `PascalCase`를 사용한다.
* 변수명은 `snake_case`를 사용한다.
* 클래스 멤버 변수는 `snake_case_`를 사용한다.
* 상수와 enum 값은 `kPascalCase`를 사용한다.
* namespace는 `re2dj` 아래에 하위 namespace를 둔다. 예: `re2dj::exe`, `re2dj::hdd`.
* C++ 소스 파일 확장자는 `.cpp`를 사용한다.
* C++ 헤더 파일 확장자는 `.h`를 사용한다.
* 헤더 가드는 `RE2DJ_<DIR>_<FILE>_H_` 형식을 사용한다. `#pragma once`는 쓰지 않는다.

## Project-Specific Exceptions

* Place opening braces `{` on the next line instead of at the end of the current line.
* Do not use tab characters.
* Use four spaces for indentation.
* Use `PascalCase` for function names, following Google style.
* Use `snake_case` for variable names.
* Use `snake_case_` for class member variables.
* Use `kPascalCase` for constants and enum values.
* Nest namespaces under `re2dj`, for example `re2dj::exe` and `re2dj::hdd`.
* Use `.cpp` for C++ source files.
* Use `.h` for C++ header files.
* Use `RE2DJ_<DIR>_<FILE>_H_` header guards. Do not use `#pragma once`.

---

## 이식성 규칙

* 공용 코어(`src/` 중 `src/platform/` 이외)는 호스트 OS 헤더를 직접 포함하지 않는다. `<windows.h>`, `<unistd.h>`, `<emscripten.h>`는 플랫폼 계층 전용이다.
* 파일 시스템 접근은 `std::filesystem`을 사용한다.
* 정수 폭에 의존하는 게스트 구조체는 `std::uint32_t` 같은 고정 폭 타입만 사용한다.
* 게스트 주소는 호스트 포인터와 혼동되지 않도록 `re2dj::runtime::GuestAddress`(32비트 값 타입)로 표현한다. 호스트 포인터로 캐스팅하지 않는다.
* 바이트 순서는 리틀 엔디안 게스트를 가정하되, 다중 바이트 읽기는 명시적 조립 함수를 거친다.
* `long`은 플랫폼마다 폭이 다르므로 게스트 데이터 해석에 사용하지 않는다.

## Portability Rules

* The shared core — everything under `src/` except `src/platform/` — must not include host OS headers directly. `<windows.h>`, `<unistd.h>`, and `<emscripten.h>` belong to the platform layer only.
* Use `std::filesystem` for file-system access.
* Use fixed-width types such as `std::uint32_t` for any guest structure whose width matters.
* Represent guest addresses as `re2dj::runtime::GuestAddress`, a 32-bit value type, so they are never confused with host pointers. Do not cast them to host pointers.
* Assume a little-endian guest, but read multi-byte values through explicit assembly helpers rather than reinterpreting memory.
* Do not use `long` to interpret guest data; its width varies across platforms.

---

## 주석 언어

* **소스 코드의 주석은 영어로만 작성한다.** 한국어 주석은 남기지 않는다.
* 한국어와 영어를 함께 적은 이중 언어 주석도 두지 않는다. 같은 내용을 두 벌 유지하면 한쪽만 갱신되어 서로 어긋난다.
* 이 규칙은 **문서 규칙과 다르다.** `docs/` 아래 Markdown 문서는 한국어를 먼저 쓰고 영어 번역을 덧붙이지만(`AGENTS.md`), 소스 주석은 영어 한 벌만 둔다.
* 커밋 메시지, 코드 식별자, 로그 문자열도 영어를 사용한다.
* 적용 대상은 C++ 소스·헤더와 빌드·측정 스크립트(`scripts/`, `CMakeLists.txt`, 워크플로 파일)를 포함한 저장소의 모든 코드다. `third_party/` 아래 외부 코드는 원본 그대로 둔다.
* 기존 파일을 수정할 때 한국어 주석을 발견하면 같은 작업에서 영어로 바꾼다.

## Comment Language

* **Write source-code comments in English only.** Do not leave Korean comments.
* Do not keep bilingual comments either: maintaining the same explanation twice lets one copy be updated while the other drifts out of date.
* This rule **differs from the documentation rule**. Markdown under `docs/` leads with Korean and adds an English translation (`AGENTS.md`), while source comments carry one English copy only.
* Commit messages, code identifiers, and log strings are English as well.
* The scope is all code in the repository, including C++ sources and headers and the build and measurement scripts (`scripts/`, `CMakeLists.txt`, workflow files). External code under `third_party/` stays as upstream wrote it.
* When editing an existing file that still has Korean comments, convert them in the same task.

---

## 적용 범위

* `include/` 아래의 C++ 헤더
* `src/` 아래의 C++ 소스
* `tests/` 아래의 테스트 코드
* 이후 추가되는 모든 C++ 파일
* 주석 언어 규칙은 `scripts/`와 빌드 파일을 포함한 저장소의 모든 코드에 적용된다

## Scope

* C++ headers under `include/`
* C++ source files under `src/`
* Test code under `tests/`
* All future C++ files
* The comment-language rule applies to all repository code, including `scripts/` and build files

---

## 예시

잘못된 예:

```cpp
int main() {
    if (ready) {
        Run();
    }
}
```

올바른 예:

```cpp
int main()
{
    if (ready)
    {
        Run();
    }
}
```

## Example

Incorrect:

```cpp
int main() {
    if (ready) {
        Run();
    }
}
```

Correct:

```cpp
int main()
{
    if (ready)
    {
        Run();
    }
}
```

---

## 디렉터리 정책

* 플랫폼 공용 로더와 런타임 코어는 `src/` 아래의 공용 영역에 둔다.
* 64비트 Windows 전용 코드는 `src/platform/windows/` 아래에 둔다.
* Linux 전용 코드는 `src/platform/linux/` 아래에 둔다.
* Web 전용 코드는 `src/platform/web/` 아래에 둔다.
* 실행 진입점은 `src/host/` 아래에 둔다.
* 비실행 분석 도구는 `src/tools/<도구 이름>/` 아래에 둔다.
* 공개 헤더는 `include/re2dj/<subsystem>/`에 두고, 한 하위 시스템 안에서만 쓰는 헤더는 해당 `src/` 디렉터리에 둔다.

> rePIU는 32비트 Win32 호스트를 전제로 `src/platform/win32/`를 사용했다. re2DJ의 Windows 호스트는 64비트이므로 비트 폭을 이름에 넣지 않고 `src/platform/windows/`를 사용한다.

## Directory Policy

* Put platform-neutral loader and runtime core code in shared areas under `src/`.
* Put 64-bit Windows-specific code under `src/platform/windows/`.
* Put Linux-specific code under `src/platform/linux/`.
* Put Web-specific code under `src/platform/web/`.
* Put execution entry points under `src/host/`.
* Put non-executing analysis tools under `src/tools/<tool-name>/`.
* Put public headers in `include/re2dj/<subsystem>/`, and keep headers used only inside one subsystem next to its sources under `src/`.

> rePIU used `src/platform/win32/` because its host was a 32-bit Win32 process. The re2DJ Windows host is 64-bit, so the directory name omits the bit width and reads `src/platform/windows/`.

---

## 라이선스 정책

* 프로젝트 기본 라이선스는 `BSD 3-Clause License`를 기준으로 한다.
* GPL, LGPL, AGPL 등 전염성 라이선스의 서드파티 코드는 도입하지 않는다. Wine은 LGPL이므로 코드 재사용 대상이 아니다.
* 서드파티 의존성을 추가하기 전에 라이선스를 확인하고 `THIRD_PARTY_NOTICES.md`에 문서화한다.

## License Policy

* Use the `BSD 3-Clause License` as the project license baseline.
* Do not introduce third-party code under copyleft licenses such as GPL, LGPL, or AGPL. Wine is LGPL and is therefore not a code-reuse source.
* Check third-party dependency licenses before adding them and record them in `THIRD_PARTY_NOTICES.md`.
