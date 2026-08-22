# 작업 지시: Windows native helper synthetic PE32와 IPC

## 목표

Windows x64 host와 Win32 x86 helper 사이에서 synthetic PE32 mapping, import event, guest memory read, import 완료 응답, process exit를 왕복 검증합니다.

## Goal

Validate synthetic PE32 mapping, import events, guest-memory reads, import completion, and process exit across a Windows x64 host and Win32 x86 helper.

## 범위

* `ExecutionBackend` guest memory read/write 경계
* Windows native helper protocol v1
* synthetic PE32 생성 x64 host probe
* PE32 mapping/IAT binding x86 helper
* x64/x86 통합 build·test script
* 관련 아키텍처와 TODO 갱신

## 제외 범위

원본 실행 파일, 일반 HLE API, relocation/TLS 실행, 병렬 guest thread, Linux/Web backend는 포함하지 않습니다.

## Verification

Run the Windows x64 warnings-as-errors suite, the Win32 native gate probe, and the new x64/x86 IPC integration probe.
