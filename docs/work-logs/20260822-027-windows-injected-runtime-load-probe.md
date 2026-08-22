# Windows Injected Runtime Load Probe Result

## 한국어

entry hardware breakpoint에서 primary thread를 추가 suspend하고 x64 host가 remote `LoadLibraryW` thread를 만들었습니다. DLL 경로 쓰기와 remote thread 생성은 성공했지만 `LoadLibraryW`는 module base 0을 반환했습니다.

원인은 x64 host에서 얻은 `kernel32!LoadLibraryW` 함수 포인터가 x86 child의 WOW64 `kernel32` 주소가 아니기 때문입니다. x64 함수 주소를 x86 remote thread 시작 주소로 사용할 수 없습니다.

다음 구현에는 선택이 필요합니다.

1. 기존 staged Win32 helper를 injector로 확장합니다. x86 helper가 child를 만들고 `LoadLibraryW`·IAT patch를 수행하며 x64 host와 protocol로 통신합니다.
2. x64 host가 child WOW64 module의 base와 SysWOW64 `kernel32` export RVA를 해석하여 x86 `LoadLibraryW` remote 주소를 직접 계산합니다.

1번은 이미 배포·자동 발견되는 x86 helper를 재사용하고 bitness 경계를 명확히 분리합니다. 2번은 helper protocol을 늘리지 않지만 WOW64 module/export parsing을 x64 host에 추가합니다. 이 probe는 IAT를 변경하지 않았고 primary thread를 resume하지 않은 채 child를 종료했습니다.

## English

At the entry hardware breakpoint, the primary thread received an additional suspend and the x64 host created a remote `LoadLibraryW` thread. Writing the DLL path and creating the remote thread succeeded, but `LoadLibraryW` returned module base zero.

The cause is that `kernel32!LoadLibraryW` obtained in the x64 host is not the address of WOW64 `kernel32` in the x86 child. An x64 function address cannot be used as the start address of an x86 remote thread.

The next implementation requires a choice:

1. Extend the existing staged Win32 helper as the injector. The x86 helper creates the child and performs `LoadLibraryW`/IAT patching, while communicating with the x64 host through the protocol.
2. Have the x64 host parse the child WOW64 module base and SysWOW64 `kernel32` export RVA to calculate the x86 `LoadLibraryW` remote address directly.

Option 1 reuses the already staged and automatically discovered x86 helper and clearly separates the bitness boundary. Option 2 avoids extending the helper protocol but adds WOW64 module/export parsing to the x64 host. This probe did not change the IAT and terminated the child without resuming its primary thread.
