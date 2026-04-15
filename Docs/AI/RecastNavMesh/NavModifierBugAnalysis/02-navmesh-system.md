# 2. NavMesh 시스템 구조 이해

> **작성일**: 2026-02-18 | **엔진 버전**: UE 5.7

## DynamicModifiersOnly 모드란

`ERuntimeGenerationType::DynamicModifiersOnly`는 **런타임에 geometry rebuild 없이 Nav Area Modifier만 업데이트**하는 모드.

### 핵심 메커니즘

```
쿠킹 시:
    ├─ 전체 NavMesh 빌드 (geometry 포함)
    └─ CompressedTileCacheLayers 저장 (타일별 geometry 캐시)

런타임 modifier 적용 시:
    ├─ 저장된 CompressedTileCacheLayers 로드 (geometry rebuild 생략)
    ├─ Nav Area Modifier만 적용 (MarkDynamicAreas)
    └─ 타일 갱신
```

### IsGameStaticNavMesh()

```cpp
// RecastNavMesh.cpp
bool ARecastNavMesh::IsGameStaticNavMesh() const
{
    return RuntimeGeneration == ERuntimeGenerationType::DynamicModifiersOnly
        && GetWorld() && GetWorld()->IsGameWorld();
}
```

이 함수가 `true`를 반환하면 **MarkDirtyTiles에서 modifier 전용 필터링**이 활성화됨:
- `DynamicModifier` 플래그가 있는 dirty area만 통과
- `NavigationBounds` 플래그가 있으면 필터링 (geometry 변경 무시)

## MarkDirtyTiles 필터링 로직

**파일**: `RecastNavMeshGenerator.cpp`

```cpp
// line 6600 근방
if (IsGameStaticNavMesh())
{
    // DynamicModifiersOnly: modifier 플래그 없으면 건너뜀
    if (!DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier))
    {
        // [NavDebug-FILTER] FILTERED 로그
        continue;
    }
    // NavigationBounds 플래그는 geometry 변경 → 제외
    if (DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds))
    {
        continue;
    }
}
// [NavDebug-FILTER] PASSED 로그
```

## TileGenerator Setup 흐름

**파일**: `RecastNavMeshGenerator.cpp` line 1800 근방

```cpp
// DirtyAreas가 비어있으면 bGeometryChanged=true (modifier-only 경로 아님)
const bool bGeometryChanged = (DirtyAreas.Num() == 0);

if (!bGeometryChanged)
{
    // modifier-only 경로: 저장된 CompressedLayers 로드
    CompressedLayers = ParentGenerator.GetOwner()->GetTileCacheLayers(TileX, TileY);
}

// CompressedLayers가 없으면 geometry rebuild 강제
bRegenerateCompressedLayers = (bGeometryChanged || TileConfig.bGenerateLinks
    || CompressedLayers.Num() == 0);
```

### 핵심: CompressedLayers가 없으면 modifier-only 경로 실패

쿠킹된 NavDataChunk가 없거나 해당 타일의 레이어 데이터가 없으면 `bRegenerateCompressedLayers=true` → geometry rebuild → modifier 적용 실패 가능

## NavDataChunk와 AttachTiles

### NavDataChunk란

- **World Partition 스트리밍 셀**에 NavMesh 타일 데이터를 저장하는 액터(`ARecastNavMeshDataChunkActor`)
- 셀이 스트리밍될 때 `AttachNavMeshDataChunk` → `AttachTiles` 호출

### AttachTiles 동작

**파일**: `RecastNavMeshDataChunk.cpp` line 215 근방

```cpp
void URecastNavMeshDataChunk::AttachTiles(ARecastNavMesh& NavMesh)
{
    for (각 타일)
    {
        // 해당 (X, Y, Layer) 위치에 기존 타일이 있으면 제거
        if (const dtMeshTile* PreExistingTile = DetourNavMesh->getTileAt(Header->x, Header->y, Header->layer))
        {
            NavMesh.LogRecastTile(..., "removing");  // VeryVerbose 로그
            DetourNavMesh->removeTile(PreExistingTileRef, nullptr, nullptr);
            // ← 동적으로 modifier가 적용된 타일을 제거!
        }
        // 쿠킹된 pre-baked 타일 추가 (modifier 없는 상태)
        NavMesh.AddTile(...);
    }
}
```

**이것이 핵심 버그 후보**: modifier가 적용된 타일을 제거하고, modifier 없는 pre-baked 타일로 교체함.

## World Partition Dynamic NavMesh 모드

`bUseWPDynamicMode=true`이면 `IsWorldPartitionedDynamicNavmesh()`가 `true`를 반환하여 추가 로직이 활성화됨.

### OnStreamingNavDataAdded의 bExcludeLoadedData

```cpp
// RecastNavMesh.cpp line 3188
void ARecastNavMesh::OnStreamingNavDataAdded(...)
{
    if (IsWorldPartitionedDynamicNavmesh())
    {
        // bExcludeLoadedData=true → dirty area 생성 시 bLoadedData 마킹
        bExcludeLoadedData = true;
    }
    // ...
}
```

**이 프로젝트에서는 `bUseWPDynamicMode=0`이므로 이 블록이 실행되지 않음** → 이전의 "bExcludeLoadedData 이론"은 틀렸음.

## 플래그 관계 요약

```
FNavigationDirtyElement::bIsFromVisibilityChange
    └─ true이면 bLoadedData=true로 마킹될 수 있음
    └─ bLoadedData=true + bExcludeLoadedData=true → dirty area 생성 안 됨

    (단, bUseWPDynamicMode=false인 경우 bExcludeLoadedData는 항상 false)
```
