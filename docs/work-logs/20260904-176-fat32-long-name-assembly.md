# 20260904-176 FAT32 긴 이름 조립 수정 결과
# 20260904-176 FAT32 Long-Name Assembly Fix Results

## 1. 개요 (Overview)

Task 175가 발견한 FAT32 긴 이름 결함을 고쳤다.

**결론: 긴 이름 슬롯의 마지막 문자 범위가 2문자가 아니라 4문자로 되어 있어, 슬롯 32바이트를 넘어 다음 항목에서 4바이트를 더 읽고 있었다. 범위를 고치자 13자를 넘는 모든 이름이 정확히 나온다. 이번 실패의 직접 원인이던 `2PLAYERInsertCoin.str`도 이제 `EZ2DJ/SYSTEM/Common`에서 온전한 이름으로 보인다.**

The final character field of a long-name slot was read as four characters instead of two, running four bytes past the 32-byte slot into the next entry. With the range corrected, every name longer than thirteen characters decodes exactly, including `2PLAYERInsertCoin.str`, the file whose load failure caused the crash.

---

## 2. 변경 내용 (Changes Implemented)

1. **이름 해독 단위 분리.** `include/re2dj/storage/fat32_directory_name.h`와 `src/storage/fat32_directory_name.cpp`를 추가하고 `DecodeFatShortName`, `FatShortNameChecksum`, `FatLongNameAssembler`, 그리고 UTF-16 변환을 옮겼다. 이름 해독을 이미지 없이 시험할 수 있게 하는 것이 분리의 목적이다.
2. **범위 수정.** 슬롯의 문자 범위를 `{{1, 5}, {14, 6}, {28, 2}}`로 고쳐 슬롯당 정확히 13문자를 취한다.
3. **호출 지점 정리.** `src/storage/fat32_chd.cpp`가 새 단위를 쓰고, 긴 이름 슬롯 판별에 `kFatLongNameAttribute`를 쓴다.
4. **단위 시험.** `tests/unit/fat32_directory_name_test.cpp`를 추가했다. 합성 디렉터리 항목만 사용한다.
5. **문서.** `ARCHITECTURE.md`에 새 단위를 반영했다.

---

## 3. 검증 결과 (Verification Results)

- `scripts/build_win32.bat`: 빌드 성공, 경고와 에러 0.
- `re2dj_unit_tests.exe`: 1,265 checks, 0 failures. 수정 전 1,253에서 12 증가.
- `re2dj_windows_product_loader_probe.exe`: 전체 통과.

### 3.1 시험이 결함을 실제로 잡는다 (확인됨)

범위를 결함 상태(`{28, 4}`)로 되돌려 빌드하면 새 시험 넷이 실패한다.

```
fat32_directory_name_test.cpp:131: FAILED DecodeName("Credits_0.abm", "CREDIT~1ABM") == "Credits_0.abm"
fat32_directory_name_test.cpp:134: FAILED DecodeName("INSERTCOIN.str", "INSERT~1STR") == "INSERTCOIN.str"
fat32_directory_name_test.cpp:139: FAILED DecodeName("1PLAYERInsertCoin.str", ...) == "1PLAYERInsertCoin.str"
fat32_directory_name_test.cpp:144: FAILED DecodeName("Channel_Eyecatch_mask.abmx", ...) == ...
checks: 1265, failures: 4
```

수정 상태에서는 1,265 checks 전부 통과한다. 13자 이하 이름과 체크섬·순서 검사는 두 상태 모두에서 통과하므로, 실패한 넷이 정확히 이번 결함의 범위다.

### 3.2 실제 이미지에서 이름이 정상이다 (확인됨)

`re2dj_chd_probe --list`를 같은 디렉터리에 다시 실행했다.

| 수정 전 | 수정 후 |
| - | - |
| `Credits_0.abm剃䑅` | `Credits_0.abm` |
| `INSERTCOIN.st义䕓r` | `INSERTCOIN.str` |
| `logo4th_mask.佌佇abm` | `logo4th_mask.abm` |
| `1PLAYERInsert倱䅌Coin.str` | `1PLAYERInsertCoin.str` |

- **확인됨 — 요청된 파일은 실재한다.** `EZ2DJ/SYSTEM/Common`에 `1PLAYERInsertCoin.str`(1,556 바이트), `2PLAYERInsertCoin.str`(1,556 바이트), `1PLAYERPressStart.str`, `2PLAYERPressStart.str`, `1PLAYERWait.str`, `2PLAYERWait.str`가 있다. 수정 전에는 이 이름들이 조립 결함으로 가려져 있었다.

---

## 4. 남은 것 (What Remains)

이 수정만으로 게스트의 자원 적재가 성공하지는 않는다. 게스트는 여전히 상대 이름으로 열고, re2DJ의 VFS는 그것을 `EZ2DJ/<name>`으로 해석한다. 실제 파일은 `EZ2DJ/SYSTEM/Common`에 있다. 두 결함 중 하나만 해소된 상태다.

This fix alone does not make the guest's load succeed: the VFS still resolves bare names to `EZ2DJ/<name>` while the files live under `EZ2DJ/SYSTEM/Common`.

---

## 5. 다음 작업 (Next Task)

VFS에 게스트의 논리 현재 디렉터리를 도입한다. `SetCurrentDirectoryA`와 `GetCurrentDirectoryA`를 HLE로 받아 게스트가 보는 현재 디렉터리를 유지하고, 상대 이름을 그 기준으로 해석한 뒤 매핑한다. 호스트 프로세스의 실제 작업 디렉터리는 바꾸지 않는다.

Give the VFS a tracked guest working directory fed by HLE `SetCurrentDirectoryA` and `GetCurrentDirectoryA`, resolve relative names against it, and leave the host process's real directory alone.

---

## 6. 관련 문서 (Related Documents)

- [Task 176 설계](../design/20260904-176-fat32-long-name-assembly.md)
- [Task 176 작업 지시서](../work-orders/20260904-176-fat32-long-name-assembly.md)
- [Task 175 작업 로그](../work-logs/20260904-175-ez2dj4th-uninitialized-out-parameter.md)
- [4th Hardlock 런타임 분석](../analysis/ez2dj4th-hardlock-runtime.md)
