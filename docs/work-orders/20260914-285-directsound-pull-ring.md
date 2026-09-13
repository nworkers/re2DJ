# 작업 지시 / Work order

## 한국어

[작업 285 설계](../design/20260914-285-directsound-pull-ring.md)에 따라 streaming
DirectSound buffer의 snapshot/FIFO 보충을 pull 방식 shadow ring으로 교체합니다. SDL stream
callback과 guest Unlock 사이를 직렬화하고, 합성 오디오 회귀 테스트를 갱신한 뒤 Windows x86
Debug 빌드와 관련 테스트를 수행합니다. 실행 로그에서 확인된 결론은 JAM 누적 분석 문서와
아키텍처 문서에 반영합니다.

## English

Following [design 285](../design/20260914-285-directsound-pull-ring.md), replace snapshot/FIFO
refill for streaming DirectSound buffers with a pull-based shadow ring. Serialize the SDL
stream callback and guest Unlock, update the synthetic audio regression, run the Windows
x86 Debug build and relevant tests, and record the runtime conclusion in the cumulative
JAM analysis and architecture documents.
