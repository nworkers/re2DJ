# 첫 원본 덤프 정적 분석 설계

## 배경

원본 덤프가 확보되면 가장 먼저 답해야 할 질문은 "무엇을 실행해야 하는가"와 "무엇을 구현해야 하는가"다. 둘 다 실행 없이 정적 분석만으로 답할 수 있다.

## 접근

세 단계로 나눈다.

1. **덤프 유출 차단.** 분석보다 먼저 한다. 덤프는 수만 개 파일이므로 한 번 커밋되면 되돌리기 어렵다.
2. **실행 파일 식별.** 어떤 파일이 게임이고 어떤 것이 도구인지, 보호 계층이 있는지 없는지.
3. **import 표면 확정.** HLE 구현 범위를 사실로 고정한다.

## 왜 보호 여부가 먼저인가

보호된 실행 파일은 진입점이 언패킹 스텁 안에 있다. 그 스텁은 자기 자신을 풀어 원래 코드 섹션에 써 넣으므로 **자기 수정 코드**다. 인터프리터 backend가 자기 수정 코드를 정확히 처리하려면 명령어 캐시 무효화까지 다뤄야 하고, 이는 초기 bring-up에 넣기에 무겁다.

보호되지 않은 빌드가 하나라도 있으면 그것으로 Stage 2·3을 진행하고, 보호된 빌드는 backend가 성숙한 뒤로 미룰 수 있다. 그래서 섹션 배치와 진입점 위치를 먼저 본다.

## 왜 import 표면이 결정적인가

HLE 구현 범위를 정하는 방법은 두 가지다. 게임을 돌려 보며 실패하는 API를 하나씩 채우거나, import 테이블을 읽어 목록을 먼저 확정하거나.

전자는 실행 backend가 있어야 시작할 수 있다. 후자는 지금 당장 가능하고 결과가 완전하다. Win32 프로그램이 정적으로 링크한 API는 전부 import 테이블에 있기 때문이다.

예외는 `GetProcAddress`로 런타임에 가져오는 API다. 보호된 빌드의 스텁이 그렇게 한다. 그래서 보호되지 않은 빌드의 import 목록이 더 신뢰할 만하다.

## 기록 방식

분석 결과는 `docs/analysis/` 아래 주제별 문서에 두고, 결론만 `docs/EXE_DESIGN.*`에 요약한다. 모든 서술에 확인됨 / 추정 / 미확정을 표기하고, 확인됨에는 측정 방법을 함께 적는다.

`ARCHITECTURE.md`의 HLE 우선순위 표는 원래 일반적인 Win32 게임을 가정해 쓴 것이었다. import 목록이 확정되었으므로 근거 기반 표로 교체한다. 추측으로 쓴 표를 남겨 두면 나중에 사실처럼 인용된다.

## Background

Once an original dump is available, the first two questions are what to execute and what to implement. Both can be answered by static analysis alone.

## Approach

Three steps: block the dump from entering the repository, which comes before any analysis because tens of thousands of files are hard to undo once committed; identify the executables and whether each carries a protection layer; and fix the import surface.

## Why protection status comes first

A protected executable's entry point sits inside an unpacking stub that writes the original code back into its own sections, which makes it **self-modifying code**. An interpreter that handles that correctly must also handle instruction-cache invalidation, and that is heavy for initial bring-up. If even one unprotected build exists, Stages 2 and 3 can proceed on it while the protected builds wait for a mature backend, so section layout and entry-point placement are examined first.

## Why the import surface is decisive

There are two ways to scope the HLE: run the game and fill in each API as it fails, or read the import table and fix the list up front. The first needs an execution backend before it can start; the second is possible right now and is complete, because everything a Win32 program links statically appears in the import table. The exception is what `GetProcAddress` resolves at run time, which is exactly what a protection stub does — another reason the unprotected build's list is the more trustworthy one.

## How it is recorded

Findings go into per-topic documents under `docs/analysis/`, with only conclusions summarised in the `EXE_DESIGN` documents. Every statement is marked confirmed, inferred, or unresolved, and confirmed ones carry their measurement method.

The HLE priority table in `ARCHITECTURE.md` was originally written against a generic Win32 game. With the import list settled it is replaced by an evidence-based table, because a table written from guesswork would later be cited as fact.
