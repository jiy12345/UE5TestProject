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

> **소스 확인 위치**
> - Recast 헤더: `Engine/Source/Runtime/Navmesh/Public/Recast/Recast.h` — `rcConfig`, `rcContext`, `rcHeightfield` 등 전체 Recast API
> - Detour 헤더: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtNavMesh`, `dtMeshTile`, `dtPoly` 등
> - UE의 Recast 통합 진입점: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h` — `ARecastNavMesh` 클래스 선언부

---

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

> **소스 확인 위치**
> - `UNavigationSystemV1`: `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h`
> - `ARecastNavMesh`: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:572`
> - `FPImplRecastNavMesh`: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/PImplRecastNavMesh.h` — `dtNavMesh*` 멤버 보유
> - `FRecastNavMeshGenerator`: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:784`
> - `FRecastTileGenerator`: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h` — `FRecastNavMeshGenerator` 하단

---

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

> **소스 확인 위치**
> - 타일 크기 설정: `RecastNavMesh.h` — `UPROPERTY TileSizeUU` (에디터에서 조정 가능한 값)
> - 타일 그리드 좌표 계산: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtNavMesh::calcTileLoc()`
> - 타일당 최대 폴리곤 수 계산: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `CalcNavMeshProperties()`

---

## 4. Runtime Generation 모드

`ARecastNavMesh::RuntimeGeneration` 속성으로 결정됩니다.

> **소스 확인 위치**
> - 열거형 정의: `Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h:528`
> - `ARecastNavMesh`에서의 프로퍼티: `RecastNavMesh.h` — `UPROPERTY RuntimeGeneration`
> - 게임 스태틱 판단: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `IsGameStaticNavMesh()` (약 라인 5105)

```cpp
// NavigationData.h:528
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

```cpp
// RecastNavMeshGenerator.cpp:5105
static bool IsGameStaticNavMesh(ARecastNavMesh* InNavMesh)
{
    return (InNavMesh->GetWorld()->IsGameWorld() &&
            InNavMesh->GetRuntimeGenerationMode() != ERuntimeGenerationType::Dynamic);
}
```

지오메트리 리빌드 없이 **Modifier만** 타일에 다시 적용합니다. 이때 기존 지오메트리 데이터는 `CompressedTileCacheLayers`에서 가져옵니다.

> **소스 확인 위치**
> - `CompressedTileCacheLayers` 저장/조회: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/PImplRecastNavMesh.cpp` — `AddTileCacheLayer()`, `GetTileCacheLayers()`
> - Modifier 전용 리빌드 판단 분기: `RecastNavMeshGenerator.cpp` 약 라인 1800-1815 — `bRegenerateCompressedLayers` 결정 로직

---

## 5. NavMesh 빌드 파이프라인 요약

```
월드 지오메트리 수집
        ↓
복셀화 (Voxelization)    ← rcRasterizeTriangles
        ↓
필터링 (Filtering)       ← rcFilterLowHangingWalkableObstacles 등
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
압축 & 캐시 저장          ← dtTileCache (DynamicModifiersOnly)
        ↓
dtNavMesh에 타일 추가     ← dtNavMesh::addTile
```

> **소스 확인 위치**
> - 각 `rc*` 함수 시그니처: `Engine/Source/Runtime/Navmesh/Public/Recast/Recast.h`
> - 전체 빌드 단계 실행부: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastTileGenerator::GenerateCompressedLayers()` (약 라인 3706)
> - Detour 타일 데이터 생성: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMeshBuilder.h` — `dtCreateNavMeshData()`
> - `dtNavMesh::addTile()`: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h`

---

## 6. World Partition 환경

World Partition에서는 NavMesh 데이터가 `ANavigationDataChunkActor`에 분산 저장됩니다.

- **에디터**: 전체 NavMesh 빌드 후 청크 액터에 타일 데이터 저장
- **런타임**: 청크 액터 스트리밍 로드 시 `AttachTiles()` 호출 → `dtNavMesh`에 타일 등록
- **언로드**: `DetachTiles()` 호출 → `dtNavMesh`에서 타일 제거

> **소스 확인 위치**
> - `ANavigationDataChunkActor`: `Engine/Source/Runtime/NavigationSystem/Public/NavigationDataChunkActor.h`
> - `AttachTiles()` / `DetachTiles()`: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp:203`
> - `ARecastNavMesh::OnStreamingNavDataAdded()`: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp:3188`

이 스트리밍 메커니즘은 DynamicModifiersOnly의 타이밍 문제와 깊이 연관되어 있습니다. ([NavModifierBugAnalysis](NavModifierBugAnalysis/README.md) 참조)
