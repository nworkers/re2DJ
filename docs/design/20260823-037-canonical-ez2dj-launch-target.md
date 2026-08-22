# 정식 ez2dj.exe launcher 대상

## 설계

Windows x86 launcher probe의 1st SE 기본 target을 개발용 `ez2dj1stse_unpacked`에서 캐비닛의 정식 entry인 `ez2dj1stse`로 변경한다. 이 profile은 `System.ini`의 `shell=d:\\ez2dj\\ez2dj.exe`로 확인된 `ez2dj.exe`를 가리킨다.

`bring_up_target`은 target을 거부하는 실행 제약이 아니라 관찰 결과의 성격을 표시하는 metadata다. 따라서 launcher는 canonical profile도 실행·IAT 관찰 대상으로 허용한다. 보호된 entry의 unpacking 또는 추가 dynamic import는 아직 미확정이며, 관찰 결과는 protected build로 명시한다.

## English

The Windows x86 launcher probe changes its 1st SE default target from the development-only `ez2dj1stse_unpacked` to the cabinet's canonical `ez2dj1stse`. This profile points to `ez2dj.exe`, confirmed by `System.ini`'s `shell=d:\\ez2dj\\ez2dj.exe`.

`bring_up_target` is metadata that qualifies observations, not an execution restriction. The launcher therefore allows canonical profiles for execution and IAT observation. Unpacking behavior and additional dynamic imports from the protected entry remain unresolved, and observations will identify the protected build.
