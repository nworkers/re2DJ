# 작업 지시 334: Linux x86 동적 resolver 관측 / Work order 334: Linux x86 dynamic resolver observation

Linux x86 in-process diagnostic에 kernel32 pseudo module identity와 첫 `GetProcAddress("GetVersion")` request 관측을 추가합니다. 실제 4th CHD와 synthetic regression으로 검증하며 resolver 결과 API 실행은 추가하지 않습니다.

*Add kernel32 pseudo-module identity and first `GetProcAddress("GetVersion")` request observation to Linux x86 in-process diagnostic. Verify with real 4th CHD and synthetic regression; do not add resolver-result API execution.*
