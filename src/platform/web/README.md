# src/platform/web

Web(WebAssembly) 전용 backend가 들어갈 자리입니다. 아직 구현은 비어 있습니다.

*Web (WebAssembly) backend. Its implementation is still empty.*

Emscripten으로 빌드하며 여기서만 `<emscripten.h>`를 포함할 수 있습니다. 브라우저는 동기 파일 I/O와 블로킹 루프를 허용하지 않으므로, 이 backend의 제약은 별도 설계 문서로 정리한 뒤 구현합니다.

*Built with Emscripten; this is the only place that may include `<emscripten.h>`. Browsers allow neither synchronous file I/O nor a blocking main loop, so this backend's constraints get their own design note before implementation.*

[Web x86 실행 엔진 조사](../../../docs/kb/web-x86-execution-engines.md)의 v86 CPU 분리성 spike는 부적합으로 끝났다. CPU-only build 경계, import gate stop/resume hook, 기본 synthetic gate 주소의 실행 가능성이 모두 현재 구조에 없다. v86 전체 PC emulator나 fork를 이 backend에 도입하지 않는다. TinyEMU 계열의 공개 Web x86 소스 경계 확인 또는 후순위 직접 인터프리터 판단 전까지 이 backend는 비어 있는 상태로 둔다.

*The v86 CPU-separability spike in the [Web x86 engine survey](../../../docs/kb/web-x86-execution-engines.md) ended as unsuitable. The current structure lacks a CPU-only build boundary, an import-gate stop/resume hook, and an executable default synthetic-gate address. This backend will not introduce the complete v86 PC emulator or a v86 fork. It remains empty pending confirmation of a published TinyEMU-family Web-x86 source boundary or a later deferred-custom-interpreter decision.*
