# ez2dj6th Hardlock 시드 설정 및 실행 검증 작업 로그

## 작업 정보

* 날짜: 2026-09-12
* 작업 지시: [docs/work-orders/20260912-253-ez2dj6th-seed-verification.md](file:///e:/MYWORK/Projects/re2DJ/docs/work-orders/20260912-253-ez2dj6th-seed-verification.md)
* 상태: 완료

---

## 작업 내용

### 한국어

1. **`hardlock.ini` 시드 설정**:
   - `reSoftlock`의 194개 후보군 중 제품군 쌍 B(`seed1=0xbe03`, `seed2=0xa335`)에 해당하는 유일한 후보인 `candidate-140`(`seed3=0x9a2b`)을 `cfg/hardlock.ini`의 `[ez2dj6th]` 섹션에 구성했습니다.
     ```ini
     [ez2dj6th]
     module_address=0x4c51
     seed1=0xbe03
     seed2=0xa335
     seed3=0x9a2b
     response450=0100fafa0010
     tail44c=0001
     ```

2. **런타임 실행 검증**:
   - `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini`를 실행했습니다.
   - 로그 `20260912-015652-740.jsonl` 및 `20260912-015652-740.child.vfs.log`:
     - 부모 bootstrap(`EZ2DJ.EXE`)에서 child process(`EZ2DJ6th.EXE`)를 생성하고, 주입 런타임 및 시드(`module_address: 0x4c51`) 핸드오프가 성공했습니다.
     - child 프로세스가 `\\.\FEnteDev`를 열고 0x9c402450 handshake 2회, 0x9c40244c descriptor 1회를 통과했습니다.
     - `.\EZ2DJ.ini` 파일을 CHD VFS로부터 정상 읽기(816 bytes)를 수행했습니다.
     - 0x9c402458 IOCTL 요청(Function 0x0011, 7 blocks = 312 bytes API_CODE)이 발생하여, `HardlockEngine`의 동적 페이로드 연산을 통해 처리되었습니다.
     - 이후 0x9c40244c cleanup descriptor (Function 1)가 호출되고, 프로세스가 정상 종료(`exit_process code 0x00000000`, `outcome: success`)에 도달했습니다.
   - 이로써 `ez2dj6th`의 시드가 완벽히 검증되었습니다.

### English

1. **`hardlock.ini` Seed Configuration**:
   - Configured `candidate-140` (`seed3=0x9a2b`), the only candidate matching family Pair B (`seed1=0xbe03`, `seed2=0xa335`), under the `[ez2dj6th]` section of `cfg/hardlock.ini`.

2. **Runtime Execution Verification**:
   - Executed `re2dj ez2dj6th --io-config .\config\ez2dj-io.example.ini`.
   - Logs `20260912-015652-740.jsonl` and `20260912-015652-740.child.vfs.log`:
     - Parent bootstrap (`EZ2DJ.EXE`) spawned child process (`EZ2DJ6th.EXE`), and runtime/seed (`module_address: 0x4c51`) handoff succeeded.
     - Child opened `\\.\FEnteDev`, passing two 0x9c402450 handshakes and one 0x9c40244c descriptor.
     - Read `.\EZ2DJ.ini` (816 bytes) from CHD VFS.
     - Dispatched 0x9c402458 IOCTL (Function 0x0011, 7 blocks = 312 bytes API_CODE), dynamically computed by `HardlockEngine`.
     - Called 0x9c40244c cleanup descriptor (Function 1) and reached clean termination (`exit_process code 0x00000000`, `outcome: success`).
   - Confirmed `ez2dj6th` seeds.
