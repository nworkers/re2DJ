# DrawPrimitive OpenGL backend 작업 지시

관련 설계: [DrawPrimitive OpenGL backend](../design/20260825-067-drawprimitive-opengl-backend.md)

## 상태

**완료**

## 작업 범위

1. 플랫폼 중립 TL vertex와 draw command/validation을 추가하고 단위 테스트한다.
2. Windows x86 facade에 확인된 DrawPrimitive slot, stage 0 texture 수명과 HWND 보존을 연결한다.
3. 전용 Windows WGL/OpenGL shader backend에서 RGB565 texture triangle strip을 draw한다.
4. Flip present를 연결한다.
5. 첫 통합 실행에서 확인된 `SetTextureStageState` null slot을 Get/Set 대칭 상태 보존으로 연결한다.
6. handoff 뒤 runtime debug marker도 진단 로그에 보존한다.
7. Windows x86 build, CTest와 canonical 2회로 AV 제거와 다음 경계를 확인한다.
8. 누적 문서와 작업 로그를 갱신하고 커밋한다.

## 완료 조건

- FVF `0x1c4`, triangle strip, 정점 4개 호출이 검증된 command로 변환된다.
- null DrawPrimitive vtable AV가 사라지고 backend draw 결과가 Flip에서 present된다.
- 지원하지 않는 입력은 결정적인 실패를 반환한다.
- 원본 자산은 읽기 전용으로 유지된다.

---

# DrawPrimitive OpenGL Backend Work Order

Related design: [DrawPrimitive OpenGL Backend](../design/20260825-067-drawprimitive-opengl-backend.md)

## Status and scope

**Complete.** Added and tested the platform-neutral transformed/lit draw command, connected the confirmed DrawPrimitive slot plus stage-zero texture and HWND lifetime, implemented a dedicated Windows WGL shader backend for the RGB565 textured triangle strip, present on Flip, retained the subsequently observed texture-stage state through symmetric Get/Set slots, and recorded two canonical runs with 201 successful draws and no OpenGL failure or access violation each.
