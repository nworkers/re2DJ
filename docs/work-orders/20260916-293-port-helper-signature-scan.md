# 작업 293 작업 지시 — legacy I/O helper 시그니처 탐색 / Task 293 work order — Locating legacy I/O helpers by signature

설계: [20260916-293-port-helper-signature-scan.md](../design/20260916-293-port-helper-signature-scan.md)
선행: [작업 292 복호화된 주 이미지 덤프](20260916-292-decrypted-image-dump.md)

## 한국어

### 변경 목록

| # | 파일 | 변경 |
| --- | --- | --- |
| 1 | `include/re2dj/exe/code_scan.h` | `PortHelperKind`, `PortHelperSite`, `ScanPortHelpers` 선언 |
| 2 | `src/exe/code_scan.cpp` | 시그니처 표와 탐색 구현 |
| 3 | `src/tools/port_helper_scan/main.cpp` | 오프라인 CLI |
| 4 | `tests/unit/port_helper_scan_test.cpp` | 합성 버퍼 단위 테스트 |
| 5 | `tests/unit/main.cpp` | 테스트 등록 |
| 6 | `CMakeLists.txt` | 도구와 테스트 등록 |

### 시그니처 표

설계에서 1st SE 정식 빌드의 실제 바이트로 확인한 다섯 가지다. 추정으로 항목을 늘리지 않는다.

| kind | 바이트 | opcode 오프셋 |
| --- | --- | --- |
| `kInPortByte` | `33 C0 66 8B 54 24 04 EC C3` | 7 |
| `kInPortWord` | `66 8B 54 24 04 66 ED C3` | 5 |
| `kInPortDword` | `66 8B 54 24 04 ED C3` | 5 |
| `kOutPortByte` | `66 8B 54 24 04 8A 44 24 08 EE C3` | 9 |
| `kOutPortWord` | `66 8B 54 24 04 66 8B 44 24 08 66 EF C3` | 10 |

현재 다섯 시그니처는 어느 것도 같은 오프셋에서 동시에 맞지 않는다. 모두 한쪽이 끝나기 전에 갈라지기 때문이다. 그래도 표는 긴 것부터 두어, 나중에 겹치는 항목이 생기면 **가장 긴 것만** 보고되도록 한다.

### CLI

```
re2dj_port_helper_scan <file> [base-address]
```

* `base-address` 기본값은 `0`이며 이때 출력은 RVA다. `0x00400000`을 주면 VA가 된다.
* 입력은 평문 `.text`를 가진 디스크 파일 또는 작업 292의 `resumed` 덤프다. 둘 다 파일 오프셋이 곧 RVA다.
* kind별로 시그니처 오프셋과 opcode 주소를 한 줄씩 출력한다.
* 하나도 못 찾으면 그 사실을 출력하고 종료 코드 `1`을 준다. 보호 빌드의 디스크 파일에서 이것이 정상 결과이므로 오류 메시지로 단정하지 않는다.

### 제약

* 탐색은 구문적이며 결과는 후보다. 출력과 문서 양쪽에 이 점을 남긴다.
* 시그니처는 실제 바이트로 확인한 것만 넣는다.
* 원본 자산을 읽기만 하고 저장소에 넣지 않는다. 도구는 아무것도 쓰지 않는다.
* `re2dj::exe`는 플랫폼 중립이므로 호스트 OS API를 부르지 않는다.
* 프로파일 값은 이번 작업에서 **바꾸지 않는다.** 불일치를 발견하면 기록만 한다. 값 교체는 런타임 확인을 거쳐 별도 작업으로 한다.

### 검증

1. Windows x86 Debug·Release 전체 빌드.
2. 단위 테스트와 CTest 실행. `re2dj_windows_vfs_runtime_probe`의 기존 실패는 그대로 확인한다.
3. 1st SE 정식 빌드에 도구를 돌려 `0x00038987`과 `0x000389ab`를 되찾는지 확인한다. 이 두 값은 프로파일과 일치하므로 회귀 기준이 된다.
4. 보호 빌드의 디스크 파일에서는 하나도 나오지 않음을 확인한다. `.text`가 암호문이라는 뜻이며 작업 292의 결론과 일치해야 한다.

### 실행이 필요한 후속 — 이번 작업 범위 밖

3·4는 자산만으로 가능하지만, 보호 빌드의 실제 helper RVA를 얻으려면 작업 292의 덤프가 있어야 하고 덤프는 실제 실행이 필요하다. 다음 순서로 별도 진행한다.

1. 대상 빌드마다 `re2dj <target> --image-dump --image-dump-delay <ms>` 실행.
2. `resumed` 덤프에 이 도구를 돌려 후보 수집.
3. 프로파일 값과 대조. 특히 값이 다른 빌드에서 넘어온 `ez2dj1stse`와 `ez2dj5th`.
4. 결과를 `docs/analysis/ez2dj-io-map.md`에 확인됨 / 추정 / 미확정으로 반영.

### 완료 조건

* 위 검증 4개.
* `docs/analysis/ez2dj-io-map.md`에 시그니처 표와 1st SE 확인 결과, 그리고 어느 프로파일 값이 아직 그 빌드에서 확인되지 않았는지 기록.
* 작업 로그.

### 범위에서 뺀 것

* 프로파일 RVA 값 변경.
* 실제 덤프 수집과 보호 빌드 대조. 실행이 필요하므로 위에 후속으로 분리했다.
* 역어셈블러 도입.
* helper가 읽고 쓰는 port의 의미. 별개 경계다.

## English

Design: [20260916-293-port-helper-signature-scan.md](../design/20260916-293-port-helper-signature-scan.md)
Prerequisite: [Task 292, decrypted main-image dump](20260916-292-decrypted-image-dump.md)

### Change list

| # | File | Change |
| --- | --- | --- |
| 1 | `include/re2dj/exe/code_scan.h` | Declare `PortHelperKind`, `PortHelperSite`, `ScanPortHelpers` |
| 2 | `src/exe/code_scan.cpp` | The signature table and the search |
| 3 | `src/tools/port_helper_scan/main.cpp` | The offline CLI |
| 4 | `tests/unit/port_helper_scan_test.cpp` | Unit tests over synthetic buffers |
| 5 | `tests/unit/main.cpp` | Test registration |
| 6 | `CMakeLists.txt` | Tool and test registration |

### Signature table

The five confirmed against the 1st SE canonical build's actual bytes in the design. Do not add entries by inference.

| Kind | Bytes | Opcode offset |
| --- | --- | --- |
| `kInPortByte` | `33 C0 66 8B 54 24 04 EC C3` | 7 |
| `kInPortWord` | `66 8B 54 24 04 66 ED C3` | 5 |
| `kInPortDword` | `66 8B 54 24 04 ED C3` | 5 |
| `kOutPortByte` | `66 8B 54 24 04 8A 44 24 08 EE C3` | 9 |
| `kOutPortWord` | `66 8B 54 24 04 66 8B 44 24 08 66 EF C3` | 10 |

None of the five current signatures can match at the same offset, since each diverges before either ends. The table is still ordered longest first so that a later overlapping entry resolves to **only the longest**.

### CLI

```
re2dj_port_helper_scan <file> [base-address]
```

* `base-address` defaults to `0`, making the output RVAs; passing `0x00400000` makes them VAs.
* The input is either a disk file with plaintext `.text` or task 292's `resumed` dump; a file offset is an RVA in both.
* Print one line per hit with its signature offset and opcode address.
* On no hits, say so and exit `1`. That is the correct result for a protected build's disk file, so it must not be phrased as an error.

### Constraints

* The search is syntactic and its results are candidates; say so in both the output and the documents.
* Only signatures confirmed against real bytes go in the table.
* Original assets are read, never copied into the repository, and the tool writes nothing.
* `re2dj::exe` is platform-neutral and calls no host OS API.
* Profile values are **not changed** in this task. A mismatch is recorded only; replacing a value needs run-time confirmation and its own task.

### Verification

1. Windows x86 Debug and Release full builds.
2. Unit tests and CTest, with the pre-existing `re2dj_windows_vfs_runtime_probe` failure confirmed as such.
3. Run the tool on the 1st SE canonical build and confirm it recovers `0x00038987` and `0x000389ab`. Those agree with the profile, so they serve as the regression baseline.
4. Confirm that a protected build's disk file yields no hits, meaning its `.text` is ciphertext, consistent with task 292.

### Follow-up needing a run — outside this task

Verifications 3 and 4 need only the assets, but obtaining a protected build's real helper RVAs needs a task 292 dump, and a dump needs a real run. That proceeds separately: run `re2dj <target> --image-dump --image-dump-delay <ms>` per build, run this tool on the `resumed` dump, compare against the profile — especially `ez2dj1stse` and `ez2dj5th`, whose values came from other builds — and record the outcome in `docs/analysis/ez2dj-io-map.md` as confirmed, inferred, or unresolved.

### Completion criteria

* The four verifications above.
* `docs/analysis/ez2dj-io-map.md` carrying the signature table, the 1st SE result, and which profile values remain unconfirmed against their own build.
* A work log.

### Out of scope

* Changing profile RVA values.
* Collecting real dumps and comparing protected builds, separated above because it needs a run.
* Introducing a disassembler.
* The meaning of the ports these helpers read and write, which is a separate boundary.
