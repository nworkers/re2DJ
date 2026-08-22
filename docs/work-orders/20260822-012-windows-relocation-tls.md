# 작업 지시: Windows native helper relocation과 TLS callback

## 목표

Windows native helper가 요청된 non-preferred base에 PE32를 mapping하고 HIGHLOW relocation 및 process-attach TLS callback을 entry point 전에 실행하게 합니다.

## Goal

Make the Windows native helper map PE32 at a requested non-preferred base, apply HIGHLOW relocations, and execute process-attach TLS callbacks before the entry point.

## 작업 항목

1. protocol v3 `LoadImageRequest`에 requested base와 file size를 추가합니다.
2. backend가 zero/preferred/non-preferred 요청 base를 helper에 전달하고 실제 결과를 검증하게 합니다.
3. image mapping을 `native_pe_image` header/source로 추출합니다.
4. native mapped image에 ABSOLUTE/HIGHLOW relocation block 검증과 적용을 구현합니다.
5. PE32 TLS callback 배열을 검증하고 `DLL_PROCESS_ATTACH` 순서로 실행합니다.
6. synthetic PE32에 `.data`, `.reloc`, TLS directory/callback/state와 relocation entries를 추가합니다.
7. integration probe가 requested base, callback-before-entry와 최종 result 51을 검증하게 합니다.
8. 관련 아키텍처, 포팅 계획, KB, README, TODO와 작업 로그를 갱신합니다.
9. x64/x86 warnings-as-errors build 및 모든 관련 test/probe를 실행하고 커밋합니다.

## Work items

1. Add requested base and file size to the protocol-v3 `LoadImageRequest`.
2. Make the backend pass zero/preferred/non-preferred bases and validate the actual result.
3. Extract image mapping into a `native_pe_image` header/source pair.
4. Validate and apply ABSOLUTE/HIGHLOW relocation blocks to the native mapped image.
5. Validate the PE32 TLS callback array and invoke callbacks for `DLL_PROCESS_ATTACH`.
6. Add `.data`, `.reloc`, a TLS directory/callback/state, and relocation entries to synthetic PE32.
7. Make the integration probe verify requested base, callback-before-entry ordering, and final result 51.
8. Update architecture, porting plan, KB, READMEs, TODO, and the work log.
9. Run x64/x86 warnings-as-errors builds and all related tests/probes, then commit.

## 완료 조건

preferred `0x10000000` image가 requested `0x11000000`에 적재되고 두 import가 정상 동작하며 TLS callback state 7을 포함한 result 51과 child exit 0을 확인하면 완료입니다.

## Completion criteria

The task is complete when the preferred-`0x10000000` image loads at requested `0x11000000`, both imports work, and the run produces result 51 incorporating TLS callback state 7 with child exit zero.
