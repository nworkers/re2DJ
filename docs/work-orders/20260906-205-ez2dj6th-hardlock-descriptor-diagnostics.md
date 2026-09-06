# ez2dj6th Hardlock descriptor 진단 작업 지시서

## 관련 설계

[ez2dj6th Hardlock descriptor 진단 설계](../design/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md)

## 목표

6th가 Hardlock 응답 뒤 종료되는 원인을 추적할 수 있도록 descriptor 헤더와 비가역 ID 해시를 VFS 진단 로그에 추가합니다.

## 작업 항목

1. descriptor 고정 헤더 파싱 및 로그 필드를 추가합니다.
2. `id_ref`·`id_verify`는 FNV-1a 해시와 non-zero 여부만 기록합니다.
3. 기존 HLE 응답 동작과 malformed request 처리를 보존합니다.
4. 6th 실행 로그에서 두 descriptor 요청을 확인합니다.
5. 빌드와 단위 테스트를 실행하고 작업 로그를 남깁니다.
6. 사용자가 요청한 6th 확인값을 Git에서 무시되는 `cfg/hardlock-id.ini`에 기록합니다.
7. 프로파일 제작에 재사용할 수 있도록 `--hardlock-descriptor-dump <path>` 옵션으로 첫 유효 descriptor의 원문 ID와 `module_address`를 해당 프로파일 section에 추출하고 다른 section을 보존합니다.

## 범위 제외

- 6th Hardlock 응답값 추측 또는 생성
- 4th material 자동 상속
- 일반 로그·문서·저장소에 원본 ID 바이트 기록
- 추출 옵션으로 사용자가 지정한 Git-ignored 로컬 파일에 기록하는 동작 자체

---

# ez2dj6th Hardlock Descriptor Diagnostic Work Order

## Related design

[ez2dj6th Hardlock Descriptor Diagnostic Design](../design/20260906-205-ez2dj6th-hardlock-descriptor-diagnostics.md)

## Objective

Add descriptor header fields and irreversible ID digests to the VFS diagnostic trace so the reason 6th exits after Hardlock responses can be investigated.

## Work items

1. Add fixed descriptor header parsing and diagnostic fields.
2. Record only FNV-1a digests and non-zero flags for `id_ref` and `id_verify`.
3. Preserve existing HLE response behavior and malformed-request handling.
4. Confirm both descriptor requests in a 6th run log.
5. Run the build and unit tests and leave a work log.
6. Store the requested 6th reference values in the Git-ignored `cfg/hardlock-id.ini`.
7. Add the reusable `--hardlock-descriptor-dump <path>` option to extract raw IDs and `module_address` from the first valid descriptor into the selected profile section while preserving other sections in the explicitly selected local file.

## Out of scope

- Guessing or generating the 6th Hardlock response
- Automatically inheriting 4th material
- Recording raw ID bytes in normal logs, documents, or the repository
- The explicitly requested extraction into a user-selected Git-ignored local file
