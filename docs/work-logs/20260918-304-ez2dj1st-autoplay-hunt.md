# 작업 304 작업 로그 — ez2dj1st autoplay 탐색 / Task 304 work log — ez2dj1st autoplay hunt

작업 지시: [20260918-304-ez2dj1st-autoplay-hunt.md](../work-orders/20260918-304-ez2dj1st-autoplay-hunt.md)
분석: [1st Tracks 자동 플레이 장면](../analysis/ez2dj1st-demo-play.md)
절차: [`game-state-hunt` 스킬](../../.agents/skills/game-state-hunt/SKILL.md)

## 한국어

### 진행

| 단계 | 결과 |
| --- | --- |
| 1. 덤프 | 작업 294의 `resumed` 덤프, timestamp `0x3862fd9d`, gaps 없음 |
| 3. 데모 단서 | `DemoGame`, `ClubMixDemoGame`, `ShowDemoPlay`에 더해 **`DemoPlayer`, `ClubMixDemoPlayer`** |
| 3-1. 장면 표 | 등록 함수 `0x00424280`, 장면 53개. 1st SE에 없던 데모 전용 플레이어 장면 |
| 3-1. 초기화 콜백 | `DemoGame` 초기화에 상수를 쓰는 전역 없음. 곡 재생기 `0x0041af60`에 `push 1` |
| 재생기 호출처 | 7곳 중 데모 두 곳만 1, 나머지 0 |
| 재생기 내부 | 인자를 `[0x0055bc4c]`에 저장. 이 전역은 곡 진행 모듈만 읽음(참조 5곳) |
| 7. 읽기 폴링 | 어트랙트 200초 중 데모 구간 두 곳에서만 1 |
| 8. 쓰기 시험 | 사용자 동의 후 일반 곡 도중 1을 씀. 20초간 유지, **변화 없음** |
| 장면 비교 | `player`와 `DemoPlayer`의 매 프레임 처리가 멤버 함수를 각자 사본으로 가짐 |

### 판단

1st Tracks의 자동 연주는 변수가 아니라 데모 플레이어 장면 코드에 있다고 **추정**한다. `game_controls`에 선언하지 않았고 코드 변경은 없다. 사용자와 합의해 장면 교체·코드 패치는 진행하지 않았다.

### 스킬 개선

* `SKILL.md` 3-1에 "변수가 없는 경우" 판정과 결과 표 행을 추가했다.
* 이번에는 즉석 스크립트 두 개(피연산자 값 검색, 두 장면의 호출 집합 비교)를 썼다. 한 번 쓴 형태라 스킬 도구로는 올리지 않았다.

### 검증

* 코드 변경 없음(문서만).
* 쓰기 시험 전 값 0, 쓴 직후 1, 20초 폴링 동안 1 유지.

### 남은 것

* `DemoPlayer`가 노트를 치는 정확한 지점과 채널 구조체 값의 의미.

## English

### Progress

The dump was task 294's `resumed` dump (timestamp `0x3862fd9d`, no gaps). Besides `DemoGame`, `ClubMixDemoGame` and `ShowDemoPlay`, the clues included **`DemoPlayer` and `ClubMixDemoPlayer`**. The scene table from registration function `0x00424280` lists 53 scenes, including those demo-only player scenes absent from 1st SE. `DemoGame`'s init writes no constant to a global but pushes 1 into the chart player `0x0041af60`; of that function's seven callers only the two demo scenes pass 1. The player stores it in `[0x0055bc4c]`, read only inside the chart module (five references). A 200-second attract poll showed it at 1 only in the two demo spans. With the user's consent, 1 was written mid-song; it held for 20 seconds with **no visible change**. Comparing the per-frame updates of `player` and `DemoPlayer` showed each calling its own copies of the member functions.

### Verdict

Automatic play in 1st Tracks is **inferred** to live in the demo player scenes' code rather than a variable. Nothing was declared in `game_controls` and no code changed; by agreement with the user, scene swapping and code patching were not pursued.

### Skill improvements

* `SKILL.md` path 3-1 gained a "no variable" verdict and a results-table row.
* Two throwaway scripts (an operand-value search and a call-set comparison of two scenes) were used; being one-off, they were not added to the skill.

### Verification

* No code change (documents only).
* The value read 0 before the write, 1 right after, and stayed 1 through a 20-second poll.

### Remaining

* Exactly where `DemoPlayer` hits notes, and what the channel struct value means.
