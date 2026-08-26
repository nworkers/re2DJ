# 렌더링 정확성·성능 회복 작업 로그

관련 설계: [렌더링 정확성·성능 회복](../design/20260826-072-render-correctness-performance.md)  
관련 작업 지시: [렌더링 정확성·성능 회복 작업 지시](../work-orders/20260826-072-render-correctness-performance.md)

## 결과

- RGB565 texture surface에 안정적인 identity와 content revision을 추가했다. GDI `ReleaseDC`와 color fill이 revision을 올린다.
- OpenGL backend는 surface별 texture를 cache하고 최초/변경 시에만 upload한다. source color-key low/high 범위는 upload 시 alpha로 변환하며 linear filtering과 clamp-to-edge 뒤에도 key 경계가 float 근사 비교에 의존하지 않는다.
- bounded state trace에서 원본의 stage 0 `MODULATE`, texture/diffuse argument, linear min/mag filter, color-key enable, alpha test `NOTEQUAL 0`, alpha blend와 `ZERO/SRCALPHA`·`ONE/ZERO` 조합을 확인해 draw-time fixed-function state로 적용했다.
- 성공 `DrawPrimitive`는 첫 1회만 기록하고 기본 I/O 상세 event를 억제했다. debugger mode 로그가 13만 줄 이상이던 사용자 실행과 달리 최종 19초 회귀 로그는 0.03 MB, 1회 draw marker, I/O 상세 0회, OpenGL 실패/AV 0회다.
- 로그 억제 뒤에도 debugger first-chance exception 왕복이 남음을 확인했다. injected runtime의 vectored handler가 1st SE의 두 helper RVA, `IN AL,DX`/`OUT DX,AL` opcode와 허용 port만 처리한다. `--run-detached`는 복원·주입·IAT 검증 뒤 debugger를 분리하고 원본 byte를 수정하지 않는다.

## 검증

- Windows x86 Debug warnings-as-errors build: 성공
- Windows x64 Debug warnings-as-errors build: 성공
- Windows x86 CTest: 2/2 성공
- Windows x64 CTest: 1/1 성공
- debugger 회귀 로그 `20260826-184241-943.jsonl`: OpenGL failure 0, `av_access` 0, 상세 I/O event 0
- detached 로그 `20260826-183749-602.jsonl`: runtime I/O 준비와 detach 성공, 40초 생존 후 검증용 강제 종료(`0xffffffff`)
- 원본 HDD와 자산: 변경·저장소 추가 없음

## 남은 확인

누락 그림, 투명 테두리와 체감 속도는 새 `--run-detached` command로 사용자가 재확인해야 한다. 오디오와 실제 입력 전이도 작업 072의 사용자 검증 항목으로 남는다.

---

# Rendering Correctness and Performance Recovery Work Log

The implementation adds stable surface identity/revision, per-surface OpenGL texture caching, RGB565 color-key-to-alpha conversion before linear filtering, clamp-to-edge sampling, and the observed stage-zero modulate, alpha-test, and blend-factor state. Draw success is reported once and default per-I/O diagnostics are suppressed.

Because debugger first-chance delivery remained the dominant I/O cost, the injected runtime now registers a vectored handler limited to the confirmed 1st SE helper RVAs, opcodes, and ports. `--run-detached` restores and verifies the guest under the debugger, then detaches without modifying original instruction bytes. x86/x64 warnings-as-errors builds and CTest pass. Debugger log `20260826-184241-943.jsonl` has zero OpenGL failures and access violations; detached log `20260826-183749-602.jsonl` confirms preparation, detachment, and 40 seconds of survival until the verification process was forcibly stopped. User revalidation of missing images, transparent borders, perceived speed, audio, and input remains pending.
