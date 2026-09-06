# ez2dj6th Hardlock descriptor 진단 작업 로그

## 결과

6th Hardlock descriptor 요청을 원문 ID 노출 없이 진단할 수 있도록 VFS trace에 헤더 필드와 `id_ref`·`id_verify` FNV-1a 해시를 추가했습니다. 사용자가 직접 참조할 수 있도록 일회성 raw extraction 후 `cfg/hardlock-id.ini`에 `[ez2dj6th]` 항목을 만들었습니다. 이 파일은 `.gitignore`의 `/cfg/` 규칙에 따라 커밋되지 않습니다.

확인된 공통 필드는 `module_address=0x4c51`, `remote=0x0001`, `port=0x0378`이며, descriptor 함수는 각각 `0x0000`, `0x0001`입니다. 두 요청의 ID는 동일했습니다. 기존 Hardlock 응답 동작은 변경하지 않았습니다.

일회성 raw-ID 계측 코드는 추출 직후 제거했고, raw ID가 기록된 임시 VFS 로그도 삭제했습니다. 이후 일반 로그에는 `id_ref_hash`와 `id_verify_hash`만 남습니다. 대신 프로파일 제작에 재사용할 수 있는 `--hardlock-descriptor-dump <path>` 옵션을 코드에 남겨, 첫 유효 descriptor를 해당 프로파일 section으로 추출하고 같은 파일의 다른 section은 보존하게 했습니다.

## 검증

- Windows x86 injected runtime Debug 빌드 성공
- 단위 테스트: 1,372 checks, 0 failures
- 6th 진단에서 descriptor 두 건의 header와 해시 확인
- `cfg/hardlock-id.ini` 생성 및 Git ignore 확인
- `--hardlock-descriptor-dump`로 별도 cfg 파일 생성 및 기존 로컬 참조값과 일치함을 확인

---

# ez2dj6th Hardlock Descriptor Diagnostic Work Log

## Result

Added fixed descriptor header fields and FNV-1a digests for `id_ref` and `id_verify` to the VFS trace without exposing raw IDs. After a one-time raw extraction, created `[ez2dj6th]` in `cfg/hardlock-id.ini` for the user's direct reference. The file is not committed because `.gitignore` ignores `/cfg/`.

The confirmed common fields are `module_address=0x4c51`, `remote=0x0001`, and `port=0x0378`; the descriptor functions are `0x0000` and `0x0001`. The IDs were identical across both requests. Existing Hardlock response behavior was unchanged.

The temporary raw-ID instrumentation was removed immediately after extraction, and the temporary VFS log containing raw IDs was deleted. Normal traces retain only `id_ref_hash` and `id_verify_hash`. The reusable `--hardlock-descriptor-dump <path>` option remains in the launcher/runtime so each future profile can update its own section while preserving other sections in a user-selected Git-ignored local file.

## Verification

- Windows x86 injected-runtime Debug build succeeded
- Unit tests: 1,372 checks, 0 failures
- Two descriptor headers and digests confirmed in a 6th diagnostic run
- `cfg/hardlock-id.ini` created and confirmed ignored by Git
- `--hardlock-descriptor-dump` created a separate cfg file whose values matched the existing local reference
