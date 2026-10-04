# 작업 449 작업 지시서 — Windows CLI의 in-process 전환 / Task 449 work order — switching the Windows CLI to the in-process runner

설계: [20261004-449-windows-cli-in-process.md](../design/20261004-449-windows-cli-in-process.md) · 상위: [작업 446 설계](../design/20261004-446-windows-in-process-loader.md)

## 절차 / Steps

1. 자식 run 옵션 이름을 `include/re2dj/platform/native/child_run_options.h`로 옮기고 Windows 런처(`CreateProcessA`, 상속 파이프)를 만든다.
   *Move the child run option names to a shared header and add the Windows launcher.*
2. CLI의 in-process 경로를 두 OS 공용으로, Windows 주입 호출 블록 제거, 이름 정리, 주입 전용 옵션 거부.
   *Widen the CLI's in-process path to both OSes, drop the injection blocks, rename, refuse the injection-only options.*
3. Windows backend에 SDL host·오디오·런처, `re2dj.exe`의 링크 옵션과 backend.
   *SDL hosts, audio and the launcher in the Windows backend; re2dj.exe's link options and backend.*
4. 0x400000 확보: 일시 정지 재실행과 예약 인수(`native_guest_reservation.cpp`, `guest_process_entry.h`), 전용 게스트 스레드.
   *Secure 0x400000: the suspended relaunch and reservation hand-over, and a dedicated guest thread.*
5. `--image-dump`를 공용 러너에(`native_image_dump`, continuation 연결, CLI 요청).
   *`--image-dump` on the shared runner.*
6. 검증과 문서(README, ARCHITECTURE, 이미지 덤프 가이드), 작업 로그.
   *Verification, documents and the work log.*
