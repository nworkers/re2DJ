# include/re2dj/hle

Win32 / DirectX HLE module table과 import dispatcher의 **공개 헤더**가 들어 있습니다.

*Public headers for the Win32 and DirectX HLE module tables and import dispatcher.*

`import_dispatcher.h`는 `{module, name 또는 ordinal, 인자 개수, 호출 규약, handler}` binding을 등록하고 `ExecutionBackend`를 통해 x86 `ESP + 4` 인자를 읽어 completion을 보냅니다. module 이름은 ASCII 대소문자를 구분하지 않고, import 이름은 정확히 비교합니다. 실제 Win32/DirectX API binding은 원본 import와 runtime 증거를 확인한 뒤 추가합니다.

*`import_dispatcher.h` registers `{module, name or ordinal, argument count, calling convention, handler}` bindings and reads x86 `ESP + 4` arguments through `ExecutionBackend` before sending completion. Module names compare ASCII case-insensitively; import names compare exactly. Actual Win32/DirectX API bindings are added only after original-import and runtime evidence is available.*
