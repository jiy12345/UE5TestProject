# PR 리뷰 기반 협업 프로세스

> 이 문서는 PR 리뷰 코멘트에 대응하여 문서를 보강할 때 참조합니다.

## 개요

엔진 소스 분석 문서 작성은 **PR 리뷰 기반 반복 개선** 방식으로 진행합니다.
사용자가 PR에 인라인 리뷰 코멘트로 추가 설명이 필요한 부분을 표시하면, Claude가 해당 부분을 보강하여 추가 커밋합니다.

## 프로세스

```
1. Claude: 엔진 소스 코드 분석 → Docs/ 문서 작성
2. Claude: feature 브랜치에서 커밋 → PR 생성 (또는 기존 PR에 push)
3. 사용자: PR 인라인 리뷰 코멘트로 추가 설명 필요한 부분 표시
4. Claude: 리뷰 코멘트 확인 → 소스 코드 재분석 → 문서 보강 → 추가 커밋
5. 반복 (3~4) → 만족스러우면 merge
```

## 리뷰 코멘트 확인 방법

PR의 인라인 리뷰 코멘트를 확인할 때는 다음 명령어를 사용합니다:

```bash
# 전체 리뷰 코멘트 목록 (파일 경로 + 코멘트 내용)
gh api repos/{owner}/{repo}/pulls/{pr_number}/comments \
  --jq '.[] | "---\nfile: \(.path)\nline: \(.original_line)\nbody: \(.body)\n"'
```

## 리뷰 대응 원칙

1. **코멘트별 대응**: 각 리뷰 코멘트에 대해 해당 문서의 관련 섹션을 보강
2. **소스 코드 재확인**: 추가 설명 요청 시 반드시 엔진 소스 코드를 다시 확인하여 정확한 정보 제공
3. **코드 기반 설명**: "더 구체적으로" 요청 시 실제 엔진 코드 스니펫과 함께 설명
4. **커밋 단위**: 리뷰 대응은 논리적 단위로 커밋 분리 (파일별 또는 주제별)
5. **커밋 메시지**: 어떤 리뷰에 대응하는 커밋인지 명확히 기술
   ```bash
   docs(ai): add dtNavMesh/dtNavMeshQuery details to architecture [#10]

   Address PR review: explain core Detour classes and tile management.
   ```

## 리뷰 코멘트 답글

문서 보강 후, **각 리뷰 코멘트에 답글을 달아야 합니다.** 답글에는 다음 두 가지를 포함합니다:

1. **질문에 대한 직접적인 답변**: 코멘트가 묻는 내용에 대한 명확한 설명
2. **문서 보강 내역**: 어떤 파일의 어떤 섹션을 보강했는지 안내

### 답글 달기 명령어

```bash
# 특정 리뷰 코멘트에 답글 달기
gh api repos/{owner}/{repo}/pulls/{pr_number}/comments/{comment_id}/replies \
  -f body="답변 내용"
```

### 답글 형식

```markdown
**답변**: (질문에 대한 핵심 답변을 간결하게)

**문서 반영**: `파일경로` — (보강한 내용 요약)
```

### 예시

리뷰 코멘트: "ANavigationData가 따로 분리되어 있어야 할 이유가 있는건지?"

답글:
```
**답변**: ANavigationData는 내비게이션 데이터의 추상 인터페이스로, Recast 외에도 다른 내비메시
구현체(예: AbstractNavData)를 플러그인 방식으로 교체할 수 있도록 분리되어 있습니다.
UNavigationSystemV1::NavDataRegisteredClasses에 등록된 클래스 목록으로 관리됩니다.

**문서 반영**: `02-architecture.md` 섹션 1 — ANavigationData 분리 이유 및 설계 의도 추가
```
