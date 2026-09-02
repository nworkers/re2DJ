# EZ2DJ Hardlock 설정 (3rd·4th) / EZ2DJ Hardlock configuration

근거: [cfg 재료 기본값 설계](../design/20260902-140-hardlock-cfg-material-defaults.md), [후보 판별 작업 로그](../work-logs/20260902-139-hardlock-candidate-judgement.md), [HLE 정리 작업 로그](../work-logs/20260902-141-hardlock-hle-consolidation.md)

*Basis: the [cfg material defaults design](../design/20260902-140-hardlock-cfg-material-defaults.md), the [candidate judgement work log](../work-logs/20260902-139-hardlock-candidate-judgement.md), and the [HLE consolidation work log](../work-logs/20260902-141-hardlock-hle-consolidation.md).*

`ez2dj3rd`와 `ez2dj4th`는 아래 재료가 `cfg/`에 있으면 **아무 옵션 없이 자동으로 읽습니다.** 없으면 오류가 아니라 재료 없이 실행되어 예전 지점에서 멈춥니다. **이 저장소는 어느 값도 만들어내지 않습니다.** 응답은 별도 프로그램이 계산하며, re2DJ는 파일을 읽어 전달만 합니다.

*`ez2dj3rd` and `ez2dj4th` read the material below automatically **with no option given** when it is under `cfg/`. Its absence is not an error: the run proceeds without it and stops where it did before. **This repository produces none of these values** — a separate program computes the responses, and re2DJ only reads the files and relays them.*

## 재료 / Material

| 파일 | 내용 |
| --- | --- |
| `cfg/hardlock-<profile-id>.map` | 외부에서 계산한 challenge→response 매핑. 3rd 28줄, 4th 32줄 |
| `cfg/hardlock.ini`의 `response450` | `0x450` handshake 응답, 12 hex |
| `cfg/hardlock.ini`의 `tail44c` | `0x44c` descriptor tail word, 4 hex |

```ini
[ez2dj4th]
response450=<12 hex digits>
tail44c=<4 hex digits>

[ez2dj3rd]
response450=<12 hex digits>
tail44c=<4 hex digits>
```

매핑 파일은 한 줄에 challenge 16 hex와 response 16 hex를 두고, `#` 주석과 빈 줄을 허용하며, 중복 challenge를 거절합니다. 매핑에 없는 block은 항등으로 통과합니다.

`cfg/` 전체가 루트 `.gitignore`에서 제외되므로 완성된 파일이 Git status나 commit에 들어가지 않습니다. `cfg.ini`와 매핑 파일은 저장소 안에서는 `cfg/` 아래에만 둘 수 있습니다.

*The map file holds sixteen hex digits of challenge and sixteen of response per line, allows `#` comments and blank lines, and rejects duplicate challenges, with any block it does not cover passing through unchanged. All of `cfg/` is excluded by the root `.gitignore`, so a completed file never enters Git status or a commit, and inside the repository this material may live only under `cfg/`.*

## 실행 / Running

```powershell
build/windows-x86/bin/Debug/re2dj.exe ez2dj3rd
build/windows-x86/bin/Debug/re2dj.exe ez2dj4th --run
```

세 재료는 **전부 함께 적용되거나 전혀 적용되지 않습니다.** 매핑 파일이 없으면 `response450`과 `tail44c`도 쓰지 않습니다. 재료가 반쪽만 있으면 보호가 자기 대화상자를 띄우고 거기서 멈추기 때문입니다.

*The three materials are applied **all together or not at all**: without the map file, `response450` and `tail44c` go unused, because half the material leaves the protection sitting at its own dialog.*

## 확인 / Confirming

| 위치 | 무엇을 보는가 |
| --- | --- |
| launcher JSONL | `{"event":"hardlock_cfg_material","response450":true,"tail44c":true,"map":true}` — 어떤 재료가 적용됐는지. **값은 남지 않습니다** |
| launcher JSONL | `{"event":"hardlock_transform_map","entries":N}` — 읽은 매핑 항목 수 |
| `.vfs.log` | `re2dj:vfs:hardlock-device:request=<kind>:outcome=<outcome>:...:mapped=N:unmapped=M` — 요청마다 한 줄 |

주입이 완전했는지를 먼저 봅니다. transform 줄이 3rd는 32개, 4th는 36개 모두 `mapped=1:unmapped=0`이어야 합니다. `unmapped`가 하나라도 있으면 매핑이 challenge를 다 덮지 못한 것이므로 그 실행의 결과는 무의미합니다. `outcome=rejected-shape`는 확인된 vendor framing에서 벗어난 요청이라는 뜻이며, 장치 경계는 그런 요청을 강제로 성공시키지 않습니다.

*Check injection completeness first: all transform lines — 32 for 3rd, 36 for 4th — must read `mapped=1:unmapped=0`, and any `unmapped` makes that run's result meaningless because the map did not cover every challenge. `outcome=rejected-shape` marks a request outside the confirmed vendor framing, which the device boundary refuses rather than forcing to succeed.*

## 진단 실행 / Diagnostic runs

launcher probe로 직접 실행할 때는 재료를 명시적으로 줄 수 있습니다. 명시적 옵션이 profile 기본값보다 우선합니다.

```powershell
build/windows-x86/bin/Debug/re2dj_windows_x86_launcher_probe.exe `
  --hdd <staged CHD root> `
  --chd roms/ez2dj4th/4thTrax.chd `
  --target ez2dj4th --target-executable EZ2DJ/EZ2DJ.EXE `
  --device-mock-wts-console-session `
  --device-mock-hardlock-450-response <12-hex-digits> `
  --device-mock-hardlock-44c-tail <4-hex-digits> `
  --hardlock-transform-map <path> `
  --slot-writer-trace
```

- staged CHD root는 제품 CLI가 만든 경로이며, 해당 실행의 JSONL `vfs_mount` 이벤트에서 `dump_root` 값으로 확인합니다.
- `--hardlock-transform-map`은 장치 경계를 함께 켭니다. 경계만 켜려면 `--hardlock-device`를 씁니다.
- `--slot-writer-trace`는 4th의 bounded 실행 경계를 제공합니다. `--break-exit-process`만 쓰면 4th에 없는 정적 `KERNEL32!ExitProcess` IAT slot을 요구해 `requested import is not present`로 실패합니다.
- 3rd는 HDD 디렉터리에서 실행하며 `--hle-dynamic-vfs`가 필요합니다. profile 기본값이므로 `--target ez2dj3rd`로 실행하면 자동으로 켜집니다.

*The staged CHD root is created by the product CLI and read from that run's JSONL `vfs_mount` event as `dump_root`. `--hardlock-transform-map` also enables the device boundary, which `--hardlock-device` enables on its own. `--slot-writer-trace` supplies 4th's bounded execution boundary, since `--break-exit-process` alone fails with `requested import is not present` for a static `KERNEL32!ExitProcess` IAT slot 4th does not have. 3rd runs from an HDD directory and needs `--hle-dynamic-vfs`, which its profile enables by default.*

## 이 경계가 하지 않는 것 / What this boundary does not do

- Function `0x0e` 변환을 구현하지 않습니다. 응답을 계산하는 코드는 이 저장소에 없습니다.
- seed를 쓰지 않습니다. re2DJ 설정에는 re2DJ가 실제로 쓰는 값만 둡니다.
- 계약 밖 요청을 성공시키지 않습니다.
- 진단 로그에 응답 바이트를 남기지 않습니다.

*What this boundary does not do: implement the Function `0x0e` transform, since no code here computes a response; use seeds, because re2DJ's configuration holds only values re2DJ actually uses; succeed on requests outside the contract; or write response bytes into the diagnostic log.*
