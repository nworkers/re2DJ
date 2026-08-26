# KSND title.wav 로드 실패 귀속 작업 로그

관련 설계: [KSND title.wav 로드 실패 귀속](../design/20260826-068-ksnd-title-load-attribution.md)
관련 작업 지시: [KSND title.wav 로드 실패 귀속 작업 지시](../work-orders/20260826-068-ksnd-title-load-attribution.md)

## 구현

- launcher에 1st SE 전용 `--ksnd-load-trace`를 추가했다.
- WAV parse, `CreateSoundBuffer`, buffer `Lock`, `Unlock` return site를 관찰한다.
- 각 breakpoint는 원래 명령을 single-step한 뒤 재무장하므로 앞선 sound load 뒤에도 유지된다.
- event에 원본 filename, EAX, sound slot, buffer pointer, parsed byte count와 retry index를 기록한다.
- 원본 흐름, HRESULT와 자산은 변경하지 않는다.

## 확인 결과

| 항목 | 실행 1 | 실행 2 |
| --- | --- | --- |
| 로그 | `20260826-001806-977.jsonl` | `20260826-001915-355.jsonl` |
| `title.wav` parse | 성공, 9,438,264 bytes | 성공, 9,438,264 bytes |
| `CreateSoundBuffer` | `E_NOTIMPL` 10/10 | `E_NOTIMPL` 10/10 |
| output buffer | null | null |
| Lock / Unlock | 미도달 | 미도달 |
| `av_access` | 0 | 0 |
| OpenGL failure | 0 | 0 |

`title.wav` 자체는 9,438,308바이트 RIFF/WAVE PCM stereo 44.1 kHz 16-bit 파일이며 header의 RIFF/data 크기와 실제 파일 길이가 일치한다. 따라서 검색 경로나 parser가 아니라 system DirectSound secondary-buffer 생성이 현재 경계다. `E_NOTIMPL`의 host 내부 원인은 아직 확정하지 않으며 다음 작업에서 원본 descriptor와 필요한 buffer method를 바탕으로 HLE를 설계한다.

## 검증

- Windows x86 Debug build: 성공
- Windows x86 CTest: 2/2 성공
- 원본 HDD/실행 파일: 읽기 전용 유지

---

# KSND title.wav Load-Failure Attribution Work Log

Task 68 adds a target-limited, filename-aware KSND stage trace that rearms breakpoints after single-stepping the restored instruction. It observes WAV parsing, IDirectSound::CreateSoundBuffer, IDirectSoundBuffer::Lock, and Unlock without changing guest results.

Final logs 20260826-001806-977.jsonl and 20260826-001915-355.jsonl both parse the 9,438,264-byte title.wav PCM payload successfully. CreateSoundBuffer returns E_NOTIMPL on all ten retries, leaves the output buffer null, and prevents Lock/Unlock from being reached. Both runs contain zero access violations and zero OpenGL failures. The asset header and file length are consistent, so the next boundary is DirectSound secondary-buffer HLE rather than VFS lookup or WAV parsing. The host-internal reason for E_NOTIMPL remains unresolved.
