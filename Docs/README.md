# Unreal Engine Concepts Documentation

> **작성일**: 2024-12-10
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#5](https://github.com/jiy12345/UE5TestProject/issues/5)

## 개요

이 디렉토리는 언리얼 엔진의 다양한 시스템과 개념을 **엔진 소스 코드와 공식 문서에 기반하여** 깊이 있고 체계적으로 분석하고 문서화하는 공간입니다.

## 목표

- **엔진 내부 동작 메커니즘의 명확한 이해**
- **소스 코드 기반의 정확한 분석**
- **실용적인 가이드와 베스트 프랙티스 제공**
- **체계적이고 재사용 가능한 문서 구조 구축**

## 디렉토리 구조

```
Docs/
├── README.md                  # 본 파일 (전체 가이드라인)
│
├── Core/                      # 핵심 시스템
│   ├── ReflectionSystem/      # UE 리플렉션 시스템 (UCLASS, UPROPERTY 등)
│   ├── GarbageCollection/     # 가비지 컬렉션 및 메모리 관리
│   ├── ObjectSystem/          # UObject 시스템
│   ├── ModuleSystem/          # 모듈 시스템
│   └── Serialization/         # 직렬화 시스템
│
├── Gameplay/                  # 게임플레이 프레임워크
│   ├── ActorLifecycle/        # 액터 생명주기
│   ├── GameFramework/         # GameMode, GameState, PlayerController 등
│   ├── ComponentSystem/       # 컴포넌트 아키텍처
│   ├── InputSystem/           # Enhanced Input System
│   └── GameplayTags/          # Gameplay Tags & Abilities
│
├── Rendering/                 # 렌더링 시스템
│   ├── RenderingPipeline/     # 렌더링 파이프라인
│   ├── Materials/             # 머티리얼 시스템
│   ├── Lighting/              # 라이팅 시스템
│   └── ShaderSystem/          # 셰이더 컴파일 및 관리
│
├── UI/                        # UI 시스템
│   ├── Slate/                 # Slate UI 프레임워크
│   ├── UMG/                   # UMG (Unreal Motion Graphics)
│   ├── MVVM/                  # MVVM 패턴
│   └── CommonUI/              # Common UI 플러그인
│
├── Animation/                 # 애니메이션 시스템
│   ├── AnimationBlueprint/    # 애니메이션 블루프린트
│   ├── IKSystem/              # IK (Inverse Kinematics)
│   └── ControlRig/            # Control Rig
│
├── AI/                        # AI 시스템
│   ├── BehaviorTree/          # Behavior Tree
│   ├── EnvironmentQuery/      # EQS (Environment Query System)
│   └── PerceptionSystem/      # AI Perception
│
├── Networking/                # 네트워킹
│   ├── Replication/           # 리플리케이션 시스템
│   ├── RPC/                   # RPC (Remote Procedure Call)
│   └── NetworkPrediction/     # 네트워크 예측
│
└── BuildSystem/               # 빌드 시스템
    ├── UnrealBuildTool/       # UBT (Unreal Build Tool)
    ├── UnrealHeaderTool/      # UHT (Unreal Header Tool)
    └── BuildConfiguration/    # 빌드 설정 및 최적화
```

## 각 개념 폴더 구조 (템플릿)

각 개념은 다음과 같은 일관된 구조로 정리합니다:

```
<ConceptName>/
├── README.md                  # 개요, 학습 목표, 목차
├── 01-Overview.md             # 개념 소개 및 전체 그림
├── 02-Architecture.md         # 아키텍처 및 설계 철학
├── 03-CoreClasses.md          # 핵심 클래스 및 구조체
├── 04-SourceAnalysis.md       # 엔진 소스 코드 심층 분석
├── 05-PracticalGuide.md       # 실전 활용 가이드
├── 06-BestPractices.md        # 베스트 프랙티스 및 패턴
├── 07-CommonPitfalls.md       # 흔한 실수 및 주의사항
├── References.md              # 참고 자료 (공식 문서, 소스 파일 경로)
└── Examples/                  # 코드 예시 및 스니펫
    ├── Example01.cpp
    └── Example02.cpp
```

**파일은 필요에 따라 추가/생략 가능**하며, 개념의 복잡도에 맞게 조정합니다.

## 정리 원칙

### 1. 소스 코드 우선 (Source-First Approach)

- **엔진 소스 코드가 최우선 참고 자료**
- 공식 문서가 불충분하거나 불명확할 때는 반드시 소스 코드 직접 분석
- 소스 파일 경로, 클래스명, 함수명을 명확히 기록

**예시:**

```markdown
## FFieldNotificationId 구조체

**파일**: `Engine/Source/Runtime/CoreUObject/Public/FieldNotification/FieldId.h`

핵심 구조:
- FName으로 필드 식별
- 컴파일 타임에 생성되는 고유 ID
```

> **참고**: 소스 코드 발췌는 최소화하고, 동작 원리 설명에 집중합니다.

### 2. 명확하고 자세한 설명

- **"어떻게 동작하는가"를 중심으로 설명**
- 단순 기능 나열이 아닌 **내부 메커니즘 이해에 집중**
- 복잡한 개념은 단계별로 나누어 설명
- 다이어그램, 시퀀스, 플로우차트 활용 (Mermaid 문법 사용)

### 3. 계층적 학습 경로

- **기초 → 중급 → 고급** 순서로 구성
- 선수 지식이 필요한 경우 명시하고 링크

**예시:**

```markdown
> **선수 지식**: 이 문서를 이해하려면 먼저 [UObject 시스템](../Core/ObjectSystem/)을 학습하세요.
```

### 4. 실용성과 이론의 균형

- 이론적 배경과 실제 사용 사례를 함께 제공
- **"왜 이렇게 설계되었는가?"** (설계 의도)
- **"언제 사용해야 하는가?"** (활용 시나리오)
- **"어떻게 사용하는가?"** (코드 예시)

### 5. 지속적 업데이트

- 엔진 버전 업그레이드 시 변경사항 추적
- 문서 상단에 **작성/수정 날짜 및 엔진 버전** 명시

**예시:**

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

1. **개요 작성** (`01-Overview.md`): 큰 그림 그리기
2. **아키텍처 분석** (`02-Architecture.md`): 설계 구조 설명
3. **소스 분석** (`04-SourceAnalysis.md`): 코드 단위 분석
4. **실용 가이드** (`05-PracticalGuide.md`): 실제 구현 예시
5. **참고 자료 정리** (`References.md`): 모든 참고 자료 링크

### Phase 4: 검증 및 개선

1. **실제 구현**: 분석한 내용을 바탕으로 테스트 코드 작성
2. **동작 확인**: 이론과 실제가 일치하는지 검증
3. **문서 보완**: 누락된 내용 추가, 오류 수정
4. **예시 추가**: 실제 동작하는 코드 예시 추가

## 참고 자료 우선순위

### 1. 엔진 소스 코드 (최우선)

- `Engine/Source/Runtime/`
- `Engine/Source/Editor/`
- `Engine/Plugins/`

### 2. 공식 문서

- [Unreal Engine Documentation](https://docs.unrealengine.com/)
- [API Reference](https://docs.unrealengine.com/5.7/en-US/API/)
- Release Notes

### 3. 엔진 샘플 및 템플릿

- `Engine/Samples/`
- Content Examples
- 프로젝트 템플릿

### 4. 학술 자료 및 논문

#### 그래픽스/렌더링 관련

- SIGGRAPH, EGSR, I3D 등의 컨퍼런스 논문
- GPU Gems, GPU Pro, GPU Zen 시리즈
- Real-Time Rendering 서적

#### 컴퓨터 과학/프로그래밍 관련

- 게임 엔진 아키텍처 관련 논문
- 메모리 관리, 멀티스레딩, 최적화 기법
- 데이터 구조 및 알고리즘 연구

#### 게임 개발 관련 서적

- Game Engine Architecture (Jason Gregory)
- Game Programming Patterns (Robert Nystrom)

### 5. 커뮤니티 자료 (보조적 참고)

- Unreal Engine Forums
- AnswerHub (archived)
- 개발자 블로그 및 기술 아티클

## 문서 품질 기준

### ✅ 좋은 문서

- 소스 코드 파일 경로 명시 (독자가 직접 확인 가능)
- 동작 원리를 명확하게 설명
- 동작 흐름을 시각화 (다이어그램)
- "왜"에 대한 답변 포함
- 실제 테스트한 예시 코드
- 관련 논문이나 학술 자료 인용 시 정확한 출처 명시

### ❌ 피해야 할 문서

- 공식 문서 단순 번역/요약
- 근거 없는 추측이나 의견
- 소스 파일 참조 없이 "아마도...", "일반적으로..." 같은 표현
- 엔진 버전 명시 없음
- 복사한 코드가 실제 동작하지 않음
- 엔진 소스 코드의 과도한 복사/붙여넣기

## 기여 가이드

이 문서는 Git으로 버전 관리되며, 다음 워크플로우를 따릅니다:

1. **GitHub Issue 생성**: 작업 시작 전 반드시 Issue 생성
2. **Feature 브랜치 생성**: `feature/#이슈번호-개념명` 형식
3. **작업 및 커밋**: 커밋 메시지에 Issue 번호 포함
4. **Pull Request**: 작업 완료 후 PR 생성 및 리뷰

자세한 Git 워크플로우는 프로젝트 루트의 `.claude/CLAUDE.md`를 참조하세요.

## 진행 상황

### 완료된 개념

- 🚧 작업 예정

### 진행 중인 개념

- 🚧 준비 단계

### 계획된 개념

- [ ] UObject 시스템
- [ ] 리플렉션 시스템
- [ ] 가비지 컬렉션
- [ ] 액터 생명주기
- [ ] 렌더링 파이프라인
- [ ] MVVM 패턴
- 기타 (이슈 #5 참조)

## 법적 고지

이 문서는 **학습 및 참고 목적**으로 작성되었습니다.

- 언리얼 엔진 소스 코드는 Epic Games, Inc.의 소유이며 [Unreal Engine EULA](https://www.unrealengine.com/en-US/eula/unreal)를 따릅니다
- 이 문서는 엔진 동작 원리에 대한 분석과 설명을 제공하며, 소스 코드 자체를 재배포하지 않습니다
- 문서에 포함된 코드 예시는 교육 목적의 fair use 범위 내에서 제공됩니다
- 독자는 언리얼 엔진을 사용하기 위해 Epic Games의 라이선스 조건을 준수해야 합니다
