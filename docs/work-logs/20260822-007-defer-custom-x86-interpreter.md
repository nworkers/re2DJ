# 작업 로그: 직접 x86 인터프리터 후순위화

## 결과

사용자 결정에 따라 직접 x86-32 인터프리터를 다음 구현 단계에서 제외하고 Web 또는 공용 fallback이 필요한 시점으로 미뤘습니다. 다음 단계는 `ExecutionBackend` 경계를 설계하고 Windows x64와 Linux x86-64의 별도 32비트 네이티브 helper를 검증하는 작업입니다. 동시에 Web에서 사용할 수 있는 허용 라이선스 실행 엔진을 조사하며, 적합한 엔진이 없을 때만 직접 인터프리터를 구현합니다.

## Result

Following the user's decision, a custom x86-32 interpreter is no longer the next implementation stage and is deferred until a Web or shared fallback requires it. The next stage defines the `ExecutionBackend` boundary and validates a separate native 32-bit helper on Windows x64 and Linux x86-64. Web-capable execution engines with permitted licenses are evaluated in parallel, and a custom interpreter is implemented only if no suitable engine exists.

## 함께 교정한 내용

기존 문서는 64비트 Windows와 Linux에서 원본 x86 코드를 CPU로 직접 실행할 방법이 전혀 없다고 서술했습니다. 이는 64비트 **프로세스 내부**에는 맞지만 x86-64 운영체제의 별도 32비트 프로세스 가능성을 누락한 표현입니다. 프로젝트 헌장, 아키텍처, 지식 문서를 다음처럼 교정했습니다.

* 데스크톱 x86-64: 별도 32비트 helper의 네이티브 실행 가능
* 64비트 host process 내부: 32비트 코드를 직접 적재할 수 없음
* WebAssembly: 인터프리터, DBT 또는 외부 엔진 중 하나가 반드시 필요
* Wine/Proton: 비교 기준으로만 사용하고 소스 또는 필수 런타임으로 통합하지 않음

## Corrected context

Earlier documents said there was no host-CPU path at all on 64-bit Windows or Linux. That is true inside a 64-bit process but omitted the separate 32-bit process available on x86-64 operating systems. The charter, architecture, and knowledge base now distinguish desktop native helpers, the 64-bit process boundary, and WebAssembly's unavoidable need for an x86 execution layer. Wine and Proton remain behavioral references only.

## 검증

관련 활성 문서에서 인터프리터를 즉시 기준 backend로 표현하는 문구가 남지 않았는지 검색했고 `git diff --check`가 통과했습니다. 코드 변경은 없어 빌드·단위 테스트는 다시 실행하지 않았습니다.

## Verification

Searched active documentation for stale statements that described the interpreter as the immediate baseline backend, and `git diff --check` passed. No code changed, so the build and unit tests were not rerun.
