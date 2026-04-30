# 실무 노트 (Practical Notes)

UE 모듈별 실무 노하우 / 함정 / 레시피. 개념 분석은 `Docs/AI/`, `Docs/MVVM/` 등 별도 디렉토리.

## 구조 원칙

- **최상위 .md 파일** — 모듈 무관 범용 인사이트 (Build.cs 패턴, DrawDebug 일반론 등)
- **모듈 폴더 (`<ModuleName>/`)** — 특정 UE 모듈 한정 인사이트. 폴더명은 `Engine/Source/Runtime/<ModuleName>/` 또는 플러그인 모듈의 `<ModuleName>` 그대로 PascalCase (Build.cs 문자열과 1:1)
- **이중 저장** — 범용+모듈 둘 다 해당하면, 최상위에 일반 원칙 / 모듈 폴더에 모듈 특수 케이스 (cross-link 필수, 코드 통째 복붙 X)

## 파일 내부 4섹션 템플릿

모든 .md 파일은 다음 구조를 따릅니다:

```markdown
# 주제명

## TL;DR
- 1줄짜리 핵심 룰 5~10개

## Recipes (이런 상황엔 이렇게)
### <상황>
[코드 + 주의점]

## Pitfalls (함정 — 증상 → 원인)
### 증상: <관찰 가능한 현상>
- 원인: <근본 원인>
- 확인: <어떻게 진단>

## References
- 출처 이슈/PR
- 엔진 소스 라인
- 개념 문서 링크
```

**섹션 헤더에 검색 키워드를 박는다** — `### 증상: 경로가 직선만 나옴` 같은 식. `grep -r "증상:" Docs/Practical/`로 함정 일람 가능.

## 최상위 (모듈 무관 범용)

- [BuildAndModule.md](BuildAndModule.md) — `Build.cs`, `PublicDependencyModuleNames` vs `PrivateDependencyModuleNames`, Unity Build, PCH
- [UEModuleSystem.md](UEModuleSystem.md) — UE 모듈 시스템이 존재하는 이유 (큰 그림)

## 모듈별

- [NavigationSystem/](NavigationSystem/) — 경로 쿼리, NavMesh 투사, NavModifier, NavData

## 검색 팁

- 증상으로 찾기: `grep -r "증상:" Docs/Practical/`
- 모르는 모듈: 해당 `.h`가 `Engine/Source/Runtime/<모듈>/Public/` 어느 모듈에 있는지 확인 후 그 모듈 폴더 열기
- TL;DR만 훑기: `grep -r "## TL;DR" -A 20 Docs/Practical/`
