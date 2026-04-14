# 03. 핵심 클래스 및 구조체

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. UE 레이어 클래스

### ARecastNavMesh

**파일**: `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:572`

#### 주요 상수 및 수정 영향

```cpp
#define RECAST_MAX_SEARCH_NODES     2048       // A* 노드 풀 크기
#define RECAST_MIN_TILE_SIZE        300.f      // 최소 타일 크기 (UU)
#define RECAST_MAX_AREAS            64         // 최대 NavArea 수
#define RECAST_DEFAULT_AREA         63         // NavArea_Default ID
#define RECAST_LOW_AREA             62         // NavArea_LowHeight ID
#define RECAST_NULL_AREA            0          // NavArea_Null ID (이동 불가)
#define RECAST_UNWALKABLE_POLY_COST FLT_MAX    // 이동 불가 폴리곤 비용
```

| 상수 | 수정 시 영향 |
|------|--------------|
| `RECAST_MAX_SEARCH_NODES` | A\* 탐색 시 오픈/클로즈드 리스트 최대 노드 수. 줄이면 메모리 절감 but 긴 경로 탐색 실패 가능(`DT_OUT_OF_NODES`). 늘리면 폴리곤당 ~20바이트 추가 소비. |
| `RECAST_MIN_TILE_SIZE` | `TileSizeUU`가 이 값보다 작으면 빌드 거부. 낮추면 초소형 타일 허용 but 타일 경계 링크 오버헤드 급증. |
| `RECAST_MAX_AREAS` | `dtPoly::areaAndtype`의 하위 6비트가 Area ID라서 **64가 하드 리미트** (6bit = 0-63). 이 값을 늘리려면 Detour 비트 레이아웃 자체를 수정해야 함. 현실적으로 고정. |
| `RECAST_DEFAULT_AREA` / `RECAST_NULL_AREA` / `RECAST_LOW_AREA` | 내장 NavArea의 ID. 변경하면 직렬화된 네비메시 데이터와 호환성 깨짐 — **수정 금지**. |
| `RECAST_UNWALKABLE_POLY_COST` | `dtQueryFilter`에서 Null 영역 비용. `FLT_MAX`보다 낮추면 AI가 Null 영역을 "비싸지만 통과 가능"으로 보고 가로지를 수 있음. |

#### 주요 UPROPERTY (에디터 설정) 및 수정 영향

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

| 프로퍼티 | 수정 시 영향 |
|----------|--------------|
| `TileSizeUU` | 타일당 메모리/빌드 시간에 비례. 크게 하면 타일 수 ↓ · 타일당 빌드 시간 ↑. 작게 하면 반대. WP 스트리밍에서는 청크 단위 ↔ 타일 단위 매핑이 달라져 재빌드 필요. |
| `NavMeshResolutionParams.CellSize` | 복셀 그리드 XY 해상도. 절반으로 줄이면 복셀 수 4배 → 빌드 시간/메모리 제곱 증가. 낮출수록 경사/좁은 통로 정밀도 ↑. |
| `NavMeshResolutionParams.CellHeight` | 복셀 Z 해상도. 계단/턱 판정 정밀도 결정. 에이전트 스텝 높이보다 작아야 함. |
| `AgentRadius` | **모든 설정 중 가장 영향이 큼**. 변경하면 전체 네비메시 재빌드 필수. Recast가 이 값만큼 장애물에서 "밀어내" 폴리곤을 생성하므로, 에이전트 캡슐 반경과 불일치하면 벽에 끼거나 틈새 통과 불가. |
| `AgentHeight` | 저공간(낮은 천장) 판정에 사용. 증가시키면 기존 통과 가능 영역이 막힐 수 있음. |
| `AgentMaxSlope` | `walkableSlopeAngle`. 급경사 배제 기준. 전체 재빌드 필요. |
| `RegionPartitioning` | Watershed = 깔끔한 영역 분할(느림), Monotone = 빠름(영역 품질↓), ChunkyMonotone = 타협안. 타일당 빌드 시간에 직접 영향. |
| `RuntimeGeneration` | 런타임 동작 자체를 결정 (2-9 참조). 변경해도 이미 쿠킹된 데이터 자체는 그대로 — 에디터에서 다시 빌드해야 실질 반영. |

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:1` — `RECAST_MAX_SEARCH_NODES`, `RECAST_MAX_AREAS` 등 상수 매크로 정의
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:572` — `ARecastNavMesh` 클래스 본문: `TileSizeUU`, `AgentRadius`, `RuntimeGeneration` UPROPERTY 선언
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h` — `GetRecastMesh()`, `GetTileCacheLayers()`, `AddTileCacheLayer()`, `AttachNavMeshDataChunk()` 선언
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp` — `ARecastNavMesh::AttachNavMeshDataChunk()` / `DetachNavMeshDataChunk()` 구현
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp` — `ARecastNavMesh::AddTileCacheLayer()` / `RemoveTileCacheLayer()`: `CompressedTileCacheLayers` TMap 관리

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:784` — `FRecastNavMeshGenerator` 클래스 선언 전체: 빌드 제어 메서드, `InclusionBounds`, `ExclusionBounds`, `Config` 멤버
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastNavMeshGenerator::RebuildDirtyAreas()`: Dirty Area → `MarkDirtyTiles()` 호출 구현
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastNavMeshGenerator::TickAsyncBuild()`: 완료된 타일 통합 및 신규 작업 큐 디스패치
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastNavMeshGenerator::AddGeneratedTilesAndGetUpdatedTiles()`: `dtNavMesh::addTile()` 호출

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:55` — `FRecastBuildConfig` 구조체: UE 추가 멤버(`AgentHeight`, `SkipNavGeometry`, `borderSize` 등) 선언
> - `Engine/Source/Runtime/Navmesh/Public/Recast/Recast.h` — `rcConfig` 원본 구조체: `cs`, `ch`, `walkableHeight`, `maxVertsPerPoly` 등 멤버 정의
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastNavMeshGenerator::Init()` 또는 빌드 설정 초기화 함수: `FRecastBuildConfig`에 값 채우는 로직

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h` — `FNavTileRef` 구조체 선언: `TileDataId` 멤버
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtNavMesh::decodePolyId()`: 타일 X, Y, Layer, 폴리곤 인덱스 비트 분리 방법

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:502` — `dtNavMesh` 클래스 선언 전체: `addTile()`, `removeTile()`, `getTileAt()`, `calcTileLoc()` 등
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtPolyRef`, `dtTileRef` typedef, `DT_VERTS_PER_POLYGON`, `DT_MAX_AREAS` 상수 정의
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMesh.cpp` — `dtNavMesh::addTile()`: 타일 메모리 블록 파싱 및 내부 링크 생성 구현
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMesh.cpp` — `dtNavMesh::removeTile()`: 타일 참조 해제 및 메모리 반환 구현

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:421` — `dtMeshTile` 구조체 선언: 모든 포인터 멤버와 `data` 블록 소유권 관계
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtMeshHeader` 구조체: `polyCount`, `vertCount`, `maxLinkCount`, `detailMeshCount` 등 타일 크기 메타데이터
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMesh.cpp` — `dtNavMesh::addTile()`: `data` 블록 포인터를 파싱하여 각 멤버에 할당하는 로직

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:205` — `dtPoly` 구조체: `areaAndtype` 비트 레이아웃, `getArea()` / `getType()` 인라인 메서드
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` — `dtPolyTypes` 열거형: `DT_POLYTYPE_GROUND`, `DT_POLYTYPE_OFFMESH_POINT` 값 정의
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastTileGenerator::ApplyModifiers()`: `dtPoly::setArea()`를 통해 Nav Modifier 영역 ID 적용

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h:252` — `dtLink` 구조체: `ref`, `side`, `bmin`, `bmax` 멤버 정의
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMesh.cpp` — `dtNavMesh::connectExtLinks()`: 타일 경계에서 인접 타일과 `dtLink`를 생성하는 로직 (portal 연결)
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMesh.cpp` — `dtNavMesh::addTile()`: 내부 폴리곤 링크(`DT_NULL_LINK` 초기화 → 링크 할당) 설정

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

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMeshQuery.h` — `dtNavMeshQuery` 클래스 선언: `findNearestPoly()`, `findPath()`, `findStraightPath()`, `raycast()`, `moveAlongSurface()` 시그니처
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMeshQuery.cpp` — `dtNavMeshQuery::findPath()`: A* 알고리즘 구현, `m_nodePool` 노드 풀 사용
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMeshQuery.cpp` — `dtNavMeshQuery::findStraightPath()`: 폴리곤 경로 → 월드 좌표 직선 경로 변환
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/PImplRecastNavMesh.cpp` — `FPImplRecastNavMesh::FindPath()`: `dtNavMeshQuery`를 풀에서 꺼내 `findPath()` → `findStraightPath()` 호출하는 UE 래퍼

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

---

## 4. 클래스 간 데이터 관계도

### 4-1. 소유·참조 관계

```
AActor
   ↑ (상속)
ANavigationData (abstract)  ────┐
   ↑ (상속)                      │ UNavigationSystemV1.NavDataSet[] 로 보관
ARecastNavMesh  ◀────────────────┘
   │
   ├─ owns ─▶ FPImplRecastNavMesh (PIMPL)
   │            │
   │            ├─ owns ─▶ dtNavMesh*                     (타일 그래프)
   │            │            │
   │            │            └─ owns ─▶ dtMeshTile[]      (개별 타일)
   │            │                          │
   │            │                          ├─ dtPoly[]    (볼록 다각형)
   │            │                          ├─ dtLink[]    (폴리곤 간 링크)
   │            │                          ├─ float verts[] (버텍스)
   │            │                          └─ dtBVNode[]  (가속 트리)
   │            │
   │            ├─ shared ─▶ dtNavMeshQuery (게임 스레드용 SharedNavQuery)
   │            │
   │            └─ TMap ──▶ CompressedTileCacheLayers     (복셀 레이어 캐시)
   │                           ↓
   │                           dtTileCacheLayer (TileX,TileY 별)
   │
   ├─ owns ─▶ TUniquePtr<FNavDataGenerator>
   │            └─ FRecastNavMeshGenerator
   │                 ├─ refs ──▶ ARecastNavMesh (DestNavMesh)
   │                 ├─ holds ──▶ FRecastBuildConfig (rcConfig 확장)
   │                 ├─ holds ──▶ InclusionBounds / ExclusionBounds
   │                 ├─ queue ──▶ PendingDirtyTiles[]
   │                 └─ queue ──▶ RunningDirtyTiles[]
   │                                 └─ FRecastTileGenerator
   │                                       ├─ holds ──▶ RawGeometry[]
   │                                       ├─ holds ──▶ Modifiers[]
   │                                       ├─ holds ──▶ rcHeightfield*
   │                                       └─ produces ──▶ FNavMeshTileData[]
   │                                                         ↓ addTile()
   │                                                         dtMeshTile
   │
   └─ owns ─▶ FRecastNavMeshCachedData (Area/Filter 캐시)

UNavigationSystemV1 (UWorld당 1개)
   │
   ├─ holds ──▶ TArray<ANavigationData*> NavDataSet       (에이전트별 여러 개)
   ├─ holds ──▶ TArray<FNavDataConfig> SupportedAgents
   ├─ holds ──▶ FNavigationOctree                         (공간 분할)
   │              └─ TOctree2<FNavigationOctreeElement>
   │                    └─ FNavigationRelevantData
   │                          ├─ Modifiers (FAreaNavModifier[])
   │                          ├─ NavLinks (FSimpleLinkNavModifier[])
   │                          └─ HasGeometry / HasDynamicModifiers
   │
   └─ holds ──▶ FNavigationDirtyAreasController
                  └─ TArray<FNavigationDirtyArea>
                        ├─ FBox Bounds
                        └─ ENavigationDirtyFlag Flags

URecastNavMeshDataChunk (스트리밍 단위)
   └─ AttachTiles/DetachTiles ──▶ ARecastNavMesh
```

### 4-2. Area ID / NavArea 클래스 간 연결

```
UNavArea (C++ 클래스)                  ──Registered──▶ UNavigationSystemV1.RegisteredNavAreaClasses
   │ StaticClass / DefaultCost               │
   ↓                                          ↓
FAreaNavModifier.AreaClass ────────▶ Area ID (0-63) ────▶ dtPoly.areaAndtype (6비트)
   (NavModifierVolume, NavModifierComponent)       │
                                                   ↓
                                     dtQueryFilter::m_areaCost[ID] ──▶ A* 비용
```

각 `UNavArea` 서브클래스는 등록 시 0-63 범위의 Area ID를 할당받고, 이 ID가 `dtPoly::areaAndtype` 하위 6비트에 저장됩니다. 쿼리 시 `dtQueryFilter`는 Area ID별 비용을 룩업하여 A\* 경로 비용에 반영합니다.

### 4-3. Dirty Flag → 빌드 경로 매핑

```
FNavigationRelevantData.GetDirtyFlag()
                ↓
        FNavigationDirtyArea.Flags
                ↓
FRecastNavMeshGenerator::MarkDirtyTiles()
                ↓
     ┌──────────────────────┬───────────────────────┐
     ↓                      ↓                       ↓
  Geometry             DynamicModifier         NavigationBounds
  (+IsGameStaticNavMesh면 skip)  │                   │
     ↓                      ↓                       ↓
FRecastTileGenerator    CompressedTileCache 경로   전체 재빌드
  전체 파이프라인         (복셀 재사용)              + Bounds 갱신
  (GatherGeometry → ...)     ↓
                          MarkDynamicAreas()
                          (Area ID만 재할당)
```

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h:316,413,427` — `UNavigationSystemV1`, `SupportedAgents`, `NavDataSet`
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h:573,1572` — `ARecastNavMesh`, `RecastNavMeshImpl` 포인터
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/PImplRecastNavMesh.h:271-284` — `FPImplRecastNavMesh` 멤버 (소유 관계)
> - `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h:361,784` — 빌드 측 관계
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationOctree.h:172` — `FNavigationOctree` 선언
> - `Engine/Source/Runtime/NavigationSystem/Private/NavigationDirtyAreasController.cpp` — Dirty Area 흐름

---

> **소스 확인 위치**
> - `Engine/Source/Runtime/NavigationSystem/Public/NavAreas/NavArea.h` — `UNavArea` 클래스: `DefaultCost`, `DrawColor`, `SupportedAgentsBitmask` UPROPERTY 선언
> - `Engine/Source/Runtime/NavigationSystem/Public/NavAreas/NavArea_Default.h` — `UNavArea_Default` (Area ID 63, DefaultCost 1.0)
> - `Engine/Source/Runtime/NavigationSystem/Public/NavAreas/NavArea_Null.h` — `UNavArea_Null` (Area ID 0, DefaultCost FLT_MAX)
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h` — `FAreaNavModifier`, `FNavigationRelevantData`, `ENavigationDirtyFlag` 선언
> - `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h:135` — `FNavigationRelevantData::GetDirtyFlag()`: `HasDynamicModifiers()` 분기로 `DynamicModifier` 플래그 결정 로직
> - `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` — `FRecastTileGenerator::ApplyModifiers()`: `FAreaNavModifier` 목록을 순회하여 `dtPoly::setArea()` 적용
