# 작업 294 작업 지시 — 보호 빌드 덤프로 helper RVA 대조 / Task 294 work order — Cross-checking helper RVAs against protected-build dumps

선행: [작업 293 helper 시그니처 탐색](20260916-293-port-helper-signature-scan.md), [작업 292 복호화된 주 이미지 덤프](20260916-292-decrypted-image-dump.md)

상태: **완료.** [작업 로그](../work-logs/20260917-294-port-helper-dump-crosscheck.md)

## 한국어

### 목적

작업 293이 `docs/TODO.md`에 남긴 후속이다. 보호 빌드는 디스크에서 시그니처가 0건이므로, 작업 292의 `resumed` 덤프에 `re2dj_port_helper_scan`을 돌려 각 프로파일의 `legacy_io_in_rva`·`legacy_io_out_rva`가 그 빌드 자신의 값인지 확인한다.

설계는 작업 292·293의 것을 그대로 쓴다. 이 작업은 두 도구를 실행해 결과를 기록하며, 새 동작을 추가하지 않는다.

### 범위

1. 보호 빌드 `ez2dj1st`, `ez2dj1stse`, `ez2dj3rd`, `ez2dj4th`, `ez2dj5th`, `ez2d2m`을 `--image-dump`로 실행한다.
2. 각 `resumed`·`entry` 덤프를 스캔하고 프로파일 값과 대조한다.
3. 작업 293이 세운 가설 — 1st SE·5th가 `0xc0000096`에서 멈추는 원인이 I/O 경계일 수 있다 — 을 현재 실행으로 확인한다.
4. 결과를 `docs/analysis/ez2dj-io-map.md`에 확인됨/추정/미확정으로 기록한다.
5. 대조로 사실과 달라진 프로파일 **설명 문자열**을 고친다. 값은 불일치가 있을 때만 별도 판단한다.

### 검증

* Windows x86 Debug 빌드와 단위 테스트.

### 범위에서 뺀 것

* 시그니처 표 확장. 맞지 않는 빌드가 나오면 원인을 기록하되 항목 추가는 별도 작업이다.
* `ez2d2m` 입력 helper의 런타임 확인.

## English

Prerequisites: [Task 293, helper signature scan](20260916-293-port-helper-signature-scan.md), [Task 292, decrypted main-image dump](20260916-292-decrypted-image-dump.md)

Status: **complete.** [Work log](../work-logs/20260917-294-port-helper-dump-crosscheck.md)

### Purpose

The follow-up task 293 left in `docs/TODO.md`. Protected builds yield zero signature hits on disk, so `re2dj_port_helper_scan` is run over task 292's `resumed` dumps to check that each profile's `legacy_io_in_rva` and `legacy_io_out_rva` are that build's own values.

It reuses the designs of tasks 292 and 293: this task runs the two tools and records the results, adding no new behavior.

### Scope

1. Run the protected builds `ez2dj1st`, `ez2dj1stse`, `ez2dj3rd`, `ez2dj4th`, `ez2dj5th` and `ez2d2m` with `--image-dump`.
2. Scan each `resumed` and `entry` dump and compare against the profile values.
3. Check task 293's hypothesis — that the `0xc0000096` stop of 1st SE and 5th may be the I/O boundary — against current runs.
4. Record the results in `docs/analysis/ez2dj-io-map.md` as confirmed, inferred or unresolved.
5. Correct profile **note strings** the cross-check shows to be wrong. Values are weighed separately, and only if a mismatch appears.

### Verification

* Windows x86 Debug build and unit tests.

### Out of scope

* Extending the signature table. A build that does not match has its cause recorded, but new entries are a separate task.
* Run-time confirmation of the `ez2d2m` input helper.
