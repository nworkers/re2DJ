# 작업 지시서: ez2dj1stse Hardlock 후보 판별

## 한국어

### 관련 문서

- 절차: [Hardlock seed 복구 워크스루](../guides/hardlock-seed-recovery-walkthrough.md)
- 절차: [Hardlock descriptor ID 추출](../guides/hardlock-descriptor-extraction.md)
- 분석: [ez2dj1stse CHD 파일시스템 분석](../analysis/ez2dj1stse-chd-filesystem.md)
- 선행 작업: [CHD 프로파일 실행 정책 정정](20260908-223-ez2dj1stse-chd-profile-correction.md)

### 작업 목표

사용자가 `cfg/ez2dj1stse/maps`에 준비한 seed 후보 map 129개를 원본 실행 파일에 주입해 판별하고, 복호화가 성립하는 map을 3rd·4th와 동일한 형태(`cfg/hardlock-ez2dj1stse.map` + `cfg/hardlock.ini`의 프로파일 section)로 확보합니다.

### 작업 항목

1. map 하나를 주입해 challenge 전부가 `mapped=1:unmapped=0`으로 덮이는지 먼저 확인합니다.
2. 후보 129개를 전부 주입 실행하고 워크스루 Stage 7의 신호를 기록합니다.
   - 종료 코드
   - `.vfs.log` 줄 수
   - `0x9c402450` handshake 횟수
   - `\\.\FEnteDev` 재개방 횟수
   - 게스트 설정 파일(`ez2dj.ini`) 열기 여부
3. 신호가 갈리는 후보를 식별하고 재실행으로 재현성을 확인합니다.
4. 확정된 map을 `cfg/hardlock-ez2dj1stse.map`으로, 재생값을 `cfg/hardlock.ini`의 `[ez2dj1stse]` section으로 배치합니다.
5. `cfg/ez2dj1stse/`에 `resolved-seeds.txt`, `resolved.mapcfg`, `maps/resolved.map`을 기록합니다.
6. `re2dj ez2dj1stse`가 자료를 자동 소비해 transform loop를 넘어가는지 확인합니다.
7. analysis와 work log를 갱신합니다.

### 제외 범위

- seed 후보 재생성 (사용자가 제공한 목록을 그대로 사용)
- `target_profile.cpp` 등 소스 변경 (선행 작업에서 이미 `hardlock_cfg_material_default`를 켜 두었음)
- graphics HLE 경로 신설
- seed 값·응답 바이트의 저장소 커밋

### 완료 조건

- 후보 129개의 판별 결과가 신호별로 정리됩니다.
- 복호화가 성립하는 후보가 하나로 특정되고 재현됩니다.
- `cfg/hardlock-ez2dj1stse.map`과 `cfg/hardlock.ini` `[ez2dj1stse]` section이 배치됩니다.
- `re2dj ez2dj1stse`가 별도 옵션 없이 그 자료를 소비합니다.

특정되지 않으면 그 사실과 관측 분포를 기록하고 다음 단계를 제시합니다. 임의의 후보를 확정하지 않습니다.

## English

### Related documents

- Procedure: [Hardlock seed recovery walkthrough](../guides/hardlock-seed-recovery-walkthrough.md)
- Procedure: [Hardlock descriptor ID extraction](../guides/hardlock-descriptor-extraction.md)
- Analysis: [ez2dj1stse CHD filesystem analysis](../analysis/ez2dj1stse-chd-filesystem.md)
- Preceding task: [CHD profile execution policy correction](20260908-223-ez2dj1stse-chd-profile-correction.md)

### Objective

Judge the 129 seed candidate maps the user prepared in `cfg/ez2dj1stse/maps` by injecting each into the original executable, and secure the map under which decryption holds in the same shape 3rd and 4th use — `cfg/hardlock-ez2dj1stse.map` plus a profile section in `cfg/hardlock.ini`.

### Work items

1. Inject one map first and confirm every challenge is covered with `mapped=1:unmapped=0`.
2. Run all 129 candidates and record the Stage 7 signals: exit code, `.vfs.log` line count, `0x9c402450` handshake count, `\\.\FEnteDev` reopen count, and whether the guest configuration file `ez2dj.ini` is opened.
3. Identify the candidate whose signals separate and confirm it reproduces on a rerun.
4. Place the confirmed map at `cfg/hardlock-ez2dj1stse.map` and the replay values in the `[ez2dj1stse]` section of `cfg/hardlock.ini`.
5. Record `resolved-seeds.txt`, `resolved.mapcfg`, and `maps/resolved.map` under `cfg/ez2dj1stse/`.
6. Confirm `re2dj ez2dj1stse` consumes that material automatically and passes the transform loop.
7. Update the analysis document and the work log.

### Out of scope

- Regenerating seed candidates; the user-supplied list is used as is
- Source changes such as `target_profile.cpp`, since the preceding task already enabled `hardlock_cfg_material_default`
- Adding a graphics HLE path
- Committing seed values or response bytes to the repository

### Completion criteria

- The judgement results for all 129 candidates are tabulated by signal.
- A single candidate under which decryption holds is identified and reproduced.
- `cfg/hardlock-ez2dj1stse.map` and the `cfg/hardlock.ini` `[ez2dj1stse]` section are in place.
- `re2dj ez2dj1stse` consumes that material with no extra options.

If no candidate separates, record that outcome and the observed distribution and propose the next step. Do not designate an arbitrary candidate.
