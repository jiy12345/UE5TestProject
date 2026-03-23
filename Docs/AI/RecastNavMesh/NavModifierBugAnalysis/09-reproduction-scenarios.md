# DynamicModifiersOnly 쿠킹 빌드 Nav Modifier 미동작 — 재현 시나리오

> **작성일**: 2026-02-18
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#9](https://github.com/jiy12345/UE5TestProject/issues/9)
> **선행 문서**: [소스 코드 심층 분석](06-source-analysis-detail.md), [PIE vs 쿠킹 빌드 차이](07-pie-vs-cooked-loading.md), [상세 테스트 플랜](08-test-plan.md)

---

## 배경

### 현재 상황

- **소규모 테스트 프로젝트에서 쿠킹 빌드 테스트 결과**: ALL TESTS PASSED (4/4)
- **대규모 실제 프로젝트에서는 버그 발생 중**
- 결론: 소규모 프로젝트에서는 모든 셀이 거의 즉시 로드되어 타이밍 이슈가 재현되지 않음

### 왜 소규모 프로젝트에서 재현되지 않는가

테스트 프로젝트 로그 분석 결과:

| 항목 | 소규모 테스트 프로젝트 | 대규모 프로젝트 (추정) |
|------|----------------------|----------------------|
| World Partition 셀 수 | 소수 | 수백~수천 |
| 전체 셀 로딩 시간 | ~1초 미만 | 수초~수십초 |
| NavDataChunkActor 로딩 | 거의 즉시 | 스트리밍 우선순위에 따라 지연 |
| Nav Modifier 등록 시점 | NavDataChunk 로드 이후 | NavDataChunk 로드 이전 가능 |
| `bUseWPDynamicMode` | 0 (모든 셀 즉시 로드) | 1 가능 (셀이 점진적 로드) |

### 재현의 핵심

**Nav Modifier가 옥트리에 등록될 때, 해당 위치의 NavDataChunkActor가 아직 로드되지 않은 상태를 만들어야 한다.**

이 상태가 되면:
1. Nav Modifier dirty area 처리 시 `CompressedTileCacheLayers`가 비어있음 (원인 1)
2. 나중에 NavDataChunkActor가 로드되어도 `bExcludeLoadedData=true` 필터로 인해 Nav Modifier가 재처리되지 않음 (원인 5)

---

## 재현 시나리오 목록

### 시나리오 A: 플레이어 시작 지점에서 먼 거리에 Nav Modifier 배치

**난이도**: 하 | **엔진 수정**: 불필요 | **재쿠킹**: 필요

**원리**: World Partition은 플레이어 위치 기준으로 가까운 셀부터 로드한다. Nav Modifier를 충분히 먼 거리에 배치하면 해당 영역의 NavDataChunkActor가 늦게 로드되므로 타이밍 갭이 발생한다.

**설정**:

1. Nav Modifier Volume을 플레이어 시작 지점에서 **매우 먼 거리** (예: 50,000~100,000 유닛)에 배치
2. `DL_NavModifier`의 `Initial Runtime State` → **Activated**
   - 게임 시작 시 즉시 데이터 레이어가 활성화됨
   - Nav Modifier가 즉시 등록을 시도하지만, 해당 위치의 NavDataChunk는 아직 스트리밍 안 됨
3. NavTestGameMode의 `DataLayerActivateDelay` → `0` (또는 매우 짧게)

**기대 결과**:
- 먼 거리의 Nav Modifier → NavDataChunk 미로드 상태에서 dirty area 처리 → 빈 타일 생성
- 나중에 NavDataChunk 로드 → bExcludeLoadedData 필터 → 재처리 안 됨
- **버그 재현**

**검증 방법**:
```bash
UE5TestProject.exe L_NavModifierTest -NavTestDelay=0 -log -LogCmds="LogNavigation VeryVerbose, LogNavigationDirtyArea VeryVerbose"
```

**주의**: 맵 크기가 작으면 모든 셀이 스트리밍 범위 내에 들어가므로 의미 없음. 맵이 충분히 크거나, 스트리밍 거리를 줄여야 함.

---

### 시나리오 B: 스트리밍 거리 축소

**난이도**: 하 | **엔진 수정**: 불필요 | **재쿠킹**: 필요

**원리**: World Partition 스트리밍 소스의 로딩 거리를 줄이면, 플레이어 근처의 셀만 로드되고 먼 셀은 지연된다.

**설정**:

1. World Settings → World Partition → Runtime Settings:
   - `Loading Range` 축소 (예: 기본값에서 절반 이하로)
2. 또는 `wp.Runtime.OverrideRuntimeSpatialHashLoadingRange` 콘솔 변수 사용
3. Nav Modifier를 로딩 범위 **경계** 또는 **바로 밖**에 배치
4. `DL_NavModifier`의 `Initial Runtime State` → **Activated**

**기대 결과**:
- Nav Modifier 액터는 데이터 레이어 활성화로 로드됨 (거리 무관, `bIsSpatiallyLoaded`에 따라 다름)
- NavDataChunkActor는 공간 기반 스트리밍이므로 거리 밖이면 미로드
- 타이밍 갭 발생 → 버그 재현

**참고**: `bIsSpatiallyLoaded` 설정에 따라 데이터 레이어 액터의 로딩 조건이 달라짐:
- `bIsSpatiallyLoaded=true`: 스트리밍 범위 내 + 데이터 레이어 활성 둘 다 필요
- `bIsSpatiallyLoaded=false`: 데이터 레이어 활성만으로 로드 (거리 무관)

Nav Modifier의 `bIsSpatiallyLoaded`를 **false**로 설정하면 데이터 레이어 활성화만으로 즉시 로드되어 타이밍 갭을 극대화할 수 있음.

---

### 시나리오 C: World Partition 셀 크기 축소

**난이도**: 중 | **엔진 수정**: 불필요 | **재쿠킹**: 필요

**원리**: 셀 크기를 줄이면 셀 수가 증가하고, 로딩해야 할 셀이 많아져 개별 셀의 로딩이 지연된다.

**설정**:

1. World Settings → World Partition → Runtime Settings → Runtime Hash:
   - `Cell Size` 축소 (예: 기본 12800 → 3200 또는 더 작게)
2. 셀이 많아지면 프레임당 로딩할 수 있는 셀 수에 제한이 걸림
3. Nav Modifier가 로딩 우선순위가 낮은 셀에 속하도록 배치

**기대 결과**:
- 셀 수 증가 → 로딩 스케줄링 지연 → NavDataChunkActor 로딩 시점 늦어짐
- Nav Modifier가 먼저 등록 → 타이밍 갭 → 버그 재현

---

### 시나리오 D: 스트리밍 예산(Budget) 제한

**난이도**: 중 | **엔진 수정**: 불필요 | **재쿠킹**: 불필요 (실행 인자)

**원리**: 프레임당 처리할 수 있는 스트리밍 요청 수를 제한하면, 셀 로딩이 여러 프레임에 걸쳐 분산된다.

**관련 콘솔 변수**:

```ini
; 프레임당 최대 셀 로딩/언로딩 수
wp.Runtime.UpdateStreaming.MaxLevelToConsider=1

; 비동기 로딩 시간 제한 (ms)
s.AsyncLoadingTimeLimit=1.0

; 프레임당 최대 비동기 패키지 수
s.MaxAsyncLoadingCount=1
```

**설정**:

1. 실행 인자 또는 DefaultEngine.ini에 위 변수 추가
2. Nav Modifier는 데이터 레이어 활성화로 즉시 로드
3. NavDataChunkActor는 스트리밍 예산 제한으로 천천히 로드

**실행 예시**:
```bash
UE5TestProject.exe L_NavModifierTest -NavTestDelay=0 -log -ExecCmds="wp.Runtime.UpdateStreaming.MaxLevelToConsider 1, s.AsyncLoadingTimeLimit 0.5" -LogCmds="LogNavigation VeryVerbose, LogNavigationDirtyArea VeryVerbose"
```

**장점**: 재쿠킹 불필요, 실행 인자만으로 테스트 가능

---

### 시나리오 E: NavDataChunkActor 로딩 인위적 지연 (엔진 수정)

**난이도**: 상 | **엔진 수정**: 필요 | **재쿠킹**: 필요

**원리**: 엔진 코드에서 NavDataChunkActor의 `BeginPlay()` 또는 `AttachTiles()`를 인위적으로 지연시켜 타이밍 갭을 확실하게 만든다.

**수정 위치**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationDataChunkActor.cpp`

```cpp
// NavigationDataChunkActor::BeginPlay() 수정 제안
void ANavigationDataChunkActor::BeginPlay()
{
    Super::BeginPlay();

#if !UE_BUILD_SHIPPING
    // 디버그: NavDataChunk 로딩 인위적 지연 (쿠킹 빌드 타이밍 이슈 재현용)
    static float NavChunkDelay = 0.f;
    if (FParse::Value(FCommandLine::Get(), TEXT("-NavChunkDelay="), NavChunkDelay) && NavChunkDelay > 0.f)
    {
        UE_LOG(LogNavigation, Warning,
            TEXT("[NavDebug] Delaying NavDataChunkActor '%s' by %.1fs"),
            *GetName(), NavChunkDelay);

        FTimerHandle TimerHandle;
        GetWorldTimerManager().SetTimer(TimerHandle,
            this, &ANavigationDataChunkActor::AddNavigationDataChunkToWorld,
            NavChunkDelay, false);
        return; // BeginPlay에서 즉시 AddNavigationDataChunkToWorld 호출 방지
    }
#endif

    AddNavigationDataChunkToWorld();
}
```

**실행 예시**:
```bash
# NavDataChunk를 10초 지연시키고, 데이터 레이어는 5초 후 활성화
# → Nav Modifier가 먼저 등록(5초), NavDataChunk는 10초 후 로드
UE5TestProject.exe L_NavModifierTest -NavTestDelay=5 -NavChunkDelay=10 -log -LogCmds="LogNavigation VeryVerbose"
```

**장점**: 가장 확실한 재현 방법. 딜레이 값 조절로 다양한 타이밍 시나리오 테스트 가능.
**단점**: 엔진 소스 수정 + 엔진 재빌드 필요.

---

### 시나리오 F: 대량 더미 액터로 로딩 부하 생성

**난이도**: 중 | **엔진 수정**: 불필요 | **재쿠킹**: 필요

**원리**: 맵에 대량의 액터를 배치하여 스트리밍 시스템에 부하를 주면, 개별 셀의 로딩이 지연된다.

**설정**:

1. 맵 전체에 수천 개의 Static Mesh Actor 배치 (간단한 Cube 등)
2. 또는 대용량 텍스처를 사용하는 액터 배치 (패키지 로딩 시간 증가)
3. Nav Modifier는 데이터 레이어에, 더미 액터는 일반 공간 기반 스트리밍으로 배치
4. 더미 액터들이 스트리밍 예산을 소모하면서 NavDataChunkActor 로딩이 밀려남

**단점**: 실제 대규모 프로젝트 환경을 모사하긴 하지만, 제어가 어렵고 재현성이 불안정할 수 있음.

---

## 시나리오 비교 및 추천 순서

| 순서 | 시나리오 | 재현 확실성 | 준비 난이도 | 재쿠킹 | 엔진 수정 |
|------|----------|------------|------------|--------|-----------|
| **1** | **D: 스트리밍 예산 제한** | ★★★★ | 하 | 불필요 | 불필요 |
| **2** | **E: NavDataChunk 인위적 지연** | ★★★★★ | 상 | 필요 | 필요 |
| **3** | **A: 먼 거리 배치** | ★★★ | 하 | 필요 | 불필요 |
| **4** | **B: 스트리밍 거리 축소** | ★★★ | 하 | 필요 | 불필요 |
| **5** | **C: 셀 크기 축소** | ★★★ | 중 | 필요 | 불필요 |
| **6** | **F: 대량 더미 액터** | ★★ | 중 | 필요 | 불필요 |

### 추천 진행 순서

**1단계: 시나리오 D (스트리밍 예산 제한)** — 가장 빠르게 시도 가능
- 재쿠킹 불필요, 실행 인자만 변경
- `wp.Runtime.UpdateStreaming.MaxLevelToConsider=1`과 `s.AsyncLoadingTimeLimit=0.5` 조합

**2단계: 시나리오 D로 재현 실패 시 → 시나리오 A+B 조합**
- Nav Modifier를 먼 거리에 배치 + 스트리밍 거리 축소
- `bIsSpatiallyLoaded=false`로 설정하여 Nav Modifier만 즉시 로드
- 재쿠킹 필요

**3단계: 위 방법으로도 실패 시 → 시나리오 E (엔진 수정)**
- 가장 확실하지만 엔진 빌드 필요
- `-NavChunkDelay=` 실행 인자로 정밀 제어 가능

---

## 재현 성공 후 검증 흐름

재현에 성공하면 상세 테스트 플랜의 **Step 14~18**을 따라 로그를 분석한다:

1. `[NavTest][SUMMARY]`에서 `DATA LAYER MODIFIERS FAILED` 확인
2. `CAUSE` 키워드 검색 (엔진 디버그 로그 포인트가 추가된 경우)
3. `[NavDebug-D]`에서 `CompressedLayers EMPTY` 확인 (원인 1)
4. `[NavDebug-E]`에서 `Found 0 elements` 확인 (원인 5)
5. 프레임 번호로 Nav Modifier 등록과 NavDataChunk 로드 순서 비교

---

## 재현 실패 시 고려사항

모든 시나리오에서 재현 실패 시, 실제 프로젝트와의 차이를 다음 관점에서 재검토:

1. **Nav Modifier 속성 차이**: 실제 프로젝트의 Nav Modifier가 `FillCollisionUnderneathForNavmesh`, `NavMeshResolution` 등 추가 속성을 사용하는지 (원인 3)
2. **BaseNavmeshDataLayers 설정**: 실제 프로젝트의 World Settings에서 해당 데이터 레이어가 BaseNavmeshDataLayers에 포함되어 있는지 (원인 2)
3. **NavModifierComponent vs NavModifierVolume**: 실제 프로젝트에서 사용하는 Nav Modifier 타입이 다른지
4. **다중 RecastNavMesh**: 여러 에이전트 타입이 있어 네비메시가 복수인 경우
5. **커스텀 NavigationSystemConfig**: 네비게이션 시스템 설정이 기본과 다른 경우
6. **다른 플러그인 영향**: 네비게이션 관련 플러그인이 동작에 영향을 주는 경우
