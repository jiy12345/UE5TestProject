# 5. 재현 조건 및 다음 단계

> **작성일**: 2026-02-18 | **엔진 버전**: UE 5.7

## 버그 재현을 위한 조건

현재 테스트 프로젝트에서 버그를 안정적으로 재현하려면, **DL 스트리밍 셀에 geometry가 포함되도록** 레벨을 수정해야 함.

### 재현 방법

1. `L_NavModifierTest` 레벨에서 **DataLayer 레이어** 안에 StaticMeshActor (geometry) 추가
2. NavMesh 재빌드 (DataLayer 비활성 상태로 빌드하면 DL 셀에 NavDataChunk 생성 안 됨)
3. **중요**: NavMesh 빌드 시 DataLayer를 **활성화한 상태**로 빌드해야 NavDataChunkActor에 DL 타일이 포함됨
4. 쿠킹 후 실행
5. 예상 결과: `AttachTiles "removing"` 로그 확인 → `[VERIFY] FAIL`

## 다음 확인 단계

### Step 1: 기존 로그에서 AttachTiles 확인

기존 쿠킹 빌드 로그(`StagedBuilds/.../UE5TestProject.log`)에서:

```bash
# "removing" 엔트리 검색
grep -i "removing" UE5TestProject.log | grep -i "AttachTiles\|DataChunk\|tile"

# NavDebug-D 이후 removing이 오는지 확인
grep -E "(NavDebug-D|NavDebug-F|removing)" UE5TestProject.log
```

### Step 2: VeryVerbose 로그 재확인

`LogNavigation=VeryVerbose`는 이미 설정됨. 기존 StagedBuild 로그에 이미 포함되어 있을 수 있음.

**찾아야 할 패턴**:
```
LogNavigation: VeryVerbose: URecastNavMeshDataChunk::AttachTiles  removing tile at [X, Y, L]
```
이 로그가 `[NavDebug-F] MarkDynamicAreas Modifiers=1` **이후**에 나타나면 가설 확증.

### Step 3: 타이밍 검증

```
이상적인 (버그) 시나리오:
    t=5.0s  DataLayer 활성화
    t=5.1s  NavModifier octree 등록 → DirtyArea → MarkDirtyTiles PASSED
    t=5.2s  TileGen 실행 → Modifiers=1 → MarkDynamicAreas → modifier 적용 완료
    t=5.3s  AttachTiles "removing" ← modifier 적용된 타일이 덮어써짐!
    t=10s   VERIFY → PathPoints=2 → FAIL
```

## 수정 방향 (가설)

### 방향 A: AttachTiles 후 dirty area 생성

`AttachNavMeshDataChunk`에서 `AttachTiles` 완료 후, 영향받은 타일들에 대해 modifier dirty area를 생성:

```cpp
void ARecastNavMesh::AttachNavMeshDataChunk(URecastNavMeshDataChunk& DataChunk)
{
    DataChunk.AttachTiles(*this);
    InvalidateAffectedPaths(DataChunk.GetTileRefs());

    // ★ 추가: 영향받은 타일들에 modifier dirty area 생성
    // → TileGen이 재실행되어 modifier 재적용
    RequestDynamicModifiersDirtyArea(affectedTileBounds);
}
```

### 방향 B: AttachTiles 시 기존 modifier 재적용

`AttachTiles`에서 pre-baked 타일로 교체 후, 해당 타일에 현재 octree의 modifier를 즉시 재적용.

### 방향 C: 레이어 활성화 순서 변경

DataLayer 활성화 이벤트 핸들링에서, NavDataChunkActor가 먼저 로드(AttachTiles 완료)된 **이후에** NavModifier를 등록하도록 순서 보장.

### 방향 D: NavDataChunkActor에 modifier 데이터 제외

NavDataChunkActor에 저장되는 타일 중 modifier에 의해 수정된 부분은 저장하지 않고, 항상 base geometry만 저장. 그 후 런타임에 modifier를 적용.

## MONITOR 코드 버그 수정 완료

**파일**: `Game/Source/UE5TestProject/NavTestGameMode.cpp`

**문제**: `PrimaryActorTick.bCanEverTick = true`를 `Super::BeginPlay()` 이후에 설정 → Tick() 미등록 → TickMonitor() 미실행 → MONITOR 로그 전혀 없음

**수정**:
```cpp
// 이전 (잘못됨)
void ANavTestGameMode::BeginPlay() {
    Super::BeginPlay();
    PrimaryActorTick.bCanEverTick = true;  // ← Super 이후: Tick 등록 안 됨

// 수정 후
void ANavTestGameMode::BeginPlay() {
    PrimaryActorTick.bCanEverTick = true;  // ← Super 이전: 정상 등록
    Super::BeginPlay();
```

## 새로운 핵심 가설: DL 런타임 STATE 자체 변경

### 발견 내용
단순히 스트리밍 셀이 언로드되는 것이 아니라, **DL 런타임 STATE 자체**가 Activated → Unloaded → Activated로 순환됩니다.

### 두 시나리오 구분

| 시나리오 | DL 상태 | 현상 |
|----------|---------|------|
| **테스트 프로젝트** (플레이어가 DL 셀 경계 근처) | Activated ↔ Unloaded 반복 | 간헐적 modifier 적용 |
| **원본 프로젝트** (플레이어가 DL 셀에서 멀리) | 항상 Unloaded 유지 | 전혀 효과 없음 |

### 테스트 프로젝트 타임라인
```
t= 0.20s  SetDataLayerRuntimeState(Activated) → Modifiers=1
t= 5.19s  VERIFY → ALL PASS ✅ (modifier 적용됨)
t=24.0s   DL: Activated → Unloaded (WP 스트리밍 정책?)
          NavModifiers octree 해제 → TileGen Modifiers=0 → 경로 직선
t=27.2s   DL: Unloaded → Activated
          NavModifiers 재등록 → TileGen Modifiers=1 → 경로 우회
t=32.5s   DL: Activated → Unloaded 또 반복
t=34s     게임 종료
```

## 현재 진행 상태

- [x] 버그 현상 파악 및 테스트 환경 구성 (`NavTestGameMode`)
- [x] NavDebug 로그 추가 및 쿠킹 빌드 완료
- [x] 로그 분석으로 `Modifiers=0/1 진동` 현상 확인
- [x] 잘못된 가설(bExcludeLoadedData) 기각
- [x] `AttachTiles 오버라이트 가설` 수립
- [x] AttachTiles 로그: 테스트 프로젝트에는 NavDataChunkActor 없음 → AttachTiles 호출 안 됨 확인
- [x] DL 런타임 STATE 자체가 Activated ↔ Unloaded 순환 발견
- [x] MONITOR 코드 버그 수정 (`bCanEverTick` → `Super::BeginPlay()` 이전으로 이동)
- [ ] NavTestGameMode 빌드 후 재쿠킹, 재실행 → MONITOR PathPoints 변화 확인
- [ ] DL 상태 Unloaded 변경 호출자 확정 (WP 내부 메커니즘 or Blueprint)
- [ ] 원본 프로젝트의 "항상 Unloaded" 상황 재현: 플레이어 스폰을 DL 셀과 멀리 배치
- [ ] (선택) DL 셀에 geometry 추가하여 AttachTiles 동작 추가 검증

## 참고: 주요 소스 파일 경로

```
Engine/Engine/Source/Runtime/NavigationSystem/Private/NavMesh/
├── RecastNavMeshGenerator.cpp  # MarkDirtyTiles(~6600), TileGen::Setup(~1800), MarkDynamicAreas(~4577)
├── RecastNavMesh.cpp           # AttachNavMeshDataChunk(~3263), OnStreamingNavDataAdded(~3188)
└── RecastNavMeshDataChunk.cpp  # AttachTiles(~215), DetachTiles(~359)
```
