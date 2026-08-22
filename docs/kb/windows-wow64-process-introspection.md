# Windows WOW64 Process Introspection

## 한국어

64비트 Windows host가 suspended 32비트 child의 주 이미지 주소를 확인할 때 `EnumProcessModulesEx`는 loader 상태에 따라 `ERROR_PARTIAL_COPY`를 반환할 수 있다. 이 프로젝트의 probe는 `NtQueryInformationProcess`의 `ProcessWow64Information`(26)으로 child의 WOW64 PEB 주소를 얻고, PEB32 prefix의 image-base field를 `ReadProcessMemory`로 읽는다.

Microsoft 문서는 `ProcessWow64Information`이 WOW64 process이면 0이 아닌 `ULONG_PTR`를 반환한다고 설명한다. `NtQueryInformationProcess`와 그 반환 구조는 장래에 바뀔 수 있는 내부 OS interface이므로, 이 방법은 suspended-process probe의 검증 보조 수단으로만 사용한다. 장기 backend ABI나 guest HLE 경계로 사용하지 않는다.

PEB32 image-base field의 offset 8은 이 probe의 live comparison으로만 확인했다. 다른 Windows 버전 또는 architecture에서 일반화된 계약으로 취급하지 않는다.

출처: [Microsoft Learn — NtQueryInformationProcess](https://learn.microsoft.com/ko-kr/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess)

## English

When a 64-bit Windows host checks the main-image address of a suspended 32-bit child, `EnumProcessModulesEx` can return `ERROR_PARTIAL_COPY` depending on loader state. This project's probe obtains the child WOW64 PEB address with `NtQueryInformationProcess` `ProcessWow64Information` (26), then reads the image-base field in the PEB32 prefix through `ReadProcessMemory`.

Microsoft documents that `ProcessWow64Information` returns a nonzero `ULONG_PTR` for a WOW64 process. Because `NtQueryInformationProcess` and its returned structures are internal OS interfaces that may change, this technique is used only as verification support in the suspended-process probe. It is not a long-term backend ABI or guest-HLE boundary.

The PEB32 image-base offset of 8 was confirmed only by this probe's live comparison. It is not treated as a generalized contract across Windows versions or architectures.

Source: [Microsoft Learn — NtQueryInformationProcess](https://learn.microsoft.com/en-us/windows/win32/api/winternl/nf-winternl-ntqueryinformationprocess)
