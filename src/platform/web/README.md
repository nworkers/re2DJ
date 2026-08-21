# src/platform/web

Web(WebAssembly) 전용 backend가 들어갈 자리입니다. 아직 비어 있습니다.

*Web (WebAssembly) backend. Empty for now.*

Emscripten으로 빌드하며 여기서만 `<emscripten.h>`를 포함할 수 있습니다. 브라우저는 동기 파일 I/O와 블로킹 루프를 허용하지 않으므로, 이 backend의 제약은 별도 설계 문서로 정리한 뒤 구현합니다.

*Built with Emscripten; this is the only place that may include `<emscripten.h>`. Browsers allow neither synchronous file I/O nor a blocking main loop, so this backend's constraints get their own design note before implementation.*
