# 작업 449 설계 — Windows CLI의 in-process 전환 / Task 449 design — switching the Windows CLI to the in-process runner

선행: [작업 446 설계](20261004-446-windows-in-process-loader.md)(4단계), [작업 448 설계](20261004-448-windows-x86-backend.md)

## 결정 / Decisions

### 1. CLI / The CLI

- CLI의 in-process 경로(`#if defined(__linux__)`)를 두 OS 공용(`RE2DJ_IN_PROCESS_HOST`)으로 넓힌다. Windows의 `--run`은 주입 경로(`RunOriginalProcess`) 대신 공용 러너를 쓴다. CLI에서 주입 호출 블록 두 개와 그 전용 도우미를 뺀다. 주입 라이브러리와 도구는 450에서 지운다.
- 이름도 OS 중립으로: `RunInProcessOriginal`, `InProcessHostLifetime`, `IsContinuationRun`, `BuildGuestDevices`, `LoadIoBindings`, `g_presentation`·`g_audio`·`g_process_launcher`. `--linux-in-process-*` 진단 옵션은 문서·스크립트가 쓰는 이름이라 그대로 두고 설명만 "In-process diagnostic"으로 바꾼다.
- 주입 경로만 받던 `--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace`, `--vsync`는 이제 두 OS 모두 거부한다(설계 446의 사용자 결정). `--image-dump`는 2절대로 공용 러너에 다시 만든다.

*The CLI's in-process path (`#if defined(__linux__)`) widens to both OSes (`RE2DJ_IN_PROCESS_HOST`); Windows `--run` takes the shared runner instead of injection (`RunOriginalProcess`), the CLI losing both injection blocks and their helper, while the injection library and tools go in 450. Names become OS-neutral (`RunInProcessOriginal`, `InProcessHostLifetime`, `IsContinuationRun`, `BuildGuestDevices`, `LoadIoBindings`, `g_presentation`, `g_audio`, `g_process_launcher`); the `--linux-in-process-*` diagnostic options keep their names, which documents and scripts use, and are described as in-process diagnostics. `--demo-volume`, `--audio-volume-trace`, `--guest-wait-trace` and `--vsync`, which only injection took, are refused on both OSes (the user's decision in design 446), and `--image-dump` comes back on the shared runner (2 below).*

### 2. `--image-dump`

- 공용 `native_image_dump`가 매핑된 게스트 이미지를 virtual 레이아웃 그대로 쓰고, 예전과 같은 필드의 `.image.json`을 남긴다(`source: "in-process"`, `gaps: []`).
- "entry" 덤프는 setup 단계(이미지 매핑과 import 연결 뒤, 진입 전)에서, "resumed" 덤프는 지연이 지난 뒤 처음 오는 import에서 게스트 스레드가 쓴다. 다른 스레드가 실행 중 이미지를 읽으면 run이 끝나며 매핑을 푸는 것과 경합하므로, 게스트가 import에 멈춰 이미지가 확실히 매핑된 순간을 고른다. 지연 전에 run이 끝나면 건너뛴 사실을 경고로 남긴다.
- 위치: `logs/image-dumps/<target>/<시각>-<실행 파일>.<point>.image.{bin,json}`. 런처의 자식도 자기 실행 파일 이름으로 쓴다.

*The shared `native_image_dump` writes the mapped guest image in its virtual layout with an `.image.json` carrying the former fields (`source: "in-process"`, `gaps: []`). The "entry" dump is written at setup (image mapped and imports bound, before the entry), the "resumed" one by the guest thread at the first import after the delay: another thread reading the running image would race the run's end unmapping it, so the moment chosen is one where the guest is stopped in an import and the image surely mapped. A run that ends before the delay leaves a warning that the dump was skipped. Files go to `logs/image-dumps/<target>/<time>-<executable>.<point>.image.{bin,json}`, a launcher's child naming its own executable.*

### 3. 0x400000 확보 / Securing 0x400000

첫 실행에서 게스트 이미지 매핑이 실패했다. 실제 주소 배치를 보니 두 가지가 0x400000을 차지했다.

- `/STACK:16MB`로 링크한 주 스레드 스택. Windows는 아래에서 위로 채우므로 0x400000 빈자리에 놓였다.
- 스택을 빼도, 로더가 프로세스 시작 시 매핑하는 시스템 데이터(MEM_MAPPED 0x31000 바이트)가 그 자리에 놓였다. 실행 파일의 TLS callback에서 예약해도 이미 늦었다(ERROR_INVALID_ADDRESS).

결정: re2dj.exe가 자기 자신을 `CREATE_SUSPENDED`로 다시 띄우고, 자식의 로더가 돌기 전에 `VirtualAllocEx`로 0x00400000–0x04400000을 예약(MEM_RESERVE)한 뒤 재개한다. 자식의 로더는 그 영역을 피하고, 자식은 환경 변수로 자신이 예약된 채 시작했음을 알아 예약을 넘겨받는다. 첫 게스트 이미지를 매핑하기 직전 `HostMapAt`이 예약을 푼다. 코드를 주입하지 않고 주소만 예약하므로 446의 원칙과 맞는다. 부모는 Job 객체(`KILL_ON_JOB_CLOSE`)로 자식과 수명을 같이하고, 표준 핸들과 상속 핸들(자식 run의 종료 코드 파이프)을 넘기며, 자식의 종료 코드를 그대로 돌려준다. 자식은 환경 변수를 바로 지워서, 자기가 띄우는 런처의 자식도 같은 과정을 거치게 한다. 게스트는 16 MiB 스택의 전용 스레드에서 돈다(링크 옵션 `/STACK`은 쓰지 않는다).

*The first run failed to map the guest image. The actual layout showed two occupants of 0x400000: the main thread's stack, linked with `/STACK:16MB` and placed in that hole by bottom-up allocation; and, without it, system data the loader maps at process start (MEM_MAPPED, 0x31000 bytes), already there when the executable's own TLS callback tried to reserve (ERROR_INVALID_ADDRESS). Decision: re2dj.exe starts itself again with `CREATE_SUSPENDED`, reserves 0x00400000–0x04400000 (MEM_RESERVE) in the new process with `VirtualAllocEx` before its loader runs, and resumes it; the new loader avoids the range, and the new process, told by an environment variable that it started reserved, takes the reservation over, `HostMapAt` releasing it right before the first guest image is mapped. Only an address range is reserved and no code injected, in line with design 446. The parent shares its lifetime with the child through a job object (`KILL_ON_JOB_CLOSE`), passes the standard and inherited handles (a child run's exit-code pipe), and returns the child's exit code; the child clears the variable at once, so the children its launcher starts go through the same steps. The guest runs on a dedicated thread with a 16 MiB stack, no `/STACK` link option being used.*

## 검증 / Verification

Windows x86 build·CTest, 다섯 타깃(4th, 1st SE, 5th, EZ2Dancer 2nd MOVE, 6th)의 in-process 실행(`--call-limit`으로 스스로 끝나게), 4th의 `--image-dump`, Linux 두 폭 build·CTest와 6th 실행.

*The Windows x86 build and CTest, in-process runs of the five targets (4th, 1st SE, 5th, EZ2Dancer 2nd MOVE, 6th) ending on their own through `--call-limit`, `--image-dump` on 4th, and both Linux widths' builds and CTest with a 6th run.*
