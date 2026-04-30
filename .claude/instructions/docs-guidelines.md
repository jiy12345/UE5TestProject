# 언리얼 엔진 개념 정리 가이드라인

> 이 문서는 엔진 소스 코드 분석 및 문서 작성 시 참조합니다.

## 정리 목적

언리얼 엔진의 다양한 개념과 시스템을 **엔진 소스 코드와 공식 문서에 기반하여** 깊이 있고 체계적으로 분석하고 정리합니다.

## 디렉토리 구조

**분석 문서는 `Docs/` 디렉토리** (저장소 루트)에 카테고리별로 정리합니다.

```
Docs/                          # 디렉토리명 컨벤션: PascalCase
├── Core/                      # 핵심 시스템
│   ├── ReflectionSystem/
│   ├── GarbageCollection/
│   ├── ObjectSystem/
│   ├── ModuleSystem/
│   └── Serialization/
│
├── Gameplay/                  # 게임플레이 프레임워크
│   ├── ActorLifecycle/
│   ├── GameFramework/
│   ├── ComponentSystem/
│   ├── InputSystem/
│   └── GameplayTags/
│
├── Rendering/                 # 렌더링 시스템
│   ├── RenderingPipeline/
│   ├── Materials/
│   ├── Lighting/
│   └── ShaderSystem/
│
├── UI/                        # UI 시스템
│   ├── Slate/
│   ├── UMG/
│   └── CommonUI/
│
├── Animation/                 # 애니메이션 시스템
│   ├── AnimationBlueprint/
│   ├── IKSystem/
│   └── ControlRig/
│
├── AI/                        # AI / 네비게이션 시스템
│   ├── RecastNavMesh/
│   ├── DetourCrowd/
│   ├── BehaviorTree/
│   ├── EnvironmentQuery/
│   └── PerceptionSystem/
│
├── Networking/                # 네트워킹
│   ├── Replication/
│   ├── RPC/
│   └── NetworkPrediction/
│
├── WorldManagement/           # World Partition, Data Layers
│
└── BuildSystem/               # 빌드 시스템
    ├── UnrealBuildTool/
    ├── UnrealHeaderTool/
    └── BuildConfiguration/
```

## 각 개념 폴더 구조 (템플릿)

각 개념은 다음과 같은 일관된 구조로 정리합니다:

```
<concept-name>/
├── README.md                      # 개요, 학습 목표, 목차
├── 01-overview.md                 # 개념 소개 및 전체 그림
├── 02-architecture.md             # 아키텍처 및 설계 철학
├── 03-core-classes.md             # 핵심 클래스 및 구조체
├── 04-source-analysis.md          # 엔진 소스 코드 심층 분석
├── 05-practical-guide.md          # 실전 활용 가이드 + 이해도 확인 실습 안내 (구현은 이슈 링크)
├── 06-best-practices.md           # 베스트 프랙티스 및 패턴
├── 07-common-pitfalls.md          # 흔한 실수 및 주의사항
└── references.md                  # 참고 자료 (공식 문서, 소스 파일 경로)
```

**파일은 필요에 따라 추가/생략 가능**하며, 개념의 복잡도에 맞게 조정합니다.

> **변경점**: 이전에 있던 `examples/` 폴더는 더 이상 쓰지 않습니다. 실습 코드는 별도 구현 이슈로 분리합니다. 자세한 규칙은 아래 **실습 원칙** 참고.

## 소스 참조 원칙

**모든 섹션/개념에는 해당 내용을 직접 확인할 수 있는 엔진 소스 위치를 명시합니다.**

각 섹션 하단에 다음 형식의 인용 블록을 추가합니다:

```markdown
> **소스 확인 위치**
> - `파일경로:라인번호` — 설명 (어떤 내용을 볼 수 있는지)
> - `파일경로` — 클래스/함수명 및 역할
```

**원칙:**
- 단순 파일 나열이 아닌, **"이 개념을 이해하려면 여기를 봐라"** 형태로 안내
- 라인 번호는 가능한 범위로 명시 (예: `라인 528`, `약 라인 1800-1815`)
- 개요(01-overview.md) 포함 **모든 파일**에 적용 — 개요에서도 각 섹션마다 소스 위치 안내
- 함수/클래스 단위로 최대한 구체적으로 지정

**예시 (좋음):**
```markdown
> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h:528` — `ERuntimeGenerationType` 열거형 정의
> - `RecastNavMeshGenerator.cpp:5105` — `IsGameStaticNavMesh()` — DynamicModifiersOnly 분기 핵심 로직
```

**예시 (나쁨):**
```markdown
> 참고: NavigationSystem 폴더 참조
```

## 실습 원칙

**분석 문서와 실습 구현을 분리합니다.** 이해도 확인 실습은 **별도 이슈로 분리**하고, 분석 문서에는 **실습 목표와 검증 체크리스트만** 남깁니다.

### 분리 이유

- 분석 문서의 가독성/포커스 향상 (개념 설명만)
- 실습 구현 진행 상황을 이슈로 추적 가능
- 여러 실습을 독립적으로 진행·리뷰 가능
- 실습 코드가 진화해도 분석 문서는 안정적으로 유지

### 분석 문서(`05-practical-guide.md` 등)에 들어갈 것

1. **실습 목표**: 분석 내용을 어떤 방식으로 눈으로 확인할지 개념적으로 설명
2. **구현 이슈 링크**: 해당 실습의 구현을 담은 GitHub 이슈 URL
3. **동작 검증 체크리스트**: PIE에서 실행해서 확인할 수 있는 항목들 (PATHPOINTS 확인, Volume 우회 확인 등)
4. **엔진 내장 도구 활용**: 콘솔 명령, GameplayDebugger 등 커스텀 코드 없이 쓸 수 있는 것들

### 구현 이슈에 들어갈 것

1. **배경**: 어떤 분석 이슈와 연결되는지
2. **목표**: 구현할 컴포넌트/액터가 무엇을 시각화/로그하는지
3. **구현 코드**: 헤더/cpp 전체 (복사해서 프로젝트에 추가 가능하도록)
4. **동작 검증 체크리스트**: 분석 문서와 동일한 항목 (실행 관점)
5. **관련 문서 링크**: 분석 문서 경로

### 이슈는 실습 단위로 분리

실습 컴포넌트/액터 하나당 이슈 하나. "마스터 이슈 + 서브 이슈" 같은 묶음은 특별한 이유가 없으면 지양 — 각각이 독립적으로 완료 가능하도록.

**예시:**
- NavMesh 분석(#10) → RecastNavMesh 디버그 시각화(#19)
- Pathfinding 분석(#16) → Pathfinding 디버그 시각화(#18)
- MVVM 분석 → PlayerStats ViewModel 구현(#3)

## 정리 원칙

### 1. 소스 코드 우선 (Source-First Approach)
- **엔진 소스 코드가 최우선 참고 자료**
- 공식 문서가 불충분하거나 불명확할 때는 반드시 소스 코드 직접 분석
- 소스 파일 경로, 클래스명, 함수명을 명확히 기록
- 예시:
  ```markdown
  ## FFieldNotificationId 구조체

  **파일**: `Engine/Source/Runtime/CoreUObject/Public/FieldNotification/FieldId.h`

  ```cpp
  struct FFieldNotificationId
  {
      FName FieldName;
      // ... (실제 소스 코드 발췌)
  };
  ```
  ```

### 2. 명확하고 자세한 설명
- **"어떻게 동작하는가"를 중심으로 설명**
- 단순 기능 나열이 아닌 **내부 메커니즘 이해에 집중**
- 복잡한 개념은 단계별로 나누어 설명
- 다이어그램, 시퀀스, 플로우차트 활용 (Mermaid 문법 사용)

### 3. 계층적 학습 경로
- **기초 → 중급 → 고급** 순서로 구성
- 선수 지식이 필요한 경우 명시하고 링크
  ```markdown
  > **선수 지식**: 이 문서를 이해하려면 먼저 [UObject 시스템](../core/object-system/)을 학습하세요.
  ```

### 4. 실용성과 이론의 균형
- 이론적 배경과 실제 사용 사례를 함께 제공
- "왜 이렇게 설계되었는가?" (설계 의도)
- "언제 사용해야 하는가?" (활용 시나리오)
- "어떻게 사용하는가?" (코드 예시)

### 5. 지속적 업데이트
- 엔진 버전 업그레이드 시 변경사항 추적
- 문서 상단에 **작성/수정 날짜 및 엔진 버전** 명시
  ```markdown
  > **작성일**: 2024-12-10
  > **엔진 버전**: UE 5.7
  > **최종 수정**: 2024-12-15
  ```

## 작성 프로세스

### Phase 1: 조사 및 계획
1. **공식 문서 검토**: Unreal Engine 공식 문서 확인
2. **소스 코드 탐색**: 관련 헤더 파일 및 구현 파일 찾기
3. **범위 정의**: 다룰 내용과 제외할 내용 결정
4. **목차 구성**: README.md 작성 및 파일 구조 결정

### Phase 2: 소스 코드 분석
1. **핵심 클래스 파악**: 주요 클래스, 구조체, 인터페이스 식별
2. **흐름 추적**: 함수 호출 흐름, 생명주기, 이벤트 처리 분석
3. **주석 및 메모**: 소스 코드 내 주석과 문서화 주석 활용
4. **테스트 코드 확인**: 엔진의 테스트 코드에서 사용 예시 추출

### Phase 3: 문서 작성
1. **개요 작성** (`01-overview.md`): 큰 그림 그리기
2. **아키텍처 분석** (`02-architecture.md`): 설계 구조 설명
3. **소스 분석** (`04-source-analysis.md`): 코드 단위 분석
4. **실용 가이드** (`05-practical-guide.md`): 실제 구현 예시
5. **참고 자료 정리** (`references.md`): 모든 참고 자료 링크

### Phase 4: 검증 및 개선
1. **실제 구현**: 분석한 내용을 바탕으로 테스트 코드 작성
2. **동작 확인**: 이론과 실제가 일치하는지 검증
3. **문서 보완**: 누락된 내용 추가, 오류 수정
4. **예시 추가**: 실제 동작하는 코드 예시 추가

## 참고 자료 우선순위

1. **엔진 소스 코드** (최우선)
   - `Engine/Source/Runtime/`
   - `Engine/Source/Editor/`
   - `Engine/Plugins/`

2. **공식 문서**
   - Unreal Engine Documentation
   - API Reference
   - Release Notes

3. **엔진 샘플 및 템플릿**
   - `Engine/Samples/`
   - Content Examples
   - 프로젝트 템플릿

4. **학술 자료 및 논문**
   - **그래픽스/렌더링 관련**: SIGGRAPH, EGSR, I3D, GPU Gems/Pro/Zen, Real-Time Rendering
   - **컴퓨터 과학/프로그래밍 관련**: 게임 엔진 아키텍처, 메모리 관리, 멀티스레딩, 최적화
   - **게임 개발 관련 서적**: Game Engine Architecture (Jason Gregory), Game Programming Patterns (Robert Nystrom)

5. **커뮤니티 자료** (보조적 참고)
   - Unreal Engine Forums
   - AnswerHub (archived)
   - 개발자 블로그 및 기술 아티클

## 문서 품질 기준

**좋은 문서:**
- 소스 코드 파일 경로와 라인 번호 명시
- 실제 코드 스니펫 포함 (복사-붙여넣기 가능)
- 동작 흐름을 시각화 (다이어그램)
- "왜"에 대한 답변 포함
- 실제 테스트한 예시 코드
- 관련 논문이나 학술 자료 인용 시 정확한 출처 명시

**피해야 할 문서:**
- 공식 문서 단순 번역/요약
- 근거 없는 추측이나 의견
- 소스 파일 참조 없이 "아마도...", "일반적으로..." 같은 표현
- 엔진 버전 명시 없음
- 복사한 코드가 실제 동작하지 않음

## 예시: 잘 작성된 분석 문서

`Docs/UI/MVVM/` 폴더를 참고하세요. 이 폴더는 다음을 잘 보여줍니다:
- 체계적인 문서 구조 (01~05 번호 체계)
- 소스 코드 기반 분석
- 명확한 설명과 코드 예시
- 실용적인 가이드
