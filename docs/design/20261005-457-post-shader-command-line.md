# 작업 457 설계 — 셰이더 명령행 옵션을 rePIU와 맞추기 / Task 457 design — matching rePIU's shader command-line option

선행: [작업 455](20261005-455-post-process-shaders.md) · 참조: rePIU 작업 771(rePIU v0.0.201, 설계 `docs/design/20261005-771-post-shader-command-line.md`)

*Builds on [task 455](20261005-455-post-process-shaders.md); reference: rePIU task 771 (rePIU v0.0.201, design `docs/design/20261005-771-post-shader-command-line.md`).*

## 배경 / Background

작업 455는 `--post-shader <id>`를 넣었다. 그 뒤 rePIU가 작업 771에서 같은 옵션을 넣으며 형식과 오류 규칙을 정했고, 사용자가 그 명령행 규칙을 re2DJ에도 적용하라고 했다(2026-10-05).

*Task 455 added `--post-shader <id>`; rePIU then added the same option in task 771 with its form and error rules, and the user asked for those command-line rules in re2DJ too (2026-10-05).*

## rePIU 규칙과 re2DJ 상태 / rePIU's rules against re2DJ

| rePIU 작업 771 | re2DJ(작업 455 뒤) | 이번 작업 |
| --- | --- | --- |
| `--post-shader <id>`, `--post-shader=<id>` | 앞 형식만 | `=` 형식 추가 |
| 롬셋 앞뒤 어디든 | 옵션 위치 자유 | 그대로 |
| 명령행 > 환경 변수 > ini | 명령행 > 환경 변수 | 그대로(re2DJ에 ini 없음) |
| 값 없음·빈 값은 exit 1 | 값 없음은 사용법 오류(1), 빈 값 거부 | `=`의 빈 값도 거부 |
| 반복하면 마지막 값 | 마지막 값 | 그대로 |
| `--` 뒤는 옵션이 아님 | `--`는 모르는 인자 | `--` 추가 |
| 런처 세션 자식이 물려받음 | 런처 자식이 명령행·환경을 물려받음 | `--`를 자식 인자에서 뺌 |

## 결정 / Decisions

- `--post-shader=<id>`는 `--post-shader <id>`와 같다. `=` 뒤가 비면 `--post-shader needs a shader id, or none`으로 사용법 오류(exit 1).
- `--` 뒤의 인자는 옵션으로 읽지 않고 프로파일 id로 읽는다. re2DJ에서 옵션이 아닌 인자는 프로파일 id 하나뿐이다.
- 런처의 자식은 이 run의 인자 뒤에 자식용 옵션(`--guest-executable` 등)을 붙여 실행된다. 부모 인자에 `--`가 있으면 그 옵션들이 옵션으로 읽히지 않으므로, 자식 인자를 만들 때 `--`를 뺀다. 프로파일 id는 `-`로 시작하지 않으므로 `--`가 지키던 인자는 빼도 같게 읽힌다. Windows의 0x400000 재실행은 명령행을 그대로 넘기며 뒤에 붙는 것이 없어 영향이 없다.
- 다른 옵션의 `=` 형식은 넣지 않는다. rePIU도 이 옵션만 그렇게 했다.

*`--post-shader=<id>` equals `--post-shader <id>`, an empty value after `=` being a usage error (`--post-shader needs a shader id, or none`, exit 1). Arguments after `--` are read as the profile id rather than options, the only non-option argument re2DJ has. A launcher's child runs with child options (`--guest-executable` and the rest) appended after this run's arguments, which a `--` there would stop being options, so `--` is dropped from the child's arguments; profile ids never start with `-`, so what it guarded reads the same without it. The Windows 0x400000 relaunch passes the command line as it is with nothing appended, so it is unaffected. No `=` form for other options, as rePIU did it for this one only.*

## 검증 / Verification

실제 실행으로 두 형식, 타깃 앞 위치, 환경 변수 `none`보다 명령행 우선, `--` 뒤 타깃, 값 없음·빈 값 exit 1, 반복 시 마지막 값, 6th를 `--`와 함께 실행했을 때 자식 시작과 셰이더 상속을 확인한다. Windows x86 Debug와 WSL Linux x64(clang) 빌드·CTest.

*Real runs check both forms, the option before the target, the command line over `none` in the variable, the target after `--`, exit 1 for missing and empty values, the last of a repeated option, and 6th started with `--` reaching its child with the shader inherited; Windows x86 Debug and WSL Linux x64 (clang) builds and CTest.*
