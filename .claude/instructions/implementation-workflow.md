# 구현 이슈 작업 워크플로우

> 이 문서는 GitHub Issue로 정의된 **구현 작업**(코드 작성)을 진행할 때 참조합니다.
> 핵심 목적은 **사용자의 학습**입니다. Claude가 한 번에 끝내버리면 사용자가 코드와 설계 의도를 익힐 시간이 없습니다.

## 핵심 원칙

**구현 이슈는 한 번에 끝내지 않는다.**

- Claude는 "구현 머신"이 아니라 사용자의 학습을 도와주는 페어 프로그래머다.
- 각 단계마다 **사용자가 코드와 설계를 납득**한 뒤에야 다음 단계로 넘어간다.
- "효율적으로 빨리 끝내기"가 아니라 "사용자가 충분히 이해하면서 진행하기"가 우선이다.

## 작업 프로세스 (필수)

### Phase 0 — 계획 공유

이슈 내용을 읽은 직후, **코드를 작성하기 전에** 다음을 사용자에게 제시합니다:

1. **이슈 요약**: 이 이슈가 무엇을 요구하는지 (1~2문단)
2. **세부 구현 단계 분할**: 전체 작업을 의미 있는 단위로 N개 단계로 쪼갠 목록
   - 각 단계는 **독립적으로 학습 포인트가 있는 단위**여야 함 (단순 줄 수 분할 X)
   - 각 단계마다 "이 단계에서 다룰 핵심 개념/문제" 한 줄 메모
3. **선행 질문**: 진행 방식, 우선순위, 알아야 할 컨텍스트 등 확인

**그리고 사용자의 OK를 받기 전까지 코드를 작성하지 않는다.**

### Phase 1 — 브랜치 + Draft PR 셋업 (계획 OK 직후 1회)

**전략 A: 단일 PR + 단계별 커밋 (Stacked Commits)**

1. `feature/#<이슈번호>-<짧은-설명>` 브랜치 생성
2. **첫 단계 커밋이 들어가면 Draft PR 즉시 생성** (이후 단계는 같은 브랜치에 추가 커밋만)
3. PR 본문에 단계 체크리스트 포함:

```markdown
> 이 PR은 학습 우선 워크플로우(.claude/instructions/implementation-workflow.md)에 따라
> 단계별 커밋으로 진행됩니다. 각 커밋이 곧 학습 단위입니다.

## 진행 상황
- [x] 1단계: <단계 제목>
- [ ] 2단계: <단계 제목>
- ...
- [ ] N단계: 빌드 검증 + 인사이트 누락 점검

각 단계 인사이트는 커밋 메시지 / 변경된 Docs/Practical 파일 참조.
Closes #<이슈번호>
```

4. 단계가 끝날 때마다 PR 본문 체크리스트 갱신 (`gh pr edit`).

**중요**: 단계별 커밋은 git-workflow의 "원자적 커밋(독립 빌드 가능)" 원칙에서 학습 우선 목적으로 일부 양보합니다.
- 가능한 한 각 커밋은 빌드 가능하도록 단계 순서를 짭니다 (예: 헤더 단독 X — 빈 cpp 같이 커밋).
- 빌드 검증 가능한 단계는 빌드 후 커밋, 검증 불가한 단계(설정만 변경 등)는 그 사실을 커밋 메시지에 명시.

### Phase 2 — 단계별 사이클 (각 단계마다 반복)

각 세부 단계마다 다음 사이클을 반복합니다:

1. **이번 단계 목표 재확인** (1~2줄)
2. **구현** (해당 단계 코드만, 다른 단계 선행 구현 금지)
3. **빌드 검증** (가능한 단계만 — UHT/UBT 결과 사용자에게 공유)
4. **문제 의식 공유** — 다음 항목을 능동적으로 제시:
   - 이 코드의 **설계 선택지**와 그중 왜 이걸 골랐는지
   - 사용한 **엔진 API의 동작 원리**나 주의점
   - **잠재적 문제**, 엣지 케이스, 향후 개선 여지
   - 사용자가 **놓칠 수 있는 함정**
5. **인사이트 후보 제시** (5분류 — §인사이트 저장 정책 참조)
6. **사용자 인사이트 검토** (OK / 수정 / 제외) — 사용자가 채팅으로 응답
7. **확정된 인사이트를 해당 단계의 코드와 함께 한 커밋에 묶음**:
   - `Docs/Practical/...` 신규/갱신
   - 기존 `Docs/AI/...` 등 개념 문서 보완
   - 코드 함정 한 줄 주석
   - PR 본문 체크리스트 갱신
8. **커밋 + 푸시 + (1단계라면 Draft PR 생성, 이후는 본문 갱신)**
9. **사용자에게 PR/커밋 링크 알리고 리뷰 대기** — Claude는 다음 단계 시작 X
10. **사용자 리뷰 채널**:
    - GitHub PR 인라인 코멘트 (`gh api repos/<owner>/<repo>/pulls/<num>/comments`로 확인)
    - 채팅 직접 피드백
    - 둘 다 OK — Claude는 양쪽 다 확인하고 응답
11. **사용자가 "다음 단계 진행" 등 명시적 OK 주면** 다음 단계 시작
12. **수정 요청 시**: 같은 브랜치에 **추가 커밋** (rebase/amend 금지 — 학습 흐름 보존)
13. **납득되지 않으면 머무름** — 사용자가 "이해 안 됨"이라고 하면 절대 다음 단계로 넘어가지 않음

### 단계별 커밋 메시지 형식

```
<type>(<scope>): [#<이슈번호> 단계 N] <단계 제목>

<설계/구현 요약 1~2문단>

인사이트:
- (실무·범용) <한 줄> → Docs/Practical/<X>.md
- (실무·모듈) <한 줄> → Docs/Practical/<Module>/<X>.md
- (개념) <한 줄> → Docs/AI/.../<X>.md §섹션
- (설계 결정) <한 줄> → 본 커밋 메시지/PR 본문
- (코드 함정) <한 줄> → <파일:라인> 주석
```

git log만 보고도 학습 흐름이 따라가지는 게 목표.

### Phase 3 — 마무리 단계 (마지막 단계)

마지막 단계는 코드뿐만 아니라 다음을 포함:

1. **전체 빌드 최종 검증** (UHT + UBT)
2. **인사이트 누락 점검** — 이전 단계들에서 놓친 인사이트가 있는지 한 번 더 훑어보고 추가/보완
3. **PR Draft → Ready for Review** 변경 (`gh pr ready`)
4. **PIE 등 사용자 검증이 필요한 항목은 체크리스트로 PR 본문에 남기고 사용자에게 위임**

## 인사이트 저장 정책

구현 중 얻은 인사이트는 4가지 분류로 나누어 적절한 위치에 저장합니다.

### 분류 → 저장 위치

| 인사이트 유형 | 저장 위치 | 형식 |
|---|---|---|
| **실무 인사이트 (모듈 무관 범용)** — DrawDebug, Build.cs 패턴, UPROPERTY 메타 등 | `Docs/Practical/<Topic>.md` (최상위) | 4섹션 템플릿 |
| **실무 인사이트 (특정 모듈 한정)** — `FPathFindingQuery`, `ProjectPointToNavigation` 등 | `Docs/Practical/<ModuleName>/<Topic>.md` | 4섹션 템플릿 |
| **개념 보완** — API 동작 원리, 자료구조 의미, 엔진 내부 메커니즘 | 기존 분석 문서 (`Docs/AI/...`, `Docs/MVVM/...`)에 인라인 보완 | 해당 문서 흐름 따라감 |
| **설계 결정 근거** — "왜 이 설계를 골랐나" | PR 본문 / 커밋 메시지 | 자유 |
| **코드 함정** — 코드 읽을 때 반드시 알아야 할 것 | 해당 라인에 한 줄 주석 (최소화 원칙 유지) | 1줄 |

### `Docs/Practical/` 디렉토리 구조

```
Docs/Practical/
├── README.md                          # 전체 인덱스
├── DebugVisualization.md              # 범용: DrawDebug 패턴
├── BuildAndModule.md                  # 범용: Build.cs 의존성, ModuleRules
├── NavigationSystem/                  # ← UE 엔진 모듈명 그대로 (PascalCase)
│   ├── README.md
│   ├── PathQuery.md
│   ├── NavMeshProjection.md
│   └── ...
├── Engine/
└── ModelViewViewModel/
```

**모듈명 규칙**: `Engine/Source/Runtime/<ModuleName>/` 또는 플러그인 모듈의 `<ModuleName>` **그대로 PascalCase**. Build.cs에 들어가는 문자열과 1:1 매칭.

| 폴더 | 엔진 경로 |
|---|---|
| `NavigationSystem/` | `Engine/Source/Runtime/NavigationSystem/` |
| `Engine/` | `Engine/Source/Runtime/Engine/` |
| `Core/` | `Engine/Source/Runtime/Core/` |
| `UMG/` | `Engine/Source/Runtime/UMG/` |
| `ModelViewViewModel/` | `Engine/Plugins/Runtime/ModelViewViewModel/Source/ModelViewViewModel/` |

### 범용 + 모듈 특화가 동시에 해당될 때 (이중 저장)

**원칙**: 범용 부분은 최상위, 모듈 특수 케이스는 모듈 폴더에 한 번 더.

- 최상위 (`Docs/Practical/<Topic>.md`) = 일반 원칙·범용 패턴 (원본)
- 모듈 폴더 (`Docs/Practical/<ModuleName>/<Topic>.md`) = "이 모듈에서 위 일반 원칙이 어떻게 적용되는가" — **차이점·특수 케이스에만 집중**, 일반 부분은 최상위 링크
- **같은 코드를 통째로 복붙 X** — 동기화 깨짐. cross-link 필수.

예: `DrawDebug` 일반 노하우는 `Docs/Practical/DebugVisualization.md`, NavMesh 경로/투사 시각화 시 색상·Z오프셋 관습은 `Docs/Practical/NavigationSystem/DebugVisualization.md`.

### 빈 폴더 사전 생성 X

첫 인사이트 추가 시점에 폴더와 README 함께 생성. 빈 자리잡기 X.

### 4섹션 템플릿 (`Docs/Practical/` 모든 .md 공통)

```markdown
# 주제명

## TL;DR
- 1줄짜리 핵심 룰 5~10개. "X 할 때는 Y" 형식.

## Recipes (이런 상황엔 이렇게)
### <상황 1>
[코드 스니펫 + 주의점]

### <상황 2>
...

## Pitfalls (함정 — 증상 → 원인)
### 증상: <관찰 가능한 현상>
- 원인: <근본 원인>
- 확인: <어떻게 진단>

## References
- 출처 이슈/PR: #N
- 엔진 소스: `Path/To/File.cpp:LINE`
- 개념 문서: `../../AI/.../XX.md` §섹션
```

**섹션 헤더에 검색 키워드를 박는다** — `### 증상: 경로가 직선만 나옴` 같은 식. grep "증상:"으로 함정 일람 가능.

### 운영 흐름 (단계별 커밋 방식)

각 세부 단계가 끝날 때 Claude가 다음 형식으로 인사이트 후보 제시:

```
[이번 단계 인사이트 후보]
1. (실무·범용) DrawDebugSphere segments 트레이드오프 → Docs/Practical/DebugVisualization.md
2. (실무·모듈) FPathFindingQuery Owner 인자 의미 → Docs/Practical/NavigationSystem/PathQuery.md
3. (개념) FindPathSync 내부 동작 → Docs/AI/RecastNavMesh/04-source-analysis.md §X 보완
4. (설계 결정) 왜 Tick 기반 시각화인가 → PR 본문
5. (코드 함정) NavData NULL 가능 타이밍 → 코드 라인 주석 1줄
```

사용자 OK / 수정 / 제외 → **확정된 인사이트는 해당 단계 커밋에 함께 포함** (단계 코드 + 인사이트 문서 갱신을 한 커밋으로 묶음 — 추적성 ↑). 마지막 단계에서 누락 점검 1회.

### 절대 금지

- **인사이트 분류 없이 다음 단계 진입** — 후보 제시는 매 단계 필수
- **같은 인사이트를 최상위/모듈 양쪽에 코드 통째 복붙** — cross-link로 처리
- **이슈 번호별/날짜별 폴더링** (예: `Practical/Issue19/`) — 시간 지나면 못 찾음. 항상 주제별
- **인사이트 문서 갱신을 별도 커밋으로 분리** — 단계 코드와 같은 커밋에 묶을 것 (추적성)
- **단계 코드 푸시 후 사용자 리뷰 없이 다음 단계 시작** — 명시적 OK 필수
- **수정 요청 시 rebase/amend로 이전 커밋 변경** — 학습 흐름 보존 위해 추가 커밋만

## 절대 금지 (워크플로우)

- **이슈 받자마자 전체 구현부터 시작하기** — 학습 기회 박탈
- **Phase 0(계획) 생략하고 브랜치/코드부터 시작** — 단계 합의 없이 진행 X
- **여러 단계를 한 번에 묶어서 구현하기** — 중간 리뷰 기회 박탈
- **"맞나요?" 없이 다음 단계로 진행** — 명시적 OK 필수
- **사용자가 모르는 설계 선택을 침묵으로 진행** — 능동적 공유 의무 위반
- **Draft PR 생략하고 마지막에 한 번에 PR 생성** — 단계별 리뷰 채널이 없어짐

## 예외

- **단순 수정** (오타, import 추가, 1~2줄 변경 등): 단계 분할 없이 바로 진행 가능. 단, "왜 이렇게 했는지"는 여전히 한 줄로라도 공유.
- **사용자가 명시적으로 "다 알아서 해줘", "한 번에 끝내줘"라고 요청한 경우**: 이 워크플로우 생략 가능. 단, 사용자의 명시적 요청이 있어야 함 — Claude가 임의 판단 금지.

## 단계 분할 가이드

좋은 단계 분할의 예 (이슈 #19 NavDebugVisualizer 기준):

1. **모듈 의존성 추가** — `NavigationSystem` 모듈은 왜 필요한가? Build.cs Public/Private 차이는?
2. **헤더 정의** — `UCLASS` 메타 옵션 의미, `UPROPERTY` 카테고리 활용, Tick 활성화
3. **경로 쿼리 시각화** — `FindPathSync` vs `FindPathAsync`, `FPathFindingQuery` 인자, 실패 케이스 처리
4. **NavMesh 투사 시각화** — `ProjectPointToNavigation` Extent의 의미, NodeRef는 무엇을 가리키는가
5. **마무리 — 전체 빌드 검증 + 인사이트 누락 점검 + Draft → Ready for Review**

나쁜 분할의 예:
- "헤더 작성", "cpp 작성" — 학습 단위가 아닌 파일 단위
- "100줄 작성", "그 다음 100줄" — 의미 없는 분할
- "다 작성하고 마지막에 한꺼번에 설명" — 단계 무시
