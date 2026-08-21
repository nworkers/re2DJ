# include/re2dj/hle

Win32 / DirectX HLE 모듈 테이블의 **공개 헤더**가 들어갈 자리입니다.

*Public headers for the Win32 and DirectX HLE module tables.*

모듈 하나가 `{이름, ordinal, 인자 개수, 호출 규약, 구현 함수}` 항목의 테이블입니다. 로더는 import 이름을 이 테이블에서 찾아 gate 주소를 배정합니다.

*One module is a table of `{name, ordinal, argument count, calling convention, implementation}` entries. The loader looks up an import name here and assigns it a gate address.*
