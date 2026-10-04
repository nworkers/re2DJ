# src/platform/sdl

in-process 러너가 쓰는 SDL3 host입니다. 게스트의 화면(`SdlHostPresentation`, SDL3/OpenGL 창과 OSD), 키보드·마우스·게임패드 입력(`host_keyboard`, `input/sdl3_gamepad_reader`), 소리(`SdlHostAudio`, SDL3_mixer)를 맡습니다. SDL만 쓰므로 Linux와 Windows가 같은 코드를 씁니다(작업 447, [설계](../../../docs/design/20261004-446-windows-in-process-loader.md)). namespace는 `re2dj::platform::sdl`, 공개 헤더는 `include/re2dj/platform/sdl/`입니다. 이 디렉터리는 OS 헤더를 포함하지 않습니다.

*The SDL3 hosts the in-process runner uses: the guest's display (`SdlHostPresentation`, an SDL3/OpenGL window with the OSD), keyboard, mouse and gamepad input (`host_keyboard`, `input/sdl3_gamepad_reader`), and sound (`SdlHostAudio`, SDL3_mixer). They use SDL alone, so Linux and Windows share them (task 447, [design](../../../docs/design/20261004-446-windows-in-process-loader.md)). The namespace is `re2dj::platform::sdl` and the public headers are under `include/re2dj/platform/sdl/`; nothing here includes an OS header.*
