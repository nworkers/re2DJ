# re2DJ Windows support directory

이 디렉터리는 **`re2dj.exe`와 같은 디렉터리 아래에 있을 때** 런타임이 `GetWindowsDirectoryA`를 통해 게스트에 제공하는 Windows support directory입니다. 현재 작업 디렉터리와 무관합니다. re2DJ가 제공하는 지원 DLL과 리소스만 둡니다. 원본 HDD 자산이나 guest가 생성한 파일은 넣지 않습니다.

## English

When this directory is beside **`re2dj.exe`**, it is the Windows support directory exposed to the guest through runtime `GetWindowsDirectoryA`. It is independent of the current working directory. Store only re2DJ-provided support DLLs and resources here. Do not place original HDD assets or guest-created files here.
