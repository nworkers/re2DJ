# ez2dj4th Hardlock HLE 호환 경로 조사 작업 로그

관련 설계: [ez2dj4th Hardlock HLE 호환 경로 조사 설계](../design/20260901-130-ez2dj4th-hardlock-bypass-path.md)

*Related design: [ez2dj4th Hardlock HLE compatibility-path investigation](../design/20260901-130-ez2dj4th-hardlock-bypass-path.md).*

## 결과

- 최초 `main` 병합 결과는 2-parent merge commit `728b4c3`였으나, 후속 history 정리에서 기존 `main` 기준의 단일 선형 commit `cfce973`으로 재작성했습니다. local annotated tag `v0.0.19`도 새 commit으로 이동했으며 원격에는 push하지 않았습니다.
- 새 조사 브랜치 `task-130-ez2dj4th-hardlock-bypass-path`에서 기존 Windows injected runtime의 dynamic resolver, profile config memory injection, `FEnteDev` synthetic handle 및 Hardlock protocol tracker를 재검토했습니다.
- `CreateFileA`/`DeviceIoControl` import boundary 뒤에 profile-selected device backend를 두는 경로가 프로젝트 HLE 원칙에 부합하는 유효 후보임을 정리했습니다. 원본 EXE patch나 protection branch skip은 범위에서 제외했습니다.
- 2EZConfig-V2는 GPL-3.0-or-later로 확인했습니다. `API_CRYPT`/Function `0x0e`, offset `0x100`, `Bcnt × 8`이라는 high-level framing만 대조했고, GPL source의 primitive나 seed algorithm은 복사·번역·link하지 않았습니다.
- 공식 vendor driver와 원본 bounded trace가 확인한 IOCTL framing은 backend shape 설계의 근거로 유지합니다. 그러나 실제 `0x450` 6바이트 응답과 Function `0x0e` 8바이트 output vector는 확보되지 않았습니다.
- 기존 synthetic `0x450` replay와 descriptor tail은 branch reachability 실험값으로만 남겨 두며, 제품 기본 응답·seed algorithm·물리 dongle response로 승격하지 않았습니다.
- 새 source code는 추가하지 않았습니다. 따라서 실제 비밀값, module address, transform block, raw dump가 source·문서·로그·명령행·commit에 들어가지 않았습니다.

*The initial main merge result was the two-parent merge commit `728b4c3`, but the history was subsequently rewritten as the single linear commit `cfce973` on the previous main base. The local annotated tag `v0.0.19` was moved to the new commit, and no remote push was performed. On the new `task-130-ez2dj4th-hardlock-bypass-path` branch, the existing Windows injected-runtime dynamic resolver, profile-config memory injection, `FEnteDev` synthetic handle, and Hardlock protocol tracker were reviewed. A profile-selected device backend behind the `CreateFileA`/`DeviceIoControl` import boundary was identified as consistent with the project's HLE principles; original-EXE patching and protection-branch skipping remain out of scope. 2EZConfig-V2 was classified as GPL-3.0-or-later. Only its high-level `API_CRYPT`/Function `0x0e`, offset `0x100`, and `Bcnt × 8` framing were compared; its GPL primitive and seed algorithm were not copied, translated, or linked. Vendor-driver and bounded-original traces continue to support the IOCTL framing, but no real six-byte `0x450` response or Function `0x0e` eight-byte output vector was obtained. Existing synthetic replay and descriptor-tail values remain branch-reachability experiments only and were not promoted to product defaults, seed algorithms, or physical-dongle responses. No source code was added, and no real secret, module address, transform block, or raw dump entered source, documentation, logs, command lines, or commits.*

## 검증

- `git log --oneline --decorate --max-count=4`로 flat commit과 tag를 확인했습니다.
- `git status --short --branch`로 조사 브랜치에서 의도한 문서 변경 전 clean 상태를 확인했습니다.
- 2EZConfig 임시 clone은 라이선스와 high-level 동작 확인 후 exact temporary path를 삭제했습니다.
- `git diff --check`와 tracked diff의 secret redaction 검사를 완료했습니다.

*Verification: the flat commit and tag were checked with `git log --oneline --decorate --max-count=4`; the investigation branch was clean before the intended documentation changes; the 2EZConfig temporary clone was deleted after license and high-level behavior review; and `git diff --check` plus a tracked-diff secret-redaction check passed.*
