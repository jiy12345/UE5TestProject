# References — 참고 자료

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7

이 문서는 Landscape 시스템 분석에 사용된 소스 파일 경로와 외부 참고 자료를 모아둔 인덱스입니다.

## 1. 엔진 소스 파일

### 1.1 Runtime/Landscape 모듈

**경로 기반**: `Engine/Source/Runtime/Landscape/`

#### Classes/ (UObject 헤더)

| 파일 | 주요 내용 |
|------|---------|
| `Landscape.h` | `ALandscape` 마스터 액터, `FLandscapeLayer`, `FLandscapeLayerBrush` |
| `LandscapeProxy.h` | `ALandscapeProxy` 공통 기반 (매우 큰 헤더, 1500+ 라인) |
| `LandscapeStreamingProxy.h` | `ALandscapeStreamingProxy` (WP 스트리밍) |
| `LandscapeComponent.h` | `ULandscapeComponent` 타일 컨테이너, `FWeightmapLayerAllocationInfo`, `FLandscapeVertexRef` |
| `LandscapeInfo.h` | `ULandscapeInfo` 레지스트리, `FLandscapeInfoLayerSettings` |
| `LandscapeInfoMap.h` | 월드당 Info 맵 |
| `LandscapeEditLayer.h` | `ULandscapeEditLayerBase` 및 하위 클래스 |
| `LandscapeHeightfieldCollisionComponent.h` | Heightfield 콜리전, `ECollisionQuadFlags`, `FHeightfieldGeometryRef` |
| `LandscapeMeshCollisionComponent.h` | ⚠️ Deprecated 5.7 (XY 오프셋 시절 유물) |
| `LandscapeNaniteComponent.h` | Nanite 컴포넌트, `FAsyncBuildData` |
| `LandscapeLayerInfoObject.h` | `ULandscapeLayerInfoObject` 페인트 레이어 에셋 |
| `LandscapeMaterialInstanceConstant.h` | Landscape 전용 MIC |
| `LandscapePhysicalMaterial.h` | `FLandscapePhysicalMaterialRenderTask` (GPU 물리재질 렌더) |
| `LandscapeGrassType.h` | 그래스 (이 문서에서는 스킵) |
| `LandscapeSplinesComponent.h` | 스플라인 (이 문서에서는 스킵) |
| `LandscapeEdgeFixup.h` | `ULandscapeHeightmapTextureEdgeFixup` 경계 공유 유틸 |

#### Public/ (LANDSCAPE_API 공개 헤더)

| 파일 | 주요 내용 |
|------|---------|
| `LandscapeSubsystem.h` | `ULandscapeSubsystem`, 그래스·스트리밍 매니저 핸들 |
| `LandscapeRender.h` | `FLandscapeComponentSceneProxy`, `FLandscapeSharedBuffers`, 셰이더 유니폼, LOD |
| `LandscapeDataAccess.h` | Heightmap 패킹/언패킹 (`LANDSCAPE_ZSCALE`, `PackHeight`, `UnpackHeight`, `UnpackNormal`) |
| `LandscapeEditLayerMergeContext.h` | `FMergeContext` BatchedMerge 전역 메타데이터 |
| `LandscapeEditLayerMergeRenderContext.h` | `FMergeRenderContext`, `FMergeRenderParams`, `FMergeRenderStep`, `FMergeRenderBatch` |
| `LandscapeEditLayerRenderer.h` | `ILandscapeEditLayerRenderer` 인터페이스 |
| `LandscapeEditLayerRendererState.h` | `FEditLayerRendererState` (Supported vs Enabled) |
| `LandscapeEditLayerTargetTypeState.h` | `FEditLayerTargetTypeState` 타겟 타입 집합 |
| `LandscapeEditLayerTargetLayerGroup.h` | `FTargetLayerGroup` 블렌딩 의존성 |
| `LandscapeEditLayerTypes.h` | `ELandscapeTargetLayerBlendMethod` 등 열거형 |
| `LandscapeEdit.h` | 에디터 편집 API |
| `LandscapeSettings.h` | 프로젝트 설정 |
| `LandscapeTextureHash.h` | 텍스처 해시 유틸 |
| `LandscapeTextureStorageProvider.h` | 텍스처 저장 공급자 |
| `LandscapeUtils.h` | 공용 유틸 함수 |
| `LandscapeBlueprintBrushBase.h` | 블루프린트 브러시 베이스 |
| `LandscapeConfigHelper.h` | 설정 헬퍼 (프렌드로 `ALandscape` 내부 접근) |
| `LandscapeModule.h` | 모듈 진입점 |

#### Private/ (구현)

| 파일 | 주요 내용 |
|------|---------|
| `LandscapeEditLayers.cpp` | **BatchedMerge 구현체** (~5000 라인) — `PerformLayersHeightmapsBatchedMerge:4499` 등 |
| `LandscapeRender.cpp` | Scene Proxy 구현 |
| `LandscapeCollision.cpp` | 콜리전 생성·갱신 |
| `Landscape.cpp` | `ALandscape` 구현 |
| `LandscapeComponent.cpp` | `ULandscapeComponent` 구현 |

### 1.2 주요 코드 진입점 (line 수치는 기준 시점 참고값, 버전별 변동 가능)

| 기능 | 파일:라인 |
|------|---------|
| ALandscape 클래스 선언 | `Landscape.h:275-276` |
| ALandscape BatchedMerge 함수 private 선언 | `Landscape.h:585-627` |
| Heightmap 16비트 패킹 | `LandscapeDataAccess.h:38-50` |
| LANDSCAPE_ZSCALE 상수 | `LandscapeDataAccess.h:13-14` |
| 섹션·서브섹션 크기 제약 | `LandscapeComponent.h:427-435` |
| FWeightmapLayerAllocationInfo | `LandscapeComponent.h:141-187` |
| ULandscapeComponent::HeightmapTexture | `LandscapeComponent.h:552` |
| ULandscapeComponent::WeightmapTextures | `LandscapeComponent.h:560` |
| ULandscapeInfo 클래스 | `LandscapeInfo.h:107` |
| ULandscapeInfo::XYtoComponentMap | `LandscapeInfo.h:146-147` |
| ULandscapeInfo::Find / FindOrCreate | `LandscapeInfo.h:380-382` |
| ULandscapeInfo::GetSortedStreamingProxies | `LandscapeInfo.h:388` |
| ULandscapeInfo::RegisterActor | `LandscapeInfo.h:406` |
| ULandscapeInfo::GetGridSize | `LandscapeInfo.h:375` |
| ULandscapeStreamingProxy::LandscapeActorRef | `LandscapeStreamingProxy.h:32-33` |
| ULandscapeStreamingProxy::IsValidLandscapeActor | `LandscapeStreamingProxy.h:63` |
| ULandscapeStreamingProxy::ShouldIncludeGridSizeInName | `LandscapeStreamingProxy.h:49` |
| FMergeContext 클래스 | `LandscapeEditLayerMergeContext.h:31` |
| FMergeRenderStep::EType (단계 타입 7종) | `LandscapeEditLayerMergeRenderContext.h:75-137` |
| FMergeRenderBatch | `LandscapeEditLayerMergeRenderContext.h:146` |
| FLandscapeComponentSceneProxy | `LandscapeRender.h:701` |
| FLandscapeSharedBuffers | `LandscapeRender.h:358` |
| LANDSCAPE_LOD_LEVELS (=8) | `LandscapeRender.h:35` |
| FLandscapeUniformShaderParameters | `LandscapeRender.h:116-140` |
| ULandscapeNaniteComponent | `LandscapeNaniteComponent.h:82` |
| FAsyncBuildData | `LandscapeNaniteComponent.h:35-78` |
| ULandscapeHeightfieldCollisionComponent | `LandscapeHeightfieldCollisionComponent.h:40` |
| ECollisionQuadFlags | `LandscapeHeightfieldCollisionComponent.h:180-185` |
| FHeightfieldGeometryRef | `LandscapeHeightfieldCollisionComponent.h:102` |
| ULandscapeSubsystem | `LandscapeSubsystem.h:101-336` |
| ULandscapeSubsystem::FindOrAddLandscapeProxy | `LandscapeSubsystem.h:173` |
| ULandscapeSubsystem::OnHeightmapStreamed | `LandscapeSubsystem.h:198` |

### 1.3 Editor/LandscapeEditor 모듈 (참고용)

**경로**: `Engine/Source/Editor/LandscapeEditor/`

이 문서 시리즈는 **Editor Mode 내부를 다루지 않지만**, 에디터 흐름이 궁금하다면:

| 파일 | 내용 |
|------|------|
| `Private/LandscapeEditorModule.cpp` | 에디터 모듈 초기화 |
| `Private/LandscapeEdMode.cpp` | Landscape 에디터 모드 (스컬프트/페인트 도구 마스터) |
| `Private/LandscapeEdModeTools.cpp` | 개별 브러시 도구들 |
| `Classes/LandscapeEditorObject.h` | 에디터 UI 옵션 |

## 2. 관련 모듈 소스

Landscape가 의존하거나 상호작용하는 다른 모듈들의 관련 파일:

| 기능 | 파일 |
|------|------|
| World Partition | `Engine/Source/Runtime/Engine/Classes/ActorPartition/PartitionActor.h` |
| Chaos Physics (Heightfield) | `Engine/Source/Runtime/Experimental/Chaos/Public/Chaos/HeightField.h` |
| Nanite | `Engine/Source/Runtime/Renderer/Private/Nanite/` |
| NavMesh 지형 export | `Engine/Source/Runtime/NavigationSystem/Public/Navigation/NavigableGeometryExport.h` |
| Runtime Virtual Texture | `Engine/Source/Runtime/Engine/Public/VT/RuntimeVirtualTextureEnum.h` |

## 3. Cvar (런타임 튜닝)

Landscape 동작에 영향을 주는 주요 콘솔 변수:

| Cvar | 영향 |
|------|------|
| `landscape.Nanite` | Nanite 경로 활성화 토글 |
| `landscape.Nanite.MaxSimultaneousMultithreadBuilds` | Nanite 동시 빌드 수 |
| `landscape.Nanite.MultithreadBuild` | Nanite 병렬 빌드 사용 여부 |
| `landscape.RenderCaptureNextMergeDraws` | 다음 머지를 RenderDoc 등에 캡처 |
| `landscape.SplineFalloffModulation` | 스플라인 감쇠 모듈레이션 |
| `r.ShaderCompilation.SkipCompilation` | (일반) 머지 진단용 |

## 4. 공식 문서

### 4.1 Epic Games 공식

- **Landscape Outliner and World Settings** — https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-outliner-in-unreal-engine
- **Landscape Technical Guide** — 공식 기술 레퍼런스 (버전별 경로 상이)
- **World Partition — Landscape** — 파티션 환경에서의 Landscape 사용
- **Nanite for Landscape** — Nanite 랜드스케이프 설정 가이드
- **Landscape Edit Layers** — 에디트 레이어 워크플로우

### 4.2 Epic 릴리즈 노트 (주요 변경점)

- **5.0**: Nanite 도입 (초기 Landscape 통합은 실험적)
- **5.3**: Edit Layers 파이프라인 개선, Heightmap RT 머지 안정화
- **5.5**: BatchedMerge 전환 시작 (GlobalMerge/LocalMerge 병존)
- **5.6**: 일부 API deprecation, 에디터 리소스 서브시스템으로 전역 이동
- **5.7**: **비-edit-layer Landscape 완전 제거**, BatchedMerge 기본, `ULandscapeMeshCollisionComponent` deprecated

## 5. 학술·기술 자료

### 5.1 지형 렌더링 일반

- **"Real-Time Rendering"** (Akenine-Möller et al.) — 지형 렌더링 장
- **GPU Gems 2, Chapter 2** — "Terrain Rendering Using GPU-Based Geometry Clipmaps" (Asirvatham & Hoppe) — GPU가 Heightmap을 샘플링해 지형을 생성하는 고전적 접근
- **GPU Gems 3, Chapter 1** — "Generating Complex Procedural Terrains Using the GPU"

### 5.2 LOD 기법

- **Lindstrom, P. et al.** (1996). "Real-Time, Continuous Level of Detail Rendering of Height Fields." SIGGRAPH. — Landscape의 Continuous LOD 아이디어의 뿌리
- **Duchaineau, M. et al.** (1997). "ROAMing Terrain: Real-time Optimally Adapting Meshes" — ROAM 알고리즘, 현대 엔진의 직접 채용은 드물지만 사고방식은 공유
- **Ulrich, T.** (2002). "Rendering Massive Terrains using Chunked Level of Detail Control" — 청크 기반 LOD

### 5.3 Nanite

- **"A Deep Dive into Nanite Virtualized Geometry"** (Karis, SIGGRAPH 2021) — Nanite 기본 설계 논문
- **Unreal Engine 공식 Nanite 문서** — 런타임 동작과 빌드 파이프라인

### 5.4 물리 Heightfield

- **Chaos Physics 공식 문서** — `FHeightField` 사용법
- **Real-Time Collision Detection** (Christer Ericson) — Heightfield 쿼리 알고리즘

## 6. 프로젝트 내 관련 문서

| 문서 | 링크 |
|------|------|
| World Partition (Data Layers) | [Docs/WorldManagement/](../../WorldManagement/) |
| RecastNavMesh / Pathfinding (Landscape collision이 NavMesh 소스) | [Docs/AI/RecastNavMesh/](../../AI/RecastNavMesh/) |

## 7. 이 문서 시리즈의 내부 상호 참조

| 문서 | 핵심 주제 | 의존 (먼저 읽으면 좋은 것) |
|------|---------|------|
| [README.md](README.md) | 전체 안내 | — |
| [01-overview.md](01-overview.md) | Landscape 개요 | — |
| [02-architecture.md](02-architecture.md) | 액터 3종 + Info | 01 |
| [03-core-classes.md](03-core-classes.md) | 클래스 레퍼런스 | 02 |
| [04-heightmap-weightmap.md](04-heightmap-weightmap.md) | 텍스처 레이아웃 | 03 |
| [05-edit-layers.md](05-edit-layers.md) | BatchedMerge | 04 |
| [06-rendering-pipeline.md](06-rendering-pipeline.md) | Scene Proxy + Nanite | 04 |
| [07-streaming-wp.md](07-streaming-wp.md) | WP 스트리밍 | 02, 06 |
| [08-collision-physics.md](08-collision-physics.md) | 콜리전·물리재질 | 04, 07 |

## 8. 향후 분석 후보 (별도 이슈)

이 문서 시리즈는 다음 서브시스템을 **의도적으로 제외**했습니다. 각각 별도 분석 이슈로 파생 가능:

- **Landscape Grass** — 그래스/식생 자동 배치, `ULandscapeGrassType`, `FLandscapeGrassMapsBuilder`
- **Landscape Splines** — 지형 위 경로 (`ULandscapeSplinesComponent`, `ULandscapeSplineControlPoint`, `ULandscapeSplineSegment`)
- **Landscape Editor Mode 내부** — 스컬프트/페인트 도구의 구체 구현
- **Blueprint Brush 파이프라인** — `ALandscapeBlueprintBrushBase` 확장 메커니즘
- **Runtime Virtual Texture 연동** — Landscape의 RVT 렌더
- **HLOD for Landscape** — `LandscapeHLODBuilder.h`
- **Foliage on Landscape** — 인스턴스 기반 식생 시스템과의 상호작용

각각의 분석은 이 문서의 기본 지식을 전제로 시작할 수 있습니다.
