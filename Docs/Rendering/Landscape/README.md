# Landscape — 지형 시스템

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7
> **분석 이슈**: [#21](https://github.com/jiy12345/UE5TestProject/issues/21)

언리얼 엔진의 **Landscape(지형) 시스템** 전체 아키텍처를 심층 분석합니다.

## 학습 목표

이 문서들을 모두 읽으면 다음 질문에 답할 수 있습니다:

- Landscape가 **StaticMesh로 해결되지 않는 어떤 문제**를 해결하기 위해 별도 시스템으로 존재하는가?
- `ALandscape`, `ALandscapeProxy`, `ALandscapeStreamingProxy`는 각각 무엇을 담고 있고 왜 3개로 분리되어 있는가?
- **Heightmap/Weightmap 텍스처**에 고도/레이어 가중치가 어떻게 패킹되어 있는가? (16비트 높이가 왜 RG 채널인가)
- **Edit Layers + BatchedMerge** 파이프라인(5.7 기본)이 왜 GPU에서 머지하는 구조로 갔는가?
- **World Partition**과 맞물려 스트리밍 프록시가 어떻게 생성·등록되는가?
- 런타임 렌더링에서 **Nanite 경로와 클래식 LOD 경로**는 어떻게 분기되는가?
- **Heightfield Collision**이 렌더 메시보다 저해상도로 만들어지는 이유와 교체 트리거는?

## 선수 지식

- UE의 Actor/Component 모델 (`AActor`, `UActorComponent`, `USceneComponent`) — `Docs/Gameplay/`(미작성)
- World Partition 기본 개념 — [Docs/WorldManagement/WorldPartition/](../../WorldManagement/WorldPartition/README.md) 참고
- 기본 렌더 파이프라인 (SceneProxy, 드로잉 스레드, RT 텍스처) — 외부 공식 문서
- 서브시스템(`UWorldSubsystem`) 수명주기

## 다루는 범위와 제외하는 범위

**이 문서에서 다루는 것**
- Landscape의 기본 액터/컴포넌트 계층
- Heightmap/Weightmap 데이터 레이아웃
- Edit Layers 시스템 및 BatchedMerge 렌더 파이프라인
- 렌더 프록시, LOD, Nanite 통합
- World Partition 스트리밍
- Heightfield/Mesh Collision과 물리 재질

**이 문서에서 다루지 않는 것** (별도 딥다이브 이슈로 분리)
- Landscape Grass (풀/식생 자동 배치)
- Landscape Splines (지형 위 경로)
- Landscape Editor Mode 내부 (스컬프트/페인트 도구)
- 블루프린트 브러시(Blueprint Brush) 상세
- Runtime Virtual Texture와의 연동

## 목차

| 파일 | 내용 | 읽는 순서 |
|------|------|----------|
| [01-overview.md](01-overview.md) | Landscape가 왜 필요한가, StaticMesh와의 차이, 전체 데이터 플로우 | 1 |
| [02-architecture.md](02-architecture.md) | `ALandscape` / `ALandscapeProxy` / `ALandscapeStreamingProxy` 계층, `ULandscapeInfo` 레지스트리 | 2 |
| [03-core-classes.md](03-core-classes.md) | 주요 클래스/구조체 레퍼런스 (`ULandscapeComponent`, `ULandscapeSubsystem` 등) | 3 |
| [04-heightmap-weightmap.md](04-heightmap-weightmap.md) | 텍스처 레이아웃, 섹션/서브섹션, `FWeightmapLayerAllocationInfo` | 4 |
| [05-edit-layers.md](05-edit-layers.md) | Edit Layers + BatchedMerge 파이프라인, GPU 머지 흐름 | 5 |
| [06-rendering-pipeline.md](06-rendering-pipeline.md) | `FLandscapeComponentSceneProxy`, LOD 선택, Nanite 분기 | 6 |
| [07-streaming-wp.md](07-streaming-wp.md) | World Partition 스트리밍, `ALandscapeStreamingProxy` 생성 | 7 |
| [08-collision-physics.md](08-collision-physics.md) | Heightfield vs Mesh Collision, `ULandscapePhysicalMaterial` | 8 |
| [references.md](references.md) | 소스 파일 경로 총정리, 공식 문서, 참고 논문 | 보조 |

## 작성 원칙

모든 섹션은 다음을 지킵니다 (프로젝트 `docs-guidelines` 기반):

1. **소스 확인 위치** 블록 필수 — 파일경로:라인번호 형식
2. 설명은 "**왜 이렇게 설계되었는가**"에 집중, 단순 기능 나열 지양
3. 실제 코드 스니펫은 복사-붙여넣기 가능하게
4. 다이어그램은 Mermaid 또는 ASCII 아트
5. 실습 코드는 본문에 포함하지 않고 별도 구현 이슈로 분리

## 주요 소스 디렉토리

- `Engine/Source/Runtime/Landscape/Public/` — Public 헤더
- `Engine/Source/Runtime/Landscape/Classes/` — UHT가 처리하는 UObject 클래스 헤더
- `Engine/Source/Runtime/Landscape/Private/` — 구현
- `Engine/Source/Runtime/Landscape/Internal/` — 모듈 내부 공유 헤더
- `Engine/Source/Editor/LandscapeEditor/` — 에디터 전용 (참고용)
