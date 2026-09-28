# 작업 406 설계 — 1st CRT 시작의 kernel32 함수 / Task 406 design — kernel32 functions of 1st's CRT startup

선행: [작업 405 설계](20260927-405-private-profile-int.md)

## 배경 / Background

작업 405 뒤 Linux의 EZ2DJ 1st는 원래 프로그램의 CRT 시작 코드에 들어가 `kernel32!InitializeCriticalSection`에서 멈췄다. CRT는 이어서 critical section, TLS, Interlocked, `IsBadReadPtr`를 쓴다.

*After Task 405, EZ2DJ 1st on Linux entered the original program's CRT startup and stopped at `kernel32!InitializeCriticalSection`. The CRT goes on to use critical sections, TLS, the Interlocked functions, and `IsBadReadPtr`.*

### 측정 / Measurements

Windows 11에서 32비트 프로그램으로 측정했다. 아래 모든 함수는 last error를 바꾸지 않는다. 예외로 적은 것만 바꾼다.

*Measured on Windows 11 with a 32-bit program. None of these functions changes the last error except where noted.*

| 함수 / Function | 결과 / Result |
| --- | --- |
| `InitializeCriticalSection` | DebugInfo −1, LockCount −1, 재귀 0, 소유자 0, 세마포어 0, SpinCount `0x020007D0` |
| `EnterCriticalSection` | 비어 있으면 LockCount −2, 재귀 1, 소유자 스레드 ID. 같은 스레드가 다시 들어가면 재귀만 증가 / *free: LockCount −2, count 1, owner the thread ID; the same thread again counts up* |
| `LeaveCriticalSection` | 재귀 감소, 0이 되면 LockCount −1·소유자 0 / *counts down; at 0 LockCount −1 and no owner* |
| `DeleteCriticalSection` | 모든 dword 0 / *every dword 0* |
| `TlsAlloc` | 가장 낮은 빈 인덱스, 처음은 1(0은 이미 쓰임), 해제한 인덱스 재사용 / *lowest free index, 1 first, freed indices reused* |
| `TlsGetValue` | TEB+0xE10 슬롯의 값, 64 미만이면 할당 여부와 무관, last error 0. 1088 이상은 0과 87 / *the TEB+0xE10 slot for any index below 64, last error 0; 1088 and up: 0 and 87* |
| `TlsSetValue` | 슬롯에 씀, TRUE. 1088 이상은 FALSE와 87 / *writes the slot, TRUE; 1088 and up: FALSE and 87* |
| `TlsFree` | 할당된 인덱스 TRUE, 아니면 FALSE와 87 / *TRUE for an allocated index, else FALSE and 87* |
| `InterlockedIncrement/Decrement` | 바뀐 값 / *the new value* |
| `GetCurrentThread` | `0xFFFFFFFE` |
| `IsBadReadPtr/IsBadWritePtr` | 크기 0은 0. 읽을 수 없거나(쓰기는 쓸 수 없거나) 하면 1 / *0 for a zero size; 1 when a byte cannot be read (or written)* |

## 결정 / Decisions

1. guest 스레드는 하나다. critical section은 측정한 필드 값을 guest 메모리에 그대로 쓴다. 다른 스레드가 가진 section에 들어가기, 가지지 않은 section에서 나가기는 멈춘다.
   *There is one guest thread. Critical sections carry the measured field values in guest memory; entering a section another thread owns, or leaving one this thread does not own, stops.*
2. TLS 할당 상태는 `GuestProcess`가 들고 값은 TEB 슬롯에 둔다. guest 코드가 슬롯을 직접 읽어도 같은 값을 본다. 64개의 TEB 슬롯을 넘는 확장 슬롯은 모델 밖이라 멈춘다.
   *`GuestProcess` keeps which TLS indices are allocated, and the values live in the TEB's slots, so guest code reading a slot directly sees the same value. Expansion slots past the TEB's 64 are unmodelled and stop.*
3. `IsBadReadPtr`는 guest 메모리 services가 읽을 수 있는지로 판단한다. `IsBadWritePtr`는 거기에 기록된 페이지 보호가 쓰기를 허용하는지를 더한다.
   *`IsBadReadPtr` is whether the guest memory services can read the range; `IsBadWritePtr` also needs any recorded page protection to allow writing.*
