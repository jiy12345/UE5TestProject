# 4. 원인 분석: AttachTiles 오버라이트 가설

> **작성일**: 2026-02-18 | **엔진 버전**: UE 5.7

## 핵심 가설

**Data Layer 스트리밍 셀에 NavDataChunkActor(geometry 포함)가 있을 경우, `AttachTiles`가 dynamically modifier-applied 타일을 pre-baked 타일로 덮어쓰고, 이후 dirty area를 생성하지 않아 modifier가 재적용되지 않는다.**

## AttachTiles 소스 분석

**파일**: `Engine/Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp`
**함수**: `URecastNavMeshDataChunk::AttachTiles`
**line**: ~215

```cpp
void URecastNavMeshDataChunk::AttachTiles(ARecastNavMesh& NavMesh)
{
    dtNavMesh* DetourNavMesh = NavMesh.GetRecastMesh();

    for (FRecastTileData& TileData : Tiles)
    {
        if (!TileData.bInitialized) continue;

        const dtMeshHeader* Header = ...;

        // ★ 핵심: 해당 (X, Y, Layer) 위치의 기존 타일을 제거
        if (const dtMeshTile* PreExistingTile =
            DetourNavMesh->getTileAt(Header->x, Header->y, Header->layer))
        {
            NavMesh.LogRecastTile(ELogVerbosity::VeryVerbose,
                TEXT("removing"), ..., "URecastNavMeshDataChunk::AttachTiles");

            dtTileRef PreExistingTileRef = DetourNavMesh->getTileRef(PreExistingTile);
            DetourNavMesh->removeTile(PreExistingTileRef, nullptr, nullptr);
            // ← modifier가 적용된 타일이 여기서 제거됨!
        }

        // pre-baked 타일 추가 (modifier가 적용되지 않은 원본)
        dtTileRef TileRef = 0;
        NavMesh.AddTile(TileData, TileRef);
        // ← modifier 없는 쿠킹 시점 타일로 교체됨!
    }
}
```

## AttachNavMeshDataChunk 전후 흐름

**파일**: `RecastNavMesh.cpp` line ~3263

```cpp
void ARecastNavMesh::AttachNavMeshDataChunk(URecastNavMeshDataChunk& DataChunk)
{
    // 타일 교체
    DataChunk.AttachTiles(*this);

    // 영향받은 경로 무효화 (경로 재계산용)
    InvalidateAffectedPaths(DataChunk.GetTileRefs());

    // ★ dirty area 생성 없음! → TileGen이 재실행되지 않음
    // → modifier 재적용 기회가 없음
}
```

## 버그 발생 시나리오 (원본 프로젝트)

```
[쿠킹 시]
AlwaysLoaded 레이어:
    NavModifier_CTRL (AlwaysLoaded 레이어) → 쿠킹 시 tile에 적용됨 → NavDataChunk에 저장

DataLayer 레이어:
    NavModifier_DL + geometry actors → 쿠킹 시 tile에 modifier 적용됨 → NavDataChunk에 저장

[런타임]
1. 게임 시작: AlwaysLoaded 타일 로드 → modifier 적용됨 (OK)

2. DataLayer 활성화:
   a. NavModifier_DL이 nav octree에 등록됨 (bIsFromVisibilityChange=?)
   b. DirtyArea 생성 → MarkDirtyTiles PASSED → TileGen 시작
   c. AfterGatherGeometry Modifiers=1 → MarkDynamicAreas 실행
   d. modifier가 적용된 타일 완성 ← 여기까지는 OK

3. NavDataChunkActor 로드 (geometry 포함 셀):
   a. AttachNavMeshDataChunk 호출
   b. AttachTiles: 기존 타일(modifier 적용됨) 제거 → "removing" 로그
   c. pre-baked 타일(modifier 없음) 추가
   d. dirty area 생성 없음 → TileGen 미실행
   e. ★ modifier 미적용 상태로 고정됨
```

## 테스트 프로젝트와 원본 프로젝트의 차이

| 항목 | 테스트 프로젝트 | 원본 프로젝트 |
|------|----------------|--------------|
| DL 셀 내용 | NavModifierVolume **만** | NavModifierVolume + geometry actors |
| NavDataChunkActor | **없음** (geometry 없으므로) | **있음** |
| AttachTiles 호출 | **없음** | **있음** |
| 결과 | 우연히 작동 (진동) | 항상 실패 |

→ 테스트 프로젝트에서는 AttachTiles가 호출되지 않으므로, modifier가 적용된 타일이 덮어써지지 않음.

## 잘못된 가설들 (기각)

### ❌ bExcludeLoadedData 이론

처음에는 `OnStreamingNavDataAdded`의 `bExcludeLoadedData=true`가 dirty area를 막는다고 가정했으나:

```cpp
// RecastNavMesh.cpp
void ARecastNavMesh::OnStreamingNavDataAdded(...)
{
    if (IsWorldPartitionedDynamicNavmesh())  // ← bUseWPDynamicMode=true일 때만
    {
        bExcludeLoadedData = true;
    }
}
```

`bUseWPDynamicMode=0`이면 이 블록 자체가 실행되지 않음 → 기각.

### ❌ MarkDirtyTiles 필터링 이론

`LogNavigationDirtyArea=VeryVerbose` 로그에서 `MarkDirtyTiles PASSED` 확인됨 → 필터링이 문제가 아님.

### ❌ CompressedLayers 부재 이론

`CompressedLayers=1` 로그 확인됨 → pre-baked 타일 존재함, modifier-only 경로 사용 중.

## 미확인 사항

- `AttachTiles`에서 "removing" 로그가 실제로 modifier 적용 후에 발생하는지 (`LogNavigation=VeryVerbose` 로그에서 확인 필요)
- 원본 프로젝트의 DL 셀에 실제로 NavDataChunkActor가 있는지 확인 필요
- `AttachTiles` 호출 순서: modifier TileGen 완료 → AttachTiles 인지, 아니면 AttachTiles → modifier TileGen 인지
