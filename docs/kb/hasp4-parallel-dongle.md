# HASP4 병렬포트 동글과 Win32 IOCTL 배경

## HASP4 API

Aladdin HASP4 Programmer's Guide는 stand-alone API를 `Service`, `SeedCode`, `PortNum`, 두 password, `Par1..Par4`로 정의한다. Service 2 `HaspCode`는 seed에 대해 `Par1..Par4` 네 개의 16비트 return code를 돌려주므로 응답 폭은 8바이트다. 자동 병렬포트 검색 순서는 `0x378`, `0x278`, `0x3bc`로 설명된다.

출처: [Aladdin HASP4 Programmer's Guide 사본](https://www.ti-soft.com/files/hasp_manual.pdf)

Hardlock은 같은 시기 Aladdin 제품이지만 별도 API/driver 계열이다. 공개 설치 자료에는 Windows 9x의 `hasp95.vxd`, `hasp95dl.vxd`, `hardlock.vxd`와 NT 계열의 `haspnt.sys`, `hardlock.sys`가 별도 파일로 제시된다. 따라서 제품명 HASP와 Hardlock을 같은 wire/API protocol로 간주하면 안 된다.

출처: [Aladdin driver package file inventory](https://www.usb-drivers.com/drivers/278/278550.htm), [Aladdin Hardlock Server manual](https://labvolt.festo.com/downloads/HLServer.pdf)

현대의 독립적인 HASP/Hardlock NTVDM 호환성 조사도 classic HASP 경로를 `\\.\HASP` → `HASPNT.SYS` → `\Device\FNT0` → `HARDLOCK.SYS`로 기술하고, DOS HASP call packet을 28바이트 구조로 설명한다. 이 자료는 BSD 호환 라이선스가 확인된 의존성이 아니므로 코드 도입 대상이 아니라 protocol shape를 대조하는 참고 자료로만 사용한다.

참고: [haspnt64 architecture notes](https://github.com/leecher1337/haspnt64)

## Win32 IOCTL 계약

`DeviceIoControl`의 `lpBytesReturned`는 output buffer에 저장된 실제 바이트 수를 받는다. `METHOD_BUFFERED` 요청은 kernel에서 하나의 system buffer를 input/output에 공유하지만, user-mode API에는 별도 input/output pointer와 크기가 보인다. 따라서 HLE가 `TRUE`를 반환하면서 bytes-returned를 0으로 두는 경우와 실제 output 길이를 쓰는 경우는 guest가 구분할 수 있다.

출처: [Microsoft DeviceIoControl](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol), [Microsoft IOCTL buffer descriptions](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/buffer-descriptions-for-i-o-control-codes)

## 적용 경계

HASP4의 공개 API shape는 특정 실행 파일의 동글 종류나 password/return code를 증명하지 않는다. 4바이트 input과 8바이트 output의 일치는 `HaspCode` 후보를 강화할 뿐이다. 실제 응답은 원본 바이너리 실행 관찰이나 합법적으로 보유한 장치의 캡처로 확인해야 한다.

---

# HASP4 Parallel-Dongle and Win32 IOCTL Background

## HASP4 API

The Aladdin HASP4 Programmer's Guide defines the stand-alone API in terms of Service, SeedCode, PortNum, two passwords, and Par1 through Par4. Service 2, HaspCode, returns four 16-bit codes for a seed, giving an eight-byte response. Its automatic parallel-port order is 0x378, 0x278, then 0x3bc.

Source: [copy of the Aladdin HASP4 Programmer's Guide](https://www.ti-soft.com/files/hasp_manual.pdf)

Hardlock was a contemporary Aladdin product but used a separate API and driver family. Public package inventories list `hasp95.vxd` / `hasp95dl.vxd` separately from `hardlock.vxd`, and `haspnt.sys` separately from `hardlock.sys`. HASP and Hardlock must therefore not be assumed to share one wire or API protocol.

Sources: [Aladdin driver package file inventory](https://www.usb-drivers.com/drivers/278/278550.htm), [Aladdin Hardlock Server manual](https://labvolt.festo.com/downloads/HLServer.pdf)

An independent modern compatibility study describes the classic HASP path as `\\.\HASP` through HASPNT.SYS and `\Device\FNT0` to HARDLOCK.SYS, with a 28-byte DOS HASP call packet. Its source is not adopted because a BSD-compatible dependency license was not established; only the documented protocol shape is used for comparison.

Reference: [haspnt64 architecture notes](https://github.com/leecher1337/haspnt64)

## Win32 IOCTL contract

DeviceIoControl writes the actual number of bytes stored in the output buffer to `lpBytesReturned`. METHOD_BUFFERED uses one kernel system buffer for input and output, while user mode still sees separate pointers and sizes. A guest can therefore distinguish TRUE with zero returned bytes from TRUE with the declared output length.

Sources: [Microsoft DeviceIoControl](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-deviceiocontrol), [Microsoft IOCTL buffer descriptions](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/buffer-descriptions-for-i-o-control-codes)

## Application boundary

The public HASP4 API shape does not prove a particular executable's dongle model, passwords, or return codes. The four-byte input/eight-byte output match only strengthens HaspCode as a candidate. Actual responses require evidence from the original binary or a legally owned device capture.
