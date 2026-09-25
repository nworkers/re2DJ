# include/re2dj/platform

플랫폼 backend가 구현해야 할 **인터페이스 헤더**가 들어갈 자리입니다.

*Interface headers that every platform backend implements.*

창, 렌더 표면, 오디오 출력, 입력, 시간 소스의 추상 인터페이스를 여기에 두고, 구현은 `src/platform/<플랫폼>/`에 둡니다. 이 헤더에는 호스트 OS 타입이 등장하지 않습니다.

*Abstract interfaces for the window, render surface, audio output, input, and time source live here; implementations live under `src/platform/<platform>/`. No host OS type appears in these headers.*

플랫폼 구현을 공개하는 header는 `include/re2dj/platform/<os>/`에 둡니다. host 비트 폭 중립 또는 x86/x64 공용 선언은 그 루트에 두고, 32비트 전용은 `x86/`, 64비트 전용은 `x64/` 하위에 둡니다. 게스트 주소가 32비트라는 이유만으로 header를 `x86/`에 두지는 않습니다.

*Public platform-implementation headers live under `include/re2dj/platform/<os>/`. Keep host-width-neutral or x86/x64-shared declarations at that root, 32-bit-only declarations under `x86/`, and 64-bit-only declarations under `x64/`. A 32-bit guest address alone does not place a header under `x86/`.*

공개 header에는 Win32·Linux host type을 드러내지 않습니다. native helper의 `ExecutionBackend` adapter header는 작업 379에서 제거했습니다.

*Public headers expose no Win32 or Linux host types. The native helper's `ExecutionBackend` adapter headers were removed in Task 379.*
