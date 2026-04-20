# 07. 이해도 확인 실습 — 길찾기 디버그 시각화

> **작성일**: 2026-04-16
> **엔진 버전**: UE 5.5
> **구현 이슈**: [#18 — Pathfinding 디버그 시각화 컴포넌트 구현](https://github.com/jiy12345/UE5TestProject/issues/18)

## 1. 실습 목표

분석한 길찾기 파이프라인을 게임 내에서 직접 눈으로 확인합니다:

- A* 탐색 결과인 **폴리곤 코리더**를 시각화
- 퍼널 알고리즘 결과인 **직선 경로(웨이포인트)**를 시각화
- **경로 추적 상태**(현재 세그먼트, 목표 웨이포인트)를 실시간 표시
- **경로 무효화/재탐색**이 발생하는 순간을 감지하여 로그 출력

## 2. 구현

`UPathfindingDebugComponent`를 AI 폰에 부착하여 매 프레임 경로 정보를 `DrawDebug`로 렌더링합니다.

**구현 코드, 상세 설계, 프로퍼티 옵션은 구현 이슈 [#18](https://github.com/jiy12345/UE5TestProject/issues/18)에서 다룹니다.**

이 문서는 "무엇을 어떻게 눈으로 확인할 것인가"라는 **개념적 목표**와 **검증 체크리스트**에 집중합니다. 구현이 완료되면 이슈 링크에서 코드를 받아 프로젝트에 추가해 사용하세요.

## 3. 동작 검증 체크리스트

### 3.1 A* 탐색 + 퍼널 알고리즘 검증

- [ ] AI 폰에 `UPathfindingDebugComponent` 부착
- [ ] PIE에서 AI에게 이동 명령 부여
- [ ] **파란색 포탈 에지**(폴리곤 코리더)가 시작→목표 사이에 표시되는지 확인
- [ ] **초록색 직선 경로**(웨이포인트)가 포탈 코리더 위에 겹쳐 표시되는지 확인
- [ ] 직선 경로의 웨이포인트 수가 코리더 폴리곤 수보다 적은지 확인 — 퍼널 알고리즘이 불필요한 꼭짓점을 제거한 결과 ([03-funnel-algorithm.md](03-funnel-algorithm.md))

### 3.2 경로 추적 상태 검증

- [ ] **노란색 선**이 AI 폰에서 현재 목표 웨이포인트까지 그려지는지 확인
- [ ] AI가 웨이포인트에 도달하면 Segment 인덱스가 증가하는지 확인
- [ ] 최종 목표 도달 시 시각화가 사라지는지 확인 ([05-path-following.md §5](05-path-following.md))

### 3.3 경로 무효화 검증

- [ ] AI 이동 중에 경로 상에 `NavModifierVolume`을 배치
- [ ] 로그에 `[PathfindingDebug] Path changed!` 메시지가 출력되는지 확인
- [ ] AI가 장애물을 우회하는 새 경로로 자동 전환하는지 확인 ([06-path-invalidation.md](06-path-invalidation.md))

### 3.4 부분 경로 검증

- [ ] NavMesh가 끊긴 지역(섬)으로 이동 명령 부여
- [ ] `Partial: Yes`가 표시되는지 확인
- [ ] AI가 도달 가능한 가장 가까운 지점까지만 이동하는지 확인 ([02-detour-a-star.md §3.3](02-detour-a-star.md))

### 3.5 영역 비용 검증

- [ ] `Nav Area` 클래스를 만들어 비용을 높게 설정 (예: `DefaultCost = 5.0`)
- [ ] 높은 비용 영역을 가로지르는 짧은 경로 vs 우회하는 긴 경로 중 어떤 것을 선택하는지 확인
- [ ] 비용이 충분히 높으면 우회 경로를 선택하는지 확인 ([04-ue-pathfinding-pipeline.md §6](04-ue-pathfinding-pipeline.md))

---

## 4. 엔진 내장 디버그 도구 활용

커스텀 컴포넌트 없이도 빠르게 쓸 수 있는 엔진 기본 도구들입니다. 디버깅 시 병행 활용하세요.

### 4.1 콘솔 명령

| 명령 | 효과 |
|------|------|
| `ai.nav.DisplayNavLinks 1` | NavLink 시각화 |
| `ai.nav.DrawNavMesh 1` | NavMesh 폴리곤 시각화 |
| `ai.nav.ShowPath 1` | AI 경로 시각화 |

### 4.2 GameplayDebugger

PIE에서 `'` (작은따옴표) 키를 누르면 GameplayDebugger가 활성화됩니다.
**NavMesh** 카테고리에서 다음을 확인할 수 있습니다:
- 현재 경로와 코리더 시각화
- PathFollowing 상태 표시
- NavMesh 타일 경계 표시

---

## 5. 심화 실습 — NavLink 기능 파고들기

위 실습(#18)이 "완성된 경로"를 보여주는 것이라면, **심화 실습은 NavLink를 A*의 간선으로 취급하는 동작, 비용 기반 라우팅, 링크 통과 완료 메커니즘 등을 직접 구성·관찰**하는 것이 목표입니다.

**구현 이슈**: [#22 — NavLink 기능 실험 컴포넌트](https://github.com/jiy12345/UE5TestProject/issues/22)

### 5.1 실습 목표 (6가지 실험 시나리오)

| 실험 | 관찰 대상 | 관련 문서 |
|------|---------|---------|
| **E1. 기본 NavLinkProxy** | 끊긴 플랫폼을 NavLink로 연결 시 A*가 링크를 간선으로 취급 | [05-path-following.md §NavLink](05-path-following.md) |
| **E2. 비용 기반 라우팅** | `UNavArea::DefaultCost` 차이로 두 NavLink 중 선택되는 것 | [02-detour-a-star.md §3.4 비용](02-detour-a-star.md) |
| **E3. Auto-complete 링크** | `OnLinkMoveStarted` 반환 true → PFC 자동 완료 감지 | [05-path-following.md §링크 완료](05-path-following.md) |
| **E4. Manual-complete 링크** | 반환 false → `FinishUsingCustomLink` 명시 호출 필요 | [05-path-following.md §링크 완료](05-path-following.md) |
| **E5. Runtime Enable/Disable** | `SetEnabled`/`SetSmartLinkEnabled`로 링크 활성화 토글 → 재탐색 | [06-path-invalidation.md](06-path-invalidation.md) |
| **E6. 방향성** | 일방향/양방향 링크의 경로 성립 여부 비교 | [05-path-following.md §NavLink](05-path-following.md) |

### 5.2 심화 실습이 추가로 답해주는 것

- NavLink가 **A* 그래프의 일반 간선**처럼 취급된다는 점 (포탈 폴리곤 + 링크 비용)
- 비용 차이가 크면 "거리가 더 짧은 링크"라도 **A*가 우회**한다는 것을 수치로 확인
- `OnLinkMoveStarted`의 true/false 선택이 **AI의 링크 체류 시간·애니메이션 타이밍**을 바꾸는 것
- 런타임에 링크를 껐을 때 PFC가 **경로 무효화 → 재탐색**하는 반응 속도
- `FNavLinkId`로 **어떤 링크를 언제 통과했는지** 추적하는 방법
