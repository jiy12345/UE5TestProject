# 3. 로그 분석 및 재현

> **작성일**: 2026-02-18 | **엔진 버전**: UE 5.7

## 추가한 NavDebug 로그

이전 세션에서 `RecastNavMeshGenerator.cpp`에 다음 NavDebug 로그를 추가하고 빌드함.

### 추가 위치 및 내용

| 위치 (approx line) | 태그 | 내용 |
|--------------------|------|------|
| ~1817 | `[NavDebug-A]` | TileGen::Setup 상태: CompressedLayers 수, bGeometryChanged, bRegenerateCompressedLayers |
| ~1835 | `[NavDebug-B]` | IntersectCheck: dirty area와 타일 교차 여부 |
| ~1846 | `[NavDebug-C]` | DirtyLayers 요약: Total/Dirty 수 |
| ~1864 | `[NavDebug-D]` | AfterGatherGeometry: Modifiers 수 |
| ~1872 | `[NavDebug-E]` | AfterPrepareGeometrySources |
| ~4577 | `[NavDebug-F]` | MarkDynamicAreas: 적용한 Modifiers 수 |
| ~6617 | `[NavDebug-FILTER]` | MarkDirtyTiles: FILTERED (통과 못한 dirty area) |
| ~6628 | `[NavDebug-FILTER]` | MarkDirtyTiles: PASSED (통과한 dirty area) |

## DefaultEngine.ini 로그 설정

```ini
[Core.Log]
LogNavigationDirtyArea=VeryVerbose
LogNavigation=VeryVerbose
LogNavOctree=Verbose
LogWorldPartition=Verbose
LogStreaming=Log
```

`LogNavigation=VeryVerbose`이면 `AttachTiles` 내부의 "removing" 로그도 출력됨.

## 테스트 1차 결과 (NavTestDelay=5, VerifyDelay=5, MonitorDuration=15)

### 정상 케이스 (1차 실행)

```
[NavTest][0.00s][INIT] Build Type: COOKED BUILD
[NavTest][0.00s][INIT] DataLayerActivateDelay: 5.0s | VerifyDelay: 5.0s | MonitorDuration: 15.0s
...
[NavDebug-FILTER] MarkDirtyTiles PASSED: DirtyArea ... DynamicModifier=1
[NavDebug-A] TileGen::Setup CompressedLayers=1 bGeometryChanged=0 bRegenerateCompressed=0
[NavDebug-D] AfterGatherGeometry Modifiers=1
[NavDebug-F] MarkDynamicAreas Modifiers=1
[NavTest][10.19s][RESULT] [DL] NavMod_DL | ... PathFound=Yes PathPoints=4 | ModifierApplied=Yes | >>> PASS <<<
[NavTest][10.19s][SUMMARY] ALL TESTS PASSED
```

- `MarkDynamicAreas Modifiers=1` → modifier 정상 인식 및 적용
- `CompressedLayers=1` → pre-baked geometry 존재, modifier-only 경로 사용

### 이상 케이스: Modifiers=0

2차 실행에서 발견:

```
[NavDebug-F] MarkDynamicAreas Modifiers=0
```

→ TileGen 시점에 nav octree에서 modifier를 찾지 못함

## 테스트 2차 결과 (NavTestDelay=0, MonitorDuration=120)

### DL 스트리밍 셀 진동 현상 발견

```
[t=0s]   LogNavigation: AddAreas: Reason=Remove from navoctree  ← modifier 해제
[t=0s]   [NavDebug-D] AfterGatherGeometry Modifiers=0           ← TileGen: modifier 없음
[t=3s]   LogNavigation: AddAreas: Reason=Addition to navoctree  ← modifier 재등록
[t=3s]   [NavDebug-D] AfterGatherGeometry Modifiers=1           ← TileGen: modifier 있음
[t=5s]   [VERIFY] PathPoints=4 → PASS                          ← 우연히 PASS 시점에 modifier 있음
[t=9s]   LogNavigation: AddAreas: Reason=Remove from navoctree  ← 또 진동
```

### 진동 원인: 스트리밍 셀 경계

플레이어 스폰 위치가 DL 스트리밍 셀(CGTVIK8TITDFIDAC6CM5BD5CP)의 스트리밍 거리 **경계 근처**에 있어서, 셀이 반복적으로 로드/언로드됨.

- `Remove from navoctree`: 셀 언로드 → NavModifierComponent가 nav octree에서 제거됨
- `Addition to navoctree`: 셀 재로드 → NavModifierComponent가 nav octree에 재등록됨 (`bIsFromVisibilityChange=0`)

### 테스트 프로젝트가 "우연히" PASS하는 이유

VERIFY 타이밍에 따라:
- modifier가 등록된 상태(`Modifiers=1`)이면 → PASS
- modifier가 해제된 상태(`Modifiers=0`)이면 → FAIL

**즉, 현재 테스트 프로젝트의 PASS는 타이밍의 우연이지, modifier가 안정적으로 적용된 것이 아님.**

## MONITOR 로그 분석

```
[NavTest][MONITOR] [+24.3s] [DL] NavMod_DL | PathPoints: 4 → 2 | modifier LOST
[NavTest][MONITOR] [+27.1s] [DL] NavMod_DL | PathPoints: 2 → 4 | modifier APPLIED
[NavTest][MONITOR] [+33.8s] [DL] NavMod_DL | PathPoints: 4 → 2 | modifier LOST
```

→ modifier 적용/해제가 진동하는 패턴 명확히 확인됨

## DL 런타임 STATE 자체가 변경됨 (핵심 발견)

```
[13:37:50:758][frame 424] LogWorldPartition: Data Layer Instance 'DL_NavModifier' state changed: Activated -> Unloaded
[13:37:50:758][frame 424] LogWorldPartition: Data Layer Instance 'DL_NavModifier' effective state changed: Activated -> Unloaded
[13:37:50:760][frame 424] UWorldPartitionStreamingPolicy::UnloadCells CGTVIK8TITDFIDAC6CM5BD5CP
```

- DL 상태 변경(758ms)이 셀 언로드(760ms)보다 **먼저** 발생
- `SetDataLayerRuntimeState` 함수 내부에서만 "state changed" 로그가 출력됨
- 해당 함수를 호출하는 파일: WorldDataLayers.cpp, DataLayerManager.cpp, DataLayerSubsystem.cpp **뿐**
- WP 스트리밍 정책(WorldPartitionStreamingPolicy.cpp)에는 `SetDataLayerRuntimeState` 호출 없음
- 호출자 불명확: BP_NavTestGameMode Blueprint 또는 다른 WP 내부 메커니즘 가능성

### 동작 의미

`SetDataLayerRuntimeState(Activated)`를 호출했어도, 플레이어가 DL 스트리밍 셀 경계 근처에 있을 때 **DL 런타임 상태 자체가 Activated → Unloaded로 변경**됩니다. 이는 UE5 WP의 "Content-based data layer streaming" 동작일 가능성이 있습니다.

## bIsFromVisibilityChange 차이

| 상황 | bIsFromVisibilityChange | 결과 |
|------|------------------------|------|
| 데이터 레이어 명시적 활성화 | `0` (false) | dirty area 정상 생성 → TileGen 실행 |
| 스트리밍 재로드 (visibility 변경) | `1` (true) | bLoadedData=true 마킹 가능 |

※ `bUseWPDynamicMode=false`이면 bLoadedData 관련 필터링은 동작하지 않음 → 스트리밍 재로드 시에도 TileGen이 실행됨
