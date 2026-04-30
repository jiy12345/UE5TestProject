# Git 워크플로우

> 이 문서는 Git 작업(커밋, 브랜치, PR) 시 참조합니다.

## 작업 프로세스 (필수)

**모든 작업은 반드시 다음 순서를 따릅니다:**

1. **GitHub Issue 생성**
   - 작업 시작 전 반드시 Issue 생성
   - Issue에 작업 내용, 목표, 체크리스트 작성
   - Issue 번호를 기억 (예: #4)

2. **Feature/Fix 브랜치 생성**
   - Issue 생성 후 즉시 브랜치 생성
   - 명명 규칙: `feature/#이슈번호-간단한-설명` 또는 `fix/#이슈번호-간단한-설명`
   - 예시:
     ```bash
     git checkout -b feature/#4-listview-mvvm-test
     git checkout -b fix/#5-binding-error
     ```

3. **작업 진행 및 커밋**
   - 브랜치에서 작업 수행
   - 커밋 메시지에 Issue 번호 포함 (예: `[#4]`)

4. **Main 브랜치로 병합**
   - 작업 완료 후 main으로 병합
   - Issue 종료

**절대 금지:**
- main 브랜치에서 직접 작업
- Issue 없이 브랜치 생성
- 브랜치 없이 커밋

**예외: `.claude/` 문서 수정**
- `.claude/` 디렉토리 내 문서(CLAUDE.md, instructions/ 등) 수정은 Issue 및 브랜치 없이 main에 직접 커밋/푸시
- 커밋 타입은 `docs(claude):` 사용
- 예시: `docs(claude): update git-workflow rules`

## 커밋 메시지 작성 규칙

**절대 포함하지 말 것:**
1. **Co-Authored-By 금지**: Claude와의 협업 표시 금지
2. **Claude Code 링크 금지**: 생성 도구 링크 추가 금지

**올바른 커밋 메시지:**
```bash
feat(mvvm): implement ListView ViewModel classes [#4]

Add ListItemViewModel and ListViewModel for ListView testing.
```

**잘못된 커밋 메시지:**
```bash
feat(mvvm): implement ListView ViewModel classes [#4]

Generated with [Claude Code](https://claude.com/claude-code)
Co-Authored-By: Claude <noreply@anthropic.com>
```

## 커밋 컨벤션

프로젝트는 **Conventional Commits** 기반의 커밋 컨벤션을 따릅니다.

### 커밋 메시지 형식

```
<type>(<scope>): <subject> [#issue]

[optional body]

[optional footer]
```

### Type (필수)
- `feat`: 새로운 기능 추가
- `fix`: 버그 수정
- `docs`: 문서 변경
- `style`: 코드 포맷팅, 세미콜론 누락 등 (기능 변경 없음)
- `refactor`: 코드 리팩토링 (기능 변경 없음)
- `perf`: 성능 개선
- `test`: 테스트 코드 추가/수정
- `chore`: 빌드 프로세스, 도구 설정 등
- `build`: 빌드 시스템, 의존성 변경
- `ci`: CI 설정 파일 변경
- `revert`: 이전 커밋 되돌리기

### Scope (선택)
변경 범위를 명시합니다:
- `mvvm`: MVVM 관련
- `ui`: UI/UMG 관련
- `gameplay`: 게임플레이 로직
- `engine`: 엔진 업그레이드/설정
- `build`: 빌드 시스템
- `docs`: 문서
- `config`: 설정 파일
- `ai`: AI/네비게이션 관련

### Issue 번호 (권장)
GitHub 이슈 번호를 `[#숫자]` 형식으로 포함합니다.

### 예시

```bash
# 기능 추가 (이슈 연결)
feat(mvvm): implement PlayerStatsViewModel class [#1]

# 버그 수정
fix(ui): resolve binding issue in health bar widget [#3]

# 문서 업데이트
docs(ai): add RecastNavMesh architecture analysis [#10]

# 엔진 업그레이드
build(engine): upgrade to Unreal Engine 5.7

# 리팩토링 (상세 설명 포함)
refactor(mvvm): simplify ViewModel property binding logic [#1]

Simplified the property binding mechanism by removing redundant
null checks and consolidating duplicate code paths.

# 되돌리기
revert: revert "feat(mvvm): implement PlayerStatsViewModel"

This reverts commit a1b2c3d due to compilation errors.
```

## 커밋 원칙

1. **커밋 분리**: 기능/변경사항별로 별도 커밋 작성
2. **명확한 메시지**: 무엇을, 왜 변경했는지 명확히 기술
3. **서브모듈 관리**: Engine 서브모듈 업데이트는 별도 커밋
4. **이슈 연동**: 관련 GitHub 이슈가 있으면 반드시 연결
5. **원자적 커밋**: 각 커밋은 독립적으로 빌드 가능해야 함

## 브랜치 전략

- `main`: 안정 버전 (빌드 성공 보장)
- `feature/<name>`: 기능 개발 브랜치
- `fix/<name>`: 버그 수정 브랜치
- `experiment/<name>`: 실험적 기능
