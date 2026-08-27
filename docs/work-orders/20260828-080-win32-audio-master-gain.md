# Win32 오디오 master gain 작업 지시

## 상태

**완료.** [Win32 오디오 master gain 설계](../design/20260828-080-win32-audio-master-gain.md)에 따라 구현과 asset-free 검증을 마쳤다.

## 작업

1. 제품 CLI와 `OriginalProcessOptions`에 검증된 dB 범위의 master gain 옵션을 추가한다.
2. 공용 launcher engine이 linear gain을 injected runtime export로 전달하게 한다.
3. SDL3_mixer backend가 초기화 시 master gain을 적용하고 조회 가능하게 한다.
4. product policy probe와 Windows runtime probe를 확장한다.
5. Win32 전체 build와 CTest를 수행한다.
6. README, 실행 guide, analysis와 작업 로그를 갱신한다.
7. 변경을 커밋한다.

## 완료 기준

- 기본 제품 실행이 `+6 dB` master 보정을 적용한다.
- `--audio-gain-db`로 `-24..+18 dB` 범위를 선택할 수 있다.
- DirectSound buffer별 dB 변환과 상대 음량은 바뀌지 않는다.
- asset-free probe와 Win32 CTest가 통과한다.

---

# Win32 Audio Master Gain Work Order

## Status

**Complete.** Implementation and asset-free verification are complete according to the [Win32 audio master gain design](../design/20260828-080-win32-audio-master-gain.md).

## Tasks

1. Add a validated dB master-gain option to the product CLI and `OriginalProcessOptions`.
2. Pass linear gain from the shared launcher engine to an injected-runtime export.
3. Apply and expose master gain during SDL3_mixer backend initialization.
4. Extend the product policy probe and Windows runtime probe.
5. Run the full Win32 build and CTest.
6. Update the README, runtime guide, analysis, and work log.
7. Commit the changes.

## Completion Criteria

Default product execution applies `+6 dB` compensation, `--audio-gain-db` accepts `-24..+18 dB`, per-buffer DirectSound dB conversion remains unchanged, and asset-free probes plus Win32 CTest pass.
