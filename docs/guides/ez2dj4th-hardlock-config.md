# ez2dj4th Hardlock 설정

근거: [현재 설계](../design/20260901-127-ez2dj4th-hardlock-runtime.md), [초기 외부 설정 설계](../design/20260901-126-ez2dj4th-hardlock-external-config.md)

*Basis: [current design](../design/20260901-127-ez2dj4th-hardlock-runtime.md), [initial external-configuration design](../design/20260901-126-ez2dj4th-hardlock-external-config.md).*

저장소 루트의 `cfg/hardlock.ini`에 설정을 만드십시오. `cfg/` 전체는 루트 `.gitignore`에서 제외되므로 완성된 파일이 Git status나 commit에 들어가지 않습니다. 각 값은 `0..65535` decimal 또는 `0x0000..0xFFFF` hex 형식입니다.

*Create the configuration at repository-root `cfg/hardlock.ini`. The root `.gitignore` excludes all of `cfg/`, so the completed file does not enter Git status or commits. Each value uses decimal `0..65535` or hexadecimal `0x0000..0xFFFF` syntax.*

```ini
[ez2dj4th]
modad=<16-bit value>
seed1=<16-bit value>
seed2=<16-bit value>
seed3=<16-bit value>
```

Windows x86 Debug build는 별도 설정 옵션 없이 실행합니다.

*Run the Windows x86 Debug build without a separate configuration option.*

```powershell
build/windows-x86/bin/Debug/re2dj.exe ez2dj4th --run
```

저장소 밖 설정을 사용해야 할 때만 `--hardlock-config <path>`를 지정합니다. 저장소 내부에서는 `cfg/` 밖의 경로를 거부합니다. section/key 누락·중복과 16-bit 범위 오류도 실행 전에 실패하며, 오류와 진단 로그는 실제 값을 출력하지 않습니다.

*Use `--hardlock-config <path>` only when an external configuration is needed. Inside the repository, paths outside `cfg/` are rejected. Missing or duplicate sections/keys and values outside the 16-bit range also fail before execution, while errors and diagnostics omit actual values.*
