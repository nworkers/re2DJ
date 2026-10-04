# 화면 후처리 셰이더 / Post-processing shaders

설계: [20261005-455](../design/20261005-455-post-process-shaders.md) · 작업 로그: [20261005-455](../work-logs/20261005-455-post-process-shaders.md) · 형식: [kb](../kb/libretro-glsl-post-shaders.md)

*Design: [20261005-455](../design/20261005-455-post-process-shaders.md) · work log: [20261005-455](../work-logs/20261005-455-post-process-shaders.md) · format: [kb](../kb/libretro-glsl-post-shaders.md)*

## 1. 고르는 방법 / Choosing a shader

| 방법 | 지속 | 예 |
| --- | --- | --- |
| 명령행 | 그 실행(환경 변수보다 우선) | `re2dj ez2dj4th --post-shader crt`, `re2dj --post-shader=scanline ez2dj4th` |
| 환경 변수 | 그 실행 | `RE2DJ_POST_SHADER=scanline` |
| 실행 중 백틱(`` ` ``) OSD | 그 실행만 | Screen shader 콤보, Reload, 매개변수 슬라이더 |

명령행 옵션은 `--post-shader <id>`와 `--post-shader=<id>` 두 형식이며 타깃 앞뒤 어디에 와도 됩니다. 여러 번 주면 마지막 값을 쓰고, 값이 없거나(`--post-shader`가 마지막 인자) 비어 있으면(`--post-shader=`) 사용법 오류로 exit 1입니다. `--` 뒤의 인자는 옵션으로 읽지 않습니다(작업 457, rePIU 작업 771과 같은 규칙). id는 `none`(기본), 내장 `crt`·`scanline`, 또는 `shaders/` 안의 파일 이름(`my_crt.glsl`)입니다. `shaders/`는 작업 디렉터리를 먼저 보고, 없으면 실행 파일 옆을 봅니다. 6th처럼 런처가 자식을 띄우는 타깃은 자식도 같은 셰이더로 열립니다.

*Command line (that run, over the environment variable): `re2dj ez2dj4th --post-shader crt` or `re2dj --post-shader=scanline ez2dj4th`, before or after the target, the last of a repeated option winning, a missing (`--post-shader` last) or empty (`--post-shader=`) value a usage error with exit 1, and nothing after `--` read as an option (task 457, rePIU task 771's rules); environment variable (that run): `RE2DJ_POST_SHADER=scanline`; the backtick OSD while running (that run only): the Screen shader combo, Reload, parameter sliders. Ids are `none` (the default), the built-in `crt` and `scanline`, or a file name in `shaders/` (`my_crt.glsl`), looked up in the working directory first and beside the executable otherwise. A target whose launcher starts a child, such as 6th, opens the child with the same shader.*

## 2. 직접 셰이더 쓰기 / Writing a shader

libretro 단일 pass GLSL 형식의 부분집합입니다. 한 파일에 두 단계를 `VERTEX`·`FRAGMENT`로 나눕니다. 내장 `src/graphics/post_shaders/scanline.glsl`이 가장 짧은 예입니다.

*A subset of libretro single-pass GLSL: one file, two stages guarded by `VERTEX` and `FRAGMENT`. The built-in `src/graphics/post_shaders/scanline.glsl` is the shortest example.*

```glsl
#pragma parameter STRENGTH "Strength" 0.5 0.0 1.0 0.05

#if defined(VERTEX)
attribute vec4 VertexCoord;
attribute vec4 TexCoord;
uniform mat4 MVPMatrix;
varying vec2 tex_coord;
void main()
{
    gl_Position = MVPMatrix * VertexCoord;
    tex_coord = TexCoord.xy;
}
#elif defined(FRAGMENT)
uniform sampler2D Texture;
uniform vec2 TextureSize;   // the game's logical resolution, e.g. 640x480
uniform vec2 OutputSize;    // the picture's pixel size in the window
varying vec2 tex_coord;
#ifdef PARAMETER_UNIFORM
uniform float STRENGTH;
#else
#define STRENGTH 0.5
#endif
void main()
{
    gl_FragColor = texture2D(Texture, tex_coord) * (1.0 - STRENGTH * 0.5);
}
#endif
```

쓸 수 있는 이름: attribute `VertexCoord`, `TexCoord`, `COLOR`; uniform `MVPMatrix`, `Texture`, `InputSize`, `TextureSize`, `OutputSize`, `FrameCount`, `FrameDirection`; `#pragma parameter`. GLSL 1.20 문법을 쓰십시오. 다단계 preset(`.glslp`), `PrevTexture`, LUT는 지원하지 않습니다.

작성 중에는 게임을 켠 채 파일을 고치고 OSD의 **Reload**를 누르면 됩니다. 컴파일 오류는 OSD에 빨간 글씨로 나오며, 그동안 화면은 `none`으로 그려집니다. 처음 연 셰이더가 적용되지 않으면 로그에 `presentation: post shader '…' not applied: …`가 남습니다.

*Available names: attributes `VertexCoord`, `TexCoord`, `COLOR`; uniforms `MVPMatrix`, `Texture`, `InputSize`, `TextureSize`, `OutputSize`, `FrameCount`, `FrameDirection`; `#pragma parameter`. Use GLSL 1.20; multi-pass presets (`.glslp`), `PrevTexture` and LUTs are not supported. While writing, keep the game running, edit the file and press **Reload** in the OSD; a compile error shows in red in the OSD and the picture is drawn as `none` meanwhile. When the shader a window opens with is not applied, the log reads `presentation: post shader '…' not applied: …`.*

## 3. 확인 절차 / Check procedure

1. `re2dj ez2dj4th --post-shader crt`로 실행합니다. 로그에 `post shader     : crt`와 `presentation: post shader crt (6 parameters)`가 나옵니다.
2. 화면이 휘고 주사선과 마스크가 보이는지 봅니다. 주사선은 기본 2배 창 이상에서 뚜렷합니다.
3. 백틱 → Screen shader에서 `scanline`, `none`으로 바꿔 즉시 바뀌는지 봅니다. `none`이면 후처리 없는 원래 화면입니다.
4. `shaders/test.glsl`에 위 예제를 넣고 Reload → 목록에 `test.glsl`이 나오고 고를 수 있는지 봅니다. 파일에 일부러 오타를 넣고 Reload하면 오류가 보이고 화면은 `none`이어야 합니다.
5. `RE2DJ_POST_SHADER=scanline`을 두고 `--post-shader` 없이 실행하면 scanline으로, `--post-shader none`을 더하면 `none`으로 열리는지 봅니다.
6. 자동 확인: `re2dj_opengl_post_shader_probe`(데스크톱에서 직접 실행)가 12개 항목을 모두 `PASS`로 끝내야 합니다.

*Run `re2dj ez2dj4th --post-shader crt` (the log shows `post shader     : crt` and `presentation: post shader crt (6 parameters)`); check the curvature, scanlines and mask, clear at the default 2x window or more; switch to `scanline` and `none` from the backtick OSD and check the change is immediate; put the example in `shaders/test.glsl`, press Reload and choose `test.glsl`, then break it on purpose and check the error and the `none` picture; with `RE2DJ_POST_SHADER=scanline` a run without `--post-shader` opens with scanline and one with `--post-shader none` with none; and `re2dj_opengl_post_shader_probe`, run by hand on a desktop, ends with all 12 checks `PASS`.*
