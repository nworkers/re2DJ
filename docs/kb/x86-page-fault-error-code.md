# x86 page-fault error code / x86 page-fault 오류 코드

x86 page fault의 error code는 fault가 발생한 접근의 성질을 비트로 나타냅니다. bit 0은 보호 위반이면 1, non-present page이면 0이고, bit 1은 write이면 1, read이면 0이며, bit 2는 user-mode 접근이면 1입니다. Linux i386 signal handler는 `ucontext_t`의 `REG_ERR`에서 이 raw 값을 읽을 수 있습니다.

*The x86 page-fault error code describes the faulting access in bits. Bit 0 is one for a protection violation and zero for a non-present page; bit 1 is one for a write and zero for a read; bit 2 is one for a user-mode access. A Linux i386 signal handler can read this raw value from `REG_ERR` in `ucontext_t`.*

따라서 error code `0x4`는 user-mode에서 non-present page를 읽으려 한 fault를 뜻합니다. 이 값만으로 guest가 왜 그 주소를 사용했는지는 알 수 없으므로, EIP·레지스터·bounded instruction window와 함께 해석해야 합니다.

*Therefore error code `0x4` means a user-mode read from a non-present page. The value alone does not reveal why the guest used that address, so interpret it together with EIP, registers, and a bounded instruction window.*

출처: [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), Volume 3A, “Interrupt 14—Page-Fault Exception (#PF)”.

*Source: [Intel® 64 and IA-32 Architectures Software Developer's Manual](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html), Volume 3A, “Interrupt 14—Page-Fault Exception (#PF)”.*