# UE5TestProject

언리얼 엔진 5 테스트 및 실험용 프로젝트.
언리얼 엔진의 새로운 기능을 테스트하고 소스코드를 분석하기 위한 실험용 프로젝트입니다.

## 프로젝트 구조

```
UE5TestProject/              # Git 저장소 루트
├── Engine/                  # 언리얼 엔진 소스 (Git 서브모듈, 브랜치: 5.7)
├── Game/                    # 게임 프로젝트 (Source, Content, Config, Plugins)
├── Docs/                    # 엔진 분석 문서 (PascalCase 컨벤션)
└── .claude/                 # Claude Code 설정 및 가이드라인
    └── instructions/        # 작업별 상세 가이드라인
```

## 언리얼 엔진 서브모듈

- Git 서브모듈로 관리
- 브랜치: 5.7
- 저장소: https://github.com/EpicGames/UnrealEngine

## 작업별 참조 가이드

**아래 문서들은 해당 작업을 수행할 때만 읽으세요.**

### 엔진 소스 분석 / 문서 작성
→ `.claude/instructions/docs-guidelines.md` 읽기

분석 문서 구조, 소스 참조 원칙, 실습 원칙, 품질 기준 등.

### Git 작업 (커밋, 브랜치, PR 생성)
→ `.claude/instructions/git-workflow.md` 읽기

커밋 컨벤션, 브랜치 전략, 이슈 연동 규칙 등.

**핵심 규칙 (항상 적용):**
- Co-Authored-By, Claude Code 링크 금지
- main 브랜치 직접 작업 금지 (단, `.claude/` 문서 수정은 main에 직접 푸시)
- Issue 없이 브랜치 생성 금지 (단, `.claude/` 문서 수정은 예외)

### PR 리뷰 코멘트 대응
→ `.claude/instructions/pr-review-workflow.md` 읽기

PR에 달린 인라인 리뷰 코멘트를 확인하고, 문서를 보강하여 추가 커밋하는 프로세스.

### 구현 이슈 (코드 작성) 작업
→ `.claude/instructions/implementation-workflow.md` 읽기

**핵심: 학습이 우선이다. 한 번에 끝내지 않는다.**
1. 이슈 받으면 → 먼저 단계 분할 계획 공유 → 사용자 OK 후 진행
2. 각 단계마다 → 구현 + 설계/문제 의식 공유 → 사용자 납득 확인 → 다음 단계
3. 사용자 명시 요청("한 번에 해줘") 없이 일괄 구현 금지
