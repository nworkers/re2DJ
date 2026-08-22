# 작업 지시: Stage 2 PE 로더와 게스트 주소 공간

## 목표

`docs/TODO.md`의 Stage 2 항목을 구현하여 PE32 이미지가 공용 게스트 주소 공간에 안전하게 올라가고 import IAT가 합성 HLE gate를 가리키도록 합니다.

## Goal

Implement the Stage 2 items in `docs/TODO.md` so a PE32 image is safely loaded into the shared guest address space and its import IAT points at synthetic HLE gates.

## 작업 범위

* `re2dj::runtime::GuestAddress`와 host pointer 비노출 `AddressSpace` 추가
* PE32 섹션 매핑, zero-fill, `IMAGE_REL_BASED_HIGHLOW` 재배치 구현
* import descriptor/ILT 해석, 이름·ordinal gate 할당, IAT 기록 구현
* 비실행 검증 도구 `re2dj_pe_loader` 추가
* synthetic fixture 기반 단위 테스트 추가
* `ARCHITECTURE.md`, `docs/TODO.md`, 작업 로그 갱신

## Out of scope

* x86 명령어 해석기 또는 DBT
* 실제 Win32/DirectX API 구현
* 원본 실행 파일 또는 HDD 자산 추가
* 보호된 실행 파일의 보호 계층 제거

## 검증

구성 및 빌드, `re2dj_unit_tests`, CTest를 실행합니다. 실패 시 원인과 범위를 작업 로그에 기록합니다.

## Verification

Run configure/build, `re2dj_unit_tests`, and CTest. Record the cause and scope in the work log if verification fails.
