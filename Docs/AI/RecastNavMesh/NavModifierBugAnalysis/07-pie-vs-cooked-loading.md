# PIE vs 쿠킹 빌드: 액터 로딩 방식 차이 심층 분석

> **작성일**: 2026-02-15
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#9](https://github.com/jiy12345/UE5TestProject/issues/9)

---

## 개요

이 문서는 PIE(Play In Editor)와 쿠킹 빌드(Packaged/Cooked Build)에서 **액터가 로드되고 초기화되는 방식의 차이**를 분석합니다. 특히 World Partition 환경에서 **액터 타입별 로딩 차이**가 네비게이션 시스템에 미치는 영향을 중점적으로 다룹니다.

---

## 1. 월드 초기화 시퀀스 (공통)

PIE와 쿠킹 빌드 모두 동일한 `InitializeActorsForPlay()` 시퀀스를 따릅니다.

**소스**: `World.cpp:5842-5934`

```
UWorld::InitializeActorsForPlay()
  │
  ├─ [1] UpdateWorldComponents(bRerunConstructionScript, ...)    ← ★ PIE/쿠킹 분기점
  │
  ├─ [2] ULevel::InitializeNetworkActors()                      ← 네트워크 액터 초기화
  │
  ├─ [3] ULevel::RouteActorInitialize()
  │      ├─ Phase 1: PreInitializeComponents()                   ← 모든 액터
  │      ├─ Phase 2: InitializeComponents()                      ← 컴포넌트 등록
  │      │           PostInitializeComponents()                  ← 초기화 완료
  │      └─ Phase 3: BeginPlay()                                 ← 게임 로직 시작
  │
  ├─ [4] NavigationSystem::OnWorldInitDone()
  │      ├─ PendingNavBoundsUpdates.Empty()                      ← ★ 초기화 중 누적된 업데이트 폐기
  │      ├─ GatherNavigationBounds()
  │      └─ RegisterNavigationDataInstances()
  │
  └─ [5] World::BeginPlay() → 각 액터 BeginPlay 호출
```

---

## 2. PIE vs 쿠킹 빌드: 핵심 차이점

### 2.1 Construction Script 실행 차이

**소스**: `World.cpp:5876-5879`

```cpp
// 쿠킹 데이터가 있거나, 게임 월드이면서 이미 Construction Script 실행했거나 PIE 복제된 경우 → false
const bool bRerunConstructionScript = !(
    FPlatformProperties::RequiresCookedData() ||
    (IsGameWorld() && (PersistentLevel->bHasRerunConstructionScripts ||
                       PersistentLevel->bWasDuplicatedForPIE))
);
UpdateWorldComponents(bRerunConstructionScript, true, Context);
```

| 환경 | `bRerunConstructionScript` | 설명 |
|------|---------------------------|------|
| **PIE (일반)** | `false` | `bWasDuplicatedForPIE = true` → 이미 에디터에서 실행됨 |
| **PIE (디스크 로드)** | `true` | 디스크에서 로드한 PIE 월드 → 재실행 필요 |
| **쿠킹 빌드** | `false` | `RequiresCookedData() = true` → 재실행 안 함 |

**영향**: Construction Script에서 컴포넌트를 생성/등록하는 경우, 쿠킹 빌드에서는 이 단계가 생략됨. 다만 일반적인 PIE에서도 `bWasDuplicatedForPIE`로 인해 재실행되지 않으므로, 이 차이 자체가 Nav Modifier 문제의 직접 원인은 아님.

### 2.2 World Partition 초기화 차이 (가장 핵심적인 차이)

**소스**: `WorldPartition.cpp:449-479, 666-868`

#### PIE 전용: PrepareEditorGameWorld()

PIE 시작 시 `PrepareEditorGameWorld()`가 호출되어 **스트리밍 셀을 메모리 패키지로 생성**합니다:

```cpp
void UWorldPartition::PrepareEditorGameWorld()
{
    bIsPIE = GIsEditor && !IsRunningGame();

    // 스트리밍 생성 (셀 할당, 패키지 생성)
    GenerateStreaming(Params, Context);

    // 메모리 패키지로 변환 (디스크 I/O 없이 즉시 접근 가능)
    for (const FString& PackageName : OutGeneratedLevelStreamingPackageNames)
    {
        FString Package = TEXT("/Memory/") + PackageName;
        GeneratedLevelStreamingPackageNames.Add(Package);
    }

    RuntimeHash->PrepareEditorGameWorld();          // 액터 참조 강제 로드
    ExternalDataLayerManager->PrepareEditorGameWorld(); // 외부 데이터 레이어 준비
}
```

**소스**: `WorldPartitionLevelStreamingPolicy.cpp:68-72`

```cpp
if (InWorld->IsPlayInEditor() || InWorld->IsPlayInPreview())
{
    // 메모리 패키지로 셀 생성 → 디스크 체크 생략 → 즉시 로딩 가능
    return FString::Printf(TEXT("/Memory/%s_%s"),
        *FPackageName::GetShortName(InWorld->GetPackage()), *InCellName.ToString());
}
```

#### 쿠킹 빌드: 디스크 기반 스트리밍

쿠킹 빌드에서는 `PrepareEditorGameWorld()`가 호출되지 않으며, 스트리밍 셀은 쿠킹된 패키지(.uasset)에서 로드됩니다.

### 2.3 Always-Loaded 액터 처리 차이

**소스**: `WorldPartitionRuntimeHash.cpp:332-388`

| 환경 | Always-Loaded 액터 처리 |
|------|------------------------|
| **PIE** | `ForceExternalActorLevelReference(true)` → PersistentLevel에 직접 로드, 월드 복제 시 함께 복제 |
| **쿠킹 빌드** | Always-Loaded 셀로 패키징 → 초기 스트리밍에서 로드 |

### 2.4 Navigation System 초기화 타이밍 차이

**소스**: `NavigationSystem.cpp:1358-1370`

```cpp
// 게임 월드 초기화 시 (PIE 포함)
PendingNavBoundsUpdates.Empty();  // ← 초기화 중 누적된 모든 바운드 업데이트 폐기!
GatherNavigationBounds();          // ← 현재 존재하는 바운드만 수집
RegisterNavigationDataInstances(); // ← 현재 등록된 네비 데이터만 처리
```

이 시점에서:
- **PIE**: 에디터에서 빌드한 네비메시가 이미 완전한 상태. Always-loaded 액터가 이미 PersistentLevel에 존재
- **쿠킹 빌드**: 스트리밍 셀이 아직 로드되지 않았을 수 있음. NavDataChunkActor도 스트리밍 대상

---

## 3. 액터 타입별 로딩 차이

### 3.1 분류 기준: bIsSpatiallyLoaded

**소스**: `Actor.h:1292-1297`

```cpp
/**
 * If true, this actor will be loaded when in the range of any streaming sources
 *     and if (1) in no data layers, or (2) one or more of its data layers are enabled.
 * If false, this actor will be loaded if (1) in no data layers,
 *     or (2) one or more of its data layers are enabled.
 */
uint8 bIsSpatiallyLoaded : 1;
```

| bIsSpatiallyLoaded | 로딩 조건 | 예시 |
|--------------------|----------|------|
| `true` | 스트리밍 소스 범위 내 + 데이터 레이어 활성 | 일반 액터, Nav Modifier Volume |
| `false` | 데이터 레이어 활성만으로 로드 (거리 무관) | 매니저 클래스, 글로벌 설정 |

### 3.2 NavigationDataChunkActor

**소스**: `NavigationDataChunkActor.h:12-63`

```
타입: APartitionActor 상속
bIsSpatiallyLoaded: 변경 불가 (CanChangeIsSpatiallyLoadedFlag() = false)
초기화 트리거: BeginPlay() → AddNavigationDataChunkToWorld()
```

| 항목 | PIE | 쿠킹 빌드 |
|------|-----|-----------|
| 셀 할당 | 공간 기반 (그리드 좌표) | 공간 기반 (그리드 좌표) |
| 로딩 시점 | 에디터에서 이미 존재, PIE 시작 시 즉시 사용 가능 | 스트리밍 셀 범위 진입 시 비동기 로드 |
| BeginPlay 타이밍 | 월드 초기화 직후 | 스트리밍 완료 후 |
| 타일 캐시 상태 | 에디터 빌드 결과 이미 존재 | BeginPlay에서 AttachTiles 호출 후 생성 |

**핵심**: NavigationDataChunkActor는 **공간 기반 스트리밍**이므로, 특정 영역의 타일 캐시는 해당 스트리밍 셀이 로드될 때까지 존재하지 않음.

### 3.3 NavModifierVolume / NavModifierComponent가 있는 액터

```
타입: AActor (일반) 또는 AVolume 상속
bIsSpatiallyLoaded: 기본 true (변경 가능)
초기화 트리거: OnRegister() → FNavigationSystem::OnComponentRegistered()
```

| 항목 | PIE | 쿠킹 빌드 |
|------|-----|-----------|
| 셀 할당 | 공간 기반 또는 데이터 레이어 기반 | 공간 기반 또는 데이터 레이어 기반 |
| 로딩 시점 | 에디터에서 이미 존재, PIE 시 즉시 등록 | 셀 스트리밍 또는 데이터 레이어 활성화 시 |
| 네비 등록 | OnRegister → Nav 옥트리에 추가 | OnRegister → Nav 옥트리에 추가 |
| bLoadedData | true (visibility change) | true (visibility change) |

**핵심**: 데이터 레이어에 속한 Nav Modifier는 데이터 레이어 활성화 시 로드되며, 이는 NavigationDataChunkActor의 스트리밍 셀 로드와 **독립적인 타이밍**에 발생.

### 3.4 일반 게임플레이 액터 (Character, Pawn 등)

```
타입: ACharacter, APawn, AActor 등
bIsSpatiallyLoaded: true (기본)
초기화 트리거: 표준 BeginPlay 시퀀스
```

| 항목 | PIE | 쿠킹 빌드 |
|------|-----|-----------|
| 로딩 | 에디터 월드 복제 | 스트리밍 셀 기반 |
| 컴포넌트 등록 | 즉시 | 셀 로드 완료 후 |
| 네비 영향 | 없음 (네비 관련 컴포넌트 없는 경우) | 없음 |

### 3.5 WorldPartitionHLOD 액터

```
타입: AWorldPartitionHLOD
bIsSpatiallyLoaded: true
NeedsLoadForServer: GetIsReplicated()에 따라 결정
```

HLOD 액터는 서버/클라이언트 로딩 조건이 다르며, 네비게이션과 직접적 관련은 없음.

### 3.6 데이터 레이어 내 액터 (핵심)

데이터 레이어에 할당된 액터의 로딩 흐름:

```
데이터 레이어 활성화 (SetDataLayerRuntimeState)
  └→ 해당 데이터 레이어의 스트리밍 셀 활성화
      └→ 셀 내 액터 로딩 (비동기)
          └→ 레벨 visibility change 발생
              └→ RouteActorInitialize
                  ├→ PreInitializeComponents
                  ├→ InitializeComponents / PostInitializeComponents
                  │   └→ OnRegister() → Nav 옥트리 등록 (bLoadedData=true)
                  └→ BeginPlay
```

| 항목 | PIE | 쿠킹 빌드 |
|------|-----|-----------|
| 데이터 레이어 로딩 | PersistentLevel에 직접 로드 | 스트리밍 레벨로 비동기 로드 |
| visibility change | 발생 | 발생 |
| bLoadedData | true | true |
| 네비 타일 캐시 | 에디터 빌드 결과 이미 존재 | 동일 영역 NavDataChunkActor 로드에 의존 |

---

## 4. 로딩 순서 타이밍 다이어그램

### 4.1 PIE 타이밍

```
t=0   에디터 월드 복제 (모든 PIE 레벨 데이터 메모리에 존재)
      ├─ PersistentLevel의 모든 액터 복제됨
      ├─ Always-loaded 액터 PersistentLevel에 참조
      └─ CompressedTileCacheLayers: 에디터 빌드 결과 이미 채워짐 ✓

t=1   InitializeActorsForPlay()
      ├─ UpdateWorldComponents()
      ├─ RouteActorInitialize() → 모든 Persistent 액터 초기화
      └─ NavigationSystem::OnWorldInitDone()
          └─ 네비메시: 에디터 빌드 결과 그대로 사용 ✓

t=2   World::BeginPlay()
      └─ 스트리밍 셀 활성화 시작

t=3+  데이터 레이어 활성화/비활성화
      └─ Nav Modifier dirty area 생성
          └─ GetTileCacheLayers() → 이미 존재 ✓
          └─ 모디파이어 정상 적용 ✓
```

### 4.2 쿠킹 빌드 타이밍

```
t=0   맵 로드 (PersistentLevel만 로드)
      └─ CompressedTileCacheLayers: 비어있음 ✗

t=1   InitializeActorsForPlay()
      ├─ UpdateWorldComponents(bRerunConstructionScript=false)
      ├─ RouteActorInitialize() → Persistent 액터만 초기화
      └─ NavigationSystem::OnWorldInitDone()
          ├─ PendingNavBoundsUpdates.Empty() ← 누적 업데이트 폐기
          └─ 네비메시: 아직 스트리밍 안 된 영역은 타일 없음

t=2   World::BeginPlay()
      └─ 스트리밍 시작

t=3   [비동기] 스트리밍 셀 A 로드
      └─ NavigationDataChunkActor A의 BeginPlay
          └─ AttachTiles() → CompressedTileCacheLayers 채움 ✓
          └─ OnStreamingNavDataAdded() → 기존 modifier 재처리 시도

t=5   [비동기] 데이터 레이어 활성화
      └─ Nav Modifier 액터 로드 (셀 B에 속함)
          └─ OnRegister() → Nav 옥트리 등록 (bLoadedData=true)
          └─ dirty area 생성
              └─ GetTileCacheLayers() → ???
                  ├─ 셀 A 영역: 이미 로드됨 → 캐시 존재 ✓
                  └─ 셀 C 영역: 아직 미로드 → 캐시 없음 ✗

t=7   [비동기] 스트리밍 셀 C 로드
      └─ NavigationDataChunkActor C의 BeginPlay
          └─ AttachTiles() → CompressedTileCacheLayers 채움 ✓
          └─ OnStreamingNavDataAdded()
              └─ FindElementsInNavOctree(bExcludeLoadedData=true)
                  └─ Nav Modifier의 bLoadedData=true → 제외됨 ✗
                      → 재처리 안 됨! ❌
```

---

## 5. 동일 스트리밍 셀 vs 다른 스트리밍 셀

### 5.1 Nav Modifier와 NavDataChunkActor가 같은 셀에 있는 경우

이 경우 동일한 레벨에 속하므로 `RouteActorInitialize()`에서 순차적으로 초기화됩니다:

```
셀 로드 완료 → RouteActorInitialize()
  ├─ 액터 순서대로 PreInitializeComponents
  ├─ 액터 순서대로 InitializeComponents + PostInitializeComponents
  │   └─ NavModifier: OnRegister → Nav 옥트리 등록
  └─ 액터 순서대로 BeginPlay
      └─ NavigationDataChunkActor: AttachTiles → OnStreamingNavDataAdded
```

**중요**: `BeginPlay` 단계에서 NavigationDataChunkActor가 `AttachTiles`를 호출하고, 그 직후 `OnStreamingNavDataAdded()`가 `FindElementsInNavOctree()`로 같은 셀의 Nav Modifier를 찾습니다.

하지만 `bExcludeLoadedData=true` 필터 때문에 `bLoadedData=true`인 Nav Modifier는 **여전히 검색에서 제외**됩니다. 다만 이 경우 Nav Modifier의 dirty area가 `InitializeComponents` 단계에서 이미 생성되었고, BeginPlay에서 AttachTiles가 호출되므로:

- dirty area 처리가 **같은 프레임** 내 또는 **다음 tick**에서 발생
- AttachTiles가 먼저 완료되었으면 → 타일 캐시 존재 → 정상 동작 가능
- dirty area가 먼저 처리되면 → 타일 캐시 부재 → 문제 발생

### 5.2 Nav Modifier와 NavDataChunkActor가 다른 셀에 있는 경우

이것이 **가장 문제가 되는 시나리오**입니다:

```
[시점 A] 데이터 레이어 활성화 → Nav Modifier 로드 (셀 X)
  └─ dirty area 생성
  └─ 해당 영역의 NavDataChunkActor는 셀 Y에 속함 → 아직 미로드
  └─ GetTileCacheLayers() → 빈 배열 → 빈 타일 생성 ❌

[시점 B] 스트리밍 셀 Y 로드 → NavDataChunkActor 로드
  └─ AttachTiles → 타일 캐시 채움
  └─ OnStreamingNavDataAdded → bExcludeLoadedData → Nav Modifier 제외 ❌
```

---

## 6. 액터 타입별 요약 테이블

| 액터 타입 | 상위 클래스 | bIsSpatiallyLoaded | 네비 영향 | PIE 로딩 | 쿠킹 로딩 |
|-----------|------------|-------------------|----------|---------|----------|
| NavigationDataChunkActor | APartitionActor | 변경 불가 (true) | 타일 캐시 제공 | 에디터 데이터 즉시 사용 | 셀 스트리밍 시 BeginPlay |
| NavModifierVolume | AVolume | true (기본) | DynamicModifier dirty | 에디터에서 이미 등록 | 셀/데이터레이어 로드 시 등록 |
| NavModifierComponent | UActorComponent | (소유 액터에 따름) | DynamicModifier dirty | 에디터에서 이미 등록 | 소유 액터 로드 시 OnRegister |
| RecastNavMesh | ANavigationData | false (Always loaded) | 네비메시 자체 | 즉시 | 즉시 |
| NavMeshBoundsVolume | AVolume | true | 네비 바운드 정의 | 에디터에서 존재 | 셀 스트리밍 시 |
| 일반 Actor (Static Mesh 등) | AActor | true (기본) | Geometry dirty | 에디터 존재 | 셀 스트리밍 시 |
| HLOD Actor | AWorldPartitionHLOD | true | 없음 (일반적) | 에디터에서 처리 | 셀 스트리밍 시 |
| GameMode/GameState | AInfo | false | 없음 | 즉시 | 즉시 |

---

## 7. Nav Modifier 문제와의 연결

### 왜 PIE에서는 동작하는가?

1. **에디터 빌드 결과가 완전**: 에디터에서 네비메시를 빌드할 때 모든 Nav Modifier가 이미 존재하므로, CompressedTileCacheLayers에 모디파이어 효과가 포함됨
2. **데이터 레이어 토글 시**: 이미 캐시가 존재하는 상태에서 모디파이어를 적용/해제하므로 `bRegenerateCompressedLayers = false` → 기존 캐시 위에 모디파이어 오버레이

### 왜 쿠킹 빌드에서 실패하는가?

1. **로딩 순서 비결정적**: Nav Modifier와 NavigationDataChunkActor의 로딩 순서가 보장되지 않음
2. **타일 캐시 부재**: Nav Modifier dirty area 처리 시 CompressedTileCacheLayers가 아직 채워지지 않았을 수 있음
3. **재처리 메커니즘 부재**: `bExcludeLoadedData` 필터로 인해 나중에 타일 캐시가 채워져도 Nav Modifier가 재처리되지 않음
4. **독립적 스트리밍**: 데이터 레이어와 공간 기반 스트리밍은 완전히 독립적인 시스템으로, 상호 의존성이 고려되지 않음

---

## 8. 관련 소스 파일

| 파일 | 핵심 라인 | 내용 |
|------|-----------|------|
| `World.cpp` | 5842-5934 | InitializeActorsForPlay — 초기화 시퀀스 |
| `World.cpp` | 5876-5879 | bRerunConstructionScript 결정 |
| `Level.cpp` | 3817-3908 | RouteActorInitialize — 3단계 초기화 |
| `WorldPartition.cpp` | 449-479 | PrepareEditorGameWorld — PIE 전용 |
| `WorldPartition.cpp` | 666-868 | Initialize — PIE/쿠킹 분기 |
| `WorldPartitionRuntimeHash.cpp` | 303-388 | PrepareEditorGameWorld, PopulateCellActorInstances |
| `WorldPartitionLevelStreamingPolicy.cpp` | 68-72 | PIE 메모리 패키지 |
| `WorldPartitionStreamingPolicy.cpp` | 273-390 | UpdateStreamingState |
| `RuntimeSpatialHashGridHelper.cpp` | 204-242 | 셀 할당 로직 |
| `NavigationDataChunkActor.h` | 12-63 | NavigationDataChunkActor 클래스 정의 |
| `NavigationDataChunkActor.cpp` | 102-125 | BeginPlay, AddNavigationDataChunkToWorld |
| `NavigationSystem.cpp` | 1358-1370 | OnWorldInitDone — PendingNavBoundsUpdates 폐기 |
| `Actor.h` | 1292-1297 | bIsSpatiallyLoaded 정의 |
| `DataLayerManager.h` | 73-95 | SetDataLayerRuntimeState |
