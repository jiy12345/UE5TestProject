# 01. RecastNavMesh 개요

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. Recast/Detour란?

Recast/Detour는 Mikko Mononen이 개발한 오픈소스 NavMesh 라이브러리로, 많은 게임 엔진에서 채택하고 있습니다.

| 라이브러리 | 역할 | UE 위치 |
|-----------|------|---------|
| **Recast** | 지오메트리 → NavMesh 생성 (빌드 타임) | `Engine/Source/Runtime/Navmesh/Private/Recast/` |
| **Detour** | NavMesh 런타임 쿼리 (경로 찾기, 투사 등) | `Engine/Source/Runtime/Navmesh/Private/Detour/` |
| **DetourCrowd** | 다중 에이전트 군집 이동 시뮬레이션 | `Engine/Source/Runtime/Navmesh/Private/DetourCrowd/` |
| **DetourTileCache** | 타일 캐시 및 동적 장애물 처리 | `Engine/Source/Runtime/Navmesh/Private/DetourTileCache/` |

## 2. UE에서의 통합 구조

```
┌─────────────────────────────────────────┐
│           언리얼 엔진 레이어              │
│                                         │
│  UNavigationSystemV1                    │  ← 네비게이션 전체 관리자
│  └── ARecastNavMesh                     │  ← NavMesh 액터 (월드에 배치)
│       ├── FPImplRecastNavMesh           │  ← Detour(dtNavMesh) 래퍼
│       └── FRecastNavMeshGenerator       │  ← 빌드 파이프라인 관리
│            └── FRecastTileGenerator     │  ← 타일 단위 빌드 작업
│                                         │
├─────────────────────────────────────────┤
│         Recast/Detour 라이브러리 레이어   │
│                                         │
│  rcContext / rcConfig                   │  ← Recast 빌드 컨텍스트/설정
│  dtNavMesh                              │  ← NavMesh 데이터 저장소
│  dtNavMeshQuery                         │  ← 경로 쿼리 실행기
│  dtTileCache                            │  ← 타일 캐시 (DynamicModifiersOnly)
└─────────────────────────────────────────┘
```

## 3. 타일(Tile) 기반 구조

UE의 NavMesh는 **타일 그리드** 방식으로 동작합니다. 월드 전체를 균일한 크기의 사각형 타일로 분할하고, 각 타일을 독립적으로 빌드/업데이트할 수 있습니다.

```
┌───┬───┬───┬───┐
│0,3│1,3│2,3│3,3│
├───┼───┼───┼───┤
│0,2│1,2│2,2│3,2│
├───┼───┼───┼───┤
│0,1│1,1│2,1│3,1│
├───┼───┼───┼───┤
│0,0│1,0│2,0│3,0│
└───┴───┴───┴───┘
  각 칸 = 하나의 NavMesh 타일 (TileSizeUU 기준)
```

**장점**:
- 변경된 영역의 타일만 재빌드 가능 (전체 재빌드 불필요)
- 타일 단위로 스트리밍/언로드 가능 (World Partition)
- 멀티스레드로 타일 병렬 빌드

## 4. Runtime Generation 모드

`ARecastNavMesh::RuntimeGeneration` 속성으로 결정됩니다.

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h:528`

```cpp
UENUM()
enum class ERuntimeGenerationType : uint8
{
    Static,                 // 런타임 재생성 없음 (에디터에서 사전 빌드)
    DynamicModifiersOnly,   // Nav Modifier만 런타임 업데이트 가능
    Dynamic,                // 지오메트리 포함 완전 동적 재생성
    LegacyGeneration,       // 구버전 호환
};
```

| 모드 | 설명 | 타일 캐시 | 지오메트리 재빌드 |
|------|------|----------|----------------|
| `Static` | 런타임 변경 없음 | 불필요 | ✗ |
| `DynamicModifiersOnly` | Modifier만 업데이트 | **필요** (CompressedTileCacheLayers) | ✗ |
| `Dynamic` | 완전 동적 | 불필요 | ✓ |

### DynamicModifiersOnly 핵심 동작

게임 월드에서 `DynamicModifiersOnly`는 "게임 스태틱 NavMesh"로 취급됩니다.

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp`

```cpp
static bool IsGameStaticNavMesh(ARecastNavMesh* InNavMesh)
{
    return (InNavMesh->GetWorld()->IsGameWorld() &&
            InNavMesh->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic);
}
```

지오메트리 리빌드 없이 **Modifier만** 타일에 다시 적용합니다. 이때 기존 지오메트리 데이터는 `CompressedTileCacheLayers`에서 가져옵니다.

## 5. NavMesh 빌드 파이프라인 요약

```
월드 지오메트리 수집
        ↓
복셀화 (Voxelization)    ← Recast rcRasterizeTriangles
        ↓
필터링 (Filtering)       ← 낮은 천장, 절벽 등 제거
        ↓
영역 분할 (Region)       ← rcBuildRegions (Watershed/Monotone/Layers)
        ↓
윤곽 생성 (Contour)      ← rcBuildContours
        ↓
폴리곤 메시 생성          ← rcBuildPolyMesh
        ↓
세부 메시 생성 (Detail)   ← rcBuildPolyMeshDetail
        ↓
Detour 타일 생성          ← dtCreateNavMeshData
        ↓
압축 & 캐시 저장          ← DetourTileCache (DynamicModifiersOnly)
        ↓
dtNavMesh에 타일 추가     ← dtNavMesh::addTile
```

## 6. World Partition 환경

World Partition에서는 NavMesh 데이터가 `ANavigationDataChunkActor`에 분산 저장됩니다.

- **에디터**: 전체 NavMesh 빌드 후 청크 액터에 타일 데이터 저장
- **런타임**: 청크 액터 스트리밍 로드 시 `AttachTiles()` 호출 → `dtNavMesh`에 타일 등록
- **언로드**: `DetachTiles()` 호출 → `dtNavMesh`에서 타일 제거

이 스트리밍 메커니즘은 DynamicModifiersOnly의 타이밍 문제와 깊이 연관되어 있습니다. ([04-source-analysis.md](04-source-analysis.md) 참조)
