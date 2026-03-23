# 03. 핵심 클래스 및 구조체

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. UE 레이어 클래스

### ARecastNavMesh

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:572`

#### 주요 상수

```cpp
#define RECAST_MAX_SEARCH_NODES     2048       // A* 노드 풀 크기
#define RECAST_MIN_TILE_SIZE        300.f      // 최소 타일 크기 (UU)
#define RECAST_MAX_AREAS            64         // 최대 NavArea 수
#define RECAST_DEFAULT_AREA         63         // NavArea_Default ID
#define RECAST_LOW_AREA             62         // NavArea_LowHeight ID
#define RECAST_NULL_AREA            0          // NavArea_Null ID (이동 불가)
#define RECAST_UNWALKABLE_POLY_COST FLT_MAX    // 이동 불가 폴리곤 비용
```

#### 주요 UPROPERTY (에디터 설정)

```cpp
// 타일 설정
UPROPERTY(EditAnywhere, Category=Generation, config)
float TileSizeUU;                  // 타일 크기 (기본 1000 UU)

UPROPERTY(EditAnywhere, Category=Generation, config)
FNavMeshResolutionParam NavMeshResolutionParams[(uint8)ENavigationDataResolution::MAX];
// 각 해상도(Low/Default/High)별 CellSize, CellHeight, AgentMaxStepHeight

// 에이전트 속성
UPROPERTY(EditAnywhere, Category=Generation, config)
float AgentRadius;                 // 에이전트 반경

UPROPERTY(EditAnywhere, Category=Generation, config)
float AgentHeight;                 // 에이전트 높이

UPROPERTY(EditAnywhere, Category=Generation, config)
float AgentMaxSlope;               // 등반 가능 최대 경사각

// 영역 처리
UPROPERTY(EditAnywhere, Category=Generation, config, AdvancedDisplay)
TEnumAsByte<ERecastPartitioning::Type> RegionPartitioning;
// Watershed / ChunkyMonotone / Monotone

// 런타임 모드
UPROPERTY(EditAnywhere, Category=Runtime, config)
ERuntimeGenerationType RuntimeGeneration;
```

#### 주요 메서드

```cpp
// Detour 메시 직접 접근 (내부용)
dtNavMesh* GetRecastMesh();
const dtNavMesh* GetRecastMesh() const;

// 타일 캐시 레이어 (DynamicModifiersOnly)
bool GetTileCacheLayers(const int32 TileX, const int32 TileY,
                        TArray<FNavMeshTileData>& Layers) const;
void AddTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx,
                       const FNavMeshTileData& LayerData);
void RemoveTileCacheLayer(int32 TileX, int32 TileY, int32 LayerIdx);

// 스트리밍 청크
virtual void AttachNavMeshDataChunk(URecastNavMeshDataChunk& NavDataChunk);
virtual void DetachNavMeshDataChunk(URecastNavMeshDataChunk& NavDataChunk);

// 활성 타일 세트 (World Partition)
const TSet<FIntPoint>& GetActiveTileSet() const;
TSet<FIntPoint>& GetActiveTileSet();
```

---

### FRecastNavMeshGenerator

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:784`

```cpp
class FRecastNavMeshGenerator : public FNavDataGenerator
{
public:
    // 빌드 제어
    virtual bool RebuildAll() override;
    virtual void EnsureBuildCompletion() override;
    virtual void CancelBuild() override;
    virtual void TickAsyncBuild(float DeltaSeconds) override;
    virtual void RebuildDirtyAreas(const TArray<FNavigationDirtyArea>& DirtyAreas) override;

    // 상태 확인
    virtual bool IsBuildInProgressCheckDirty() const override;
    virtual int32 GetNumRemaningBuildTasks() const override;
    virtual int32 GetNumRunningBuildTasks() const override;
    bool HasDirtyTiles() const;

    // 타일 생성 결과 통합
    TArray<FNavTileRef> AddGeneratedTilesAndGetUpdatedTiles(FRecastTileGenerator& TileGenerator);

protected:
    ARecastNavMesh*         DestNavMesh;
    TNavStatArray<FBox>     InclusionBounds;      // 빌드 포함 영역
    TNavStatArray<FBox>     ExclusionBounds;      // 빌드 제외 영역
    FBox                    TotalNavBounds;
    FRecastBuildConfig      Config;               // 빌드 설정 (rcConfig 확장)
    uint32                  Version;              // 빌드 버전
};
```

---

### FRecastBuildConfig

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:55`

`rcConfig` (Recast 원본 설정)를 상속하며 UE 전용 설정을 추가합니다.

```cpp
struct FRecastBuildConfig : public rcConfig
{
    // rcConfig 원본 멤버 (일부):
    // int   width, height         타일 복셀 해상도
    // float cs                    Cell Size (XY 복셀 크기)
    // float ch                    Cell Height (Z 복셀 크기)
    // float walkableSlopeAngle    등반 가능 최대 경사
    // int   walkableHeight        에이전트 높이 (voxel 단위)
    // int   walkableClimb         스텝 높이 (voxel 단위)
    // int   walkableRadius        에이전트 반경 (voxel 단위)
    // int   maxEdgeLen            폴리곤 최대 에지 길이
    // float maxSimplificationError 단순화 오차 허용값
    // int   minRegionArea         최소 영역 면적
    // int   mergeRegionArea       병합할 영역 면적
    // int   maxVertsPerPoly       폴리곤당 최대 버텍스 (≤6)
    // float detailSampleDist      세부 메시 샘플 거리
    // float detailSampleMaxError  세부 메시 최대 오차

    // UE 추가:
    float AgentHeight;            // UU 단위 에이전트 높이
    float AgentRadius;            // UU 단위 에이전트 반경
    float AgentMaxClimb;          // UU 단위 최대 스텝 높이
    float SkipNavGeometry;        // true이면 지오메트리 수집 건너뜀
    int   tileSize;               // 타일당 복셀 수
    int   borderSize;             // 타일 경계 패딩 (복셀)
    bool  bMarkLowHeightAreas;
    bool  bFilterLowSpanSequences;
    bool  bFilterLowSpanFromTileCache;
};
```

---

### FNavTileRef

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h`

타일을 식별하는 값 타입입니다.

```cpp
struct FNavTileRef
{
    uint64 TileDataId;   // dtTileRef 기반 고유 ID
    // 타일 X, Y, Layer는 dtNavMesh::decodePolyId()로 분리 가능
};
```

---

## 2. Detour 라이브러리 클래스

### dtNavMesh

**파일**: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:502`

NavMesh 데이터를 저장하고 타일을 관리하는 핵심 클래스입니다.

```cpp
class dtNavMesh
{
public:
    dtStatus init(const dtNavMeshParams* params);

    // 타일 관리
    dtStatus addTile(unsigned char* data, int dataSize,
                     int flags, dtTileRef lastRef, dtTileRef* result);
    dtStatus removeTile(dtTileRef ref, unsigned char** data, int* dataSize);

    // 타일 조회
    const dtMeshTile* getTileAt(int x, int y, int layer) const;
    dtTileRef         getTileRefAt(int x, int y, int layer) const;
    const dtMeshTile* getTileByRef(dtTileRef ref) const;

    // 폴리곤 조회
    dtStatus getTileAndPolyByRef(dtPolyRef ref,
                                  const dtMeshTile** tile,
                                  const dtPoly** poly) const;
    bool isValidPolyRef(dtPolyRef ref) const;

    // 좌표 → 타일 그리드
    void calcTileLoc(const dtReal* pos, int* tx, int* ty) const;
};
```

**타입 정의** (`DetourNavMesh.h`):

```cpp
// 64비트 주소 환경 (UE 기본)
typedef unsigned long long dtPolyRef;   // 폴리곤 핸들
typedef unsigned long long dtTileRef;   // 타일 핸들

static const int DT_VERTS_PER_POLYGON = 6;
static const int DT_MAX_AREAS         = 64;
static const unsigned int DT_NULL_LINK = 0xffffffff;
```

---

### dtMeshTile

**파일**: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:421`

단일 타일의 런타임 데이터입니다.

```cpp
struct dtMeshTile
{
    unsigned int   salt;            // 레퍼런스 검증용 salt
    dtMeshHeader*  header;          // 타일 헤더
    dtPoly*        polys;           // 폴리곤 배열
    float*         verts;           // 버텍스 배열 (float3 * vertCount)
    dtLink*        links;           // 폴리곤 링크 배열
    dtPolyDetail*  detailMeshes;    // 세부 메시 메타
    float*         detailVerts;     // 세부 버텍스
    unsigned char* detailTris;      // 세부 삼각형
    dtBVNode*      bvTree;          // 가속 BV 트리
    dtOffMeshConnection* offMeshCons; // 오프메시 연결
    unsigned char* data;            // 타일 전체 데이터 블록 (소유)
    int            dataSize;
    int            flags;
    dtMeshTile*    next;            // 같은 그리드 위치의 다음 레이어
};
```

---

### dtPoly

**파일**: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:205`

NavMesh의 기본 단위인 볼록 다각형(Convex Polygon)입니다.

```cpp
struct dtPoly
{
    unsigned int   firstLink;                   // 첫 링크 인덱스
    unsigned short verts[DT_VERTS_PER_POLYGON]; // 버텍스 인덱스 (최대 6)
    unsigned short neis[DT_VERTS_PER_POLYGON];  // 이웃 폴리곤 또는 경계 에지
    unsigned short flags;                       // 폴리곤 플래그
    unsigned char  vertCount;                   // 실제 버텍스 수
    unsigned char  areaAndtype;                 // 하위 6비트: Area ID, 상위 2비트: 타입

    inline unsigned char getArea() const  { return areaAndtype & 0x3f; }
    inline unsigned char getType() const  { return areaAndtype >> 6; }
    inline void setArea(unsigned char a)  { areaAndtype = (areaAndtype & 0xc0) | (a & 0x3f); }
    inline void setType(unsigned char t)  { areaAndtype = (areaAndtype & 0x3f) | (t << 6); }
};

// 폴리곤 타입
enum dtPolyTypes
{
    DT_POLYTYPE_GROUND       = 0,  // 일반 지면 폴리곤
    DT_POLYTYPE_OFFMESH_POINT= 1,  // 오프메시 연결점 (점 → 점)
    DT_POLYTYPE_OFFMESH_SEGMENT = 2, // 오프메시 세그먼트
};
```

---

### dtLink

**파일**: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:252`

폴리곤 간 연결을 표현합니다.

```cpp
struct dtLink
{
    dtPolyRef     ref;    // 이웃 폴리곤 레퍼런스
    unsigned int  next;   // 다음 링크 인덱스
    unsigned char edge;   // 이 링크가 속한 에지 인덱스
    unsigned char side;   // 경계 링크의 방향 (8방향)
    unsigned char bmin;   // 서브에지 최소 범위 [0,255]
    unsigned char bmax;   // 서브에지 최대 범위 [0,255]
};
```

---

### dtNavMeshQuery

**파일**: `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMeshQuery.h`

NavMesh에서 경로 찾기, 투사, 레이캐스트 등을 수행하는 쿼리 클래스입니다. **스레드마다 별도 인스턴스** 필요.

```cpp
class dtNavMeshQuery
{
public:
    dtStatus init(const dtNavMesh* nav, const int maxNodes);

    // 폴리곤 탐색
    dtStatus findNearestPoly(const dtReal* center, const dtReal* halfExtents,
                             const dtQueryFilter* filter,
                             dtPolyRef* nearestRef, dtReal* nearestPt) const;

    // 경로 찾기
    dtStatus findPath(dtPolyRef startRef, dtPolyRef endRef,
                      const dtReal* startPos, const dtReal* endPos,
                      const dtQueryFilter* filter,
                      dtPolyRef* path, int* pathCount, const int maxPath) const;

    // 경로 위치 스무딩
    dtStatus findStraightPath(const dtReal* startPos, const dtReal* endPos,
                              const dtPolyRef* path, const int pathSize,
                              dtReal* straightPath, unsigned char* straightPathFlags,
                              dtPolyRef* straightPathRefs,
                              int* straightPathCount, const int maxStraightPath,
                              const int options = 0) const;

    // 레이캐스트
    dtStatus raycast(dtPolyRef startRef,
                     const dtReal* startPos, const dtReal* endPos,
                     const dtQueryFilter* filter,
                     dtReal* t, dtReal* hitNormal,
                     dtPolyRef* path, int* pathCount, const int maxPath) const;

    // 표면 이동
    dtStatus moveAlongSurface(dtPolyRef startRef,
                              const dtReal* startPos, const dtReal* endPos,
                              const dtQueryFilter* filter,
                              dtReal* resultPos,
                              dtPolyRef* visited, int* visitedCount,
                              const int maxVisitedSize) const;
};
```

---

## 3. NavArea / NavModifier 관련

### UNavArea (C++ 기반 영역 클래스)

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavAreas/NavArea.h`

NavMesh 폴리곤에 부여되는 영역(Area) 속성을 정의합니다.

```cpp
UCLASS(abstract, Config=Engine, MinimalAPI)
class UNavArea : public UObject
{
    UPROPERTY(EditAnywhere, Category=NavArea, config)
    float DefaultCost;           // 통과 비용 (1.0 = 기본)

    UPROPERTY(EditAnywhere, Category=NavArea, config)
    FColor DrawColor;            // 디버그 표시 색상

    UPROPERTY(EditAnywhere, Category=NavArea, config, meta=(Bitmask))
    int32 SupportedAgentsBitmask; // 이 영역을 사용할 에이전트 비트마스크
};
```

주요 내장 NavArea:
| 클래스 | Area ID | 비용 | 의미 |
|--------|---------|------|------|
| `UNavArea_Default` | 63 | 1.0 | 기본 이동 가능 영역 |
| `UNavArea_LowHeight` | 62 | 1.0 | 낮은 공간 |
| `UNavArea_Null` | 0 | FLT_MAX | 이동 불가 |
| `UNavArea_Obstacle` | - | 높음 | 장애물 (커스텀) |

### FNavigationModifier (Nav Modifier 데이터)

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h`

```cpp
struct FAreaNavModifier
{
    FBox             Bounds;          // 영역 경계
    TSubclassOf<UNavArea> AreaClass;  // 적용할 NavArea 클래스
    ENavigationShapeMetaData ShapeMetaData;
    float            Radius;          // (구/캡슐형)
    float            Height;
    bool             bIncludeAgentHeight;
};

struct FNavigationRelevantData
{
    FNavigationBounds           Bounds;
    TArray<FAreaNavModifier>    Modifiers;      // Nav Modifier 목록
    TArray<FSimpleLinkNavModifier> NavLinks;    // Nav Link 목록
    // ...

    bool HasDynamicModifiers() const;
    ENavigationDirtyFlag GetDirtyFlag() const;  // Geometry | DynamicModifier
};
```

### ENavigationDirtyFlag

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h`

```cpp
UENUM()
enum class ENavigationDirtyFlag : uint8
{
    None              = 0,
    Geometry          = (1 << 0),   // 지오메트리 변경
    DynamicModifier   = (1 << 1),   // Dynamic Modifier 변경
    UseAgentHeight    = (1 << 2),   // 에이전트 높이 적용
    NavigationBounds  = (1 << 3),   // 네비게이션 경계 변경
};
```

`DynamicModifiersOnly` 모드에서는 `DynamicModifier` 플래그를 가진 Dirty Area만 처리됩니다.
