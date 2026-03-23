# 05. 실전 활용 가이드

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 1. 기본 설정

### RecastNavMesh 배치 및 설정

레벨에 `RecastNavMesh-Default` 액터가 자동 배치됩니다. 주요 설정:

| 항목 | 프로퍼티 | 권장값 | 설명 |
|------|----------|--------|------|
| 타일 크기 | `TileSizeUU` | 1000 ~ 3000 UU | 크게 할수록 빌드 속도 ↓, 타일 수 ↓ |
| Cell Size | `NavMeshResolutionParams[Default].CellSize` | 10 ~ 20 UU | 정밀도 vs 성능 |
| Cell Height | `NavMeshResolutionParams[Default].CellHeight` | 10 UU | 수직 해상도 |
| Agent Radius | `AgentRadius` | 캐릭터 캡슐 반경과 일치 | |
| Agent Height | `AgentHeight` | 캐릭터 키와 일치 | |
| Runtime Generation | `RuntimeGeneration` | 목적에 맞게 | 아래 참조 |

### RuntimeGeneration 선택 기준

```
Static           → 정적 월드. 런타임 NavMesh 변경 없음 (최고 성능)
DynamicModifiersOnly → Nav Modifier(NavArea, 비용 변경)만 런타임에 추가/제거
Dynamic          → 지오메트리 자체가 변하는 경우 (문, 파괴 가능 벽 등)
```

---

## 2. C++에서 NavMesh 쿼리

### 기본 경로 쿼리

```cpp
#include "NavigationSystem.h"
#include "NavigationPath.h"

// 경로 찾기 (동기)
UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());
if (!NavSys) return;

FNavLocation StartLoc, EndLoc;
NavSys->ProjectPointToNavigation(StartPos, StartLoc, FVector(100.f, 100.f, 200.f));
NavSys->ProjectPointToNavigation(EndPos,   EndLoc,   FVector(100.f, 100.f, 200.f));

FPathFindingQuery Query(this, *NavSys->GetDefaultNavDataInstance(),
                         StartLoc.Location, EndLoc.Location);
FPathFindingResult Result = NavSys->FindPathSync(Query);

if (Result.IsSuccessful() && Result.Path.IsValid())
{
    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    // Points[0] = 시작, Points.Last() = 도착
}
```

### NavQueryFilter 커스터마이징

```cpp
// 특정 NavArea 비용을 높여서 우회하게 만들기
UNavigationQueryFilter* Filter = NewObject<URecastFilter_UseDefaultArea>(this);
// 또는 C++에서 직접:
FSharedNavQueryFilter QueryFilter = NavData->GetDefaultQueryFilter()->GetCopy();
QueryFilter->SetAreaCost(RECAST_NULL_AREA, FLT_MAX);   // Null 영역 통과 불가
QueryFilter->SetAreaCost(RECAST_DEFAULT_AREA, 1.0f);   // 기본 영역 비용

FPathFindingQuery Query(this, *NavData, Start, End, QueryFilter);
```

### 특정 위치가 NavMesh 위에 있는지 확인

```cpp
FNavLocation NavLocation;
bool bOnNavMesh = NavSys->ProjectPointToNavigation(
    ActorLocation,
    NavLocation,
    FVector(50.f, 50.f, 100.f)  // 검색 반경
);
```

---

## 3. 커스텀 NavArea 만들기

### C++ NavArea 정의

```cpp
// NavArea_Water.h
#pragma once
#include "NavAreas/NavArea.h"
#include "NavArea_Water.generated.h"

UCLASS()
class UNavArea_Water : public UNavArea
{
    GENERATED_BODY()
public:
    UNavArea_Water()
    {
        DefaultCost = 3.0f;           // 물 위는 3배 비용
        DrawColor = FColor(0, 100, 255);
    }
};
```

### Nav Modifier Volume에 적용

에디터에서 `NavModifierVolume` 액터를 배치하고 `Area Class`를 `NavArea_Water`로 설정합니다.
런타임에서 C++로:

```cpp
#include "NavModifierVolume.h"

ANavModifierVolume* ModVol = GetWorld()->SpawnActor<ANavModifierVolume>(
    ANavModifierVolume::StaticClass(), SpawnParams);
ModVol->SetAreaClass(UNavArea_Water::StaticClass());
// 위치/크기 설정...
```

---

## 4. 이해도 확인 실습 — NavMesh 디버그 시각화

분석한 내용을 직접 확인하는 C++ GameMode/Actor를 구현하여 `DynamicModifiersOnly` 동작을 이해합니다.

### 실습 목표

- NavMesh 타일 경계 시각화
- 특정 위치의 NavArea ID / 이동 비용 출력
- 경로 쿼리 결과 실시간 렌더링

### NavDebugVisualizerComponent

```cpp
// NavDebugVisualizerComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavDebugVisualizerComponent.generated.h"

UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class UNavDebugVisualizerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** 경로 쿼리 시작점 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathStart = nullptr;

    /** 경로 쿼리 끝점 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathEnd = nullptr;

    /** 활성화 여부 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    bool bEnabled = true;

private:
    void DrawNavPath();
    void DrawNavMeshInfo(const FVector& Location);
};
```

```cpp
// NavDebugVisualizerComponent.cpp
#include "NavDebugVisualizerComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "NavMesh/RecastNavMesh.h"

void UNavDebugVisualizerComponent::TickComponent(
    float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnabled) return;

    DrawNavPath();
    if (PathStart)
        DrawNavMeshInfo(PathStart->GetActorLocation());
}

void UNavDebugVisualizerComponent::DrawNavPath()
{
    if (!PathStart || !PathEnd) return;

    UNavigationSystemV1* NavSys =
        UNavigationSystemV1::GetNavigationSystem(GetWorld());
    if (!NavSys) return;

    FPathFindingQuery Query(
        this,
        *NavSys->GetDefaultNavDataInstance(),
        PathStart->GetActorLocation(),
        PathEnd->GetActorLocation()
    );

    FPathFindingResult Result = NavSys->FindPathSync(Query);
    if (!Result.IsSuccessful() || !Result.Path.IsValid()) return;

    const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
    for (int32 i = 1; i < Points.Num(); ++i)
    {
        DrawDebugLine(GetWorld(),
            Points[i - 1].Location,
            Points[i].Location,
            FColor::Green, false, -1.f, 0, 5.f);
    }

    // 경로 포인트 수 화면 출력
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            1, 0.f, FColor::Yellow,
            FString::Printf(TEXT("PathPoints: %d  PathLen: %.0f cm"),
                Points.Num(),
                Result.Path->GetLength())
        );
    }
}

void UNavDebugVisualizerComponent::DrawNavMeshInfo(const FVector& Location)
{
    ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(
        UNavigationSystemV1::GetNavigationSystem(GetWorld())
            ->GetDefaultNavDataInstance());
    if (!NavMesh) return;

    // 위치 투사
    FNavLocation NavLoc;
    bool bOnNav = UNavigationSystemV1::GetNavigationSystem(GetWorld())
        ->ProjectPointToNavigation(Location, NavLoc, FVector(50, 50, 100));

    DrawDebugSphere(GetWorld(), NavLoc.Location, 20.f, 8,
        bOnNav ? FColor::Cyan : FColor::Red, false, -1.f);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            2, 0.f, FColor::Cyan,
            FString::Printf(TEXT("OnNavMesh: %s  NodeRef: %llu"),
                bOnNav ? TEXT("Yes") : TEXT("No"),
                (uint64)NavLoc.NodeRef)
        );
    }
}
```

### 실습 검증 체크리스트

- [ ] GameMode에서 컴포넌트 부착 또는 에디터에서 Actor에 추가
- [ ] `PathStart`, `PathEnd` 설정 후 PIE 실행
- [ ] 경로가 녹색 선으로 표시되는지 확인
- [ ] Nav Modifier Volume 배치 후 경로가 우회하는지 확인
- [ ] `DynamicModifiersOnly` 모드에서 런타임 Nav Modifier 추가/제거 시 경로 변화 확인

---

## 5. DynamicModifiersOnly 동작 검증

이슈 #9에서 분석한 쿠킹 빌드 버그를 이해하기 위한 로그 확인 방법:

### 로그 활성화 (DefaultEngine.ini)

```ini
[Core.Log]
LogNavigation=VeryVerbose
LogNavigationDirtyArea=VeryVerbose
LogNavOctree=Verbose
```

### 확인 포인트

```
[NavTest][...][INIT] Build Type: COOKED BUILD         ← 쿠킹 빌드 확인
[NavTest][...][VERIFY] PathPoints: 2                  ← 직선 경로 = Modifier 미적용
[NavTest][...][VERIFY] PathPoints: 5                  ← 우회 경로 = Modifier 정상 적용
```

### CompressedTileCacheLayers 확인

엔진 소스 `RecastNavMeshGenerator.cpp` 약 라인 1808에 로그 추가:

```cpp
// 엔진 커스터마이징 예시 (개인 fork에서)
if (CompressedLayers.Num() == 0)
{
    UE_LOG(LogNavigation, Warning,
        TEXT("[NavDebug] Tile(%d,%d): CompressedLayers EMPTY → "
             "geometry rebuild will be attempted"),
        TileX, TileY);
}
```

---

## 6. 게임 활용 패턴

### 패턴 1: 런타임 위험 구역 설정

AI가 특정 영역을 피하도록 런타임에 NavArea_Obstacle을 동적으로 적용합니다.

```cpp
// 폭발 지점 주변을 일시적으로 위험 구역으로 설정
void AGameManager::MarkExplosionZone(FVector Center, float Radius, float Duration)
{
    // NavModifierVolume을 스폰하거나
    // 또는 Data Layer를 활성화하여 미리 배치된 Nav Modifier를 적용
}
```

### 패턴 2: 에이전트별 경로 커스터마이징

서로 다른 `AgentRadius`를 가진 NavMesh를 여러 개 설정하여 대형/소형 AI를 구분합니다.

```cpp
// 특정 에이전트 속성으로 NavData 조회
FNavAgentProperties LargeAgentProps;
LargeAgentProps.AgentRadius = 100.f;
LargeAgentProps.AgentHeight = 200.f;

ANavigationData* NavData = NavSys->GetNavDataForProps(LargeAgentProps);
FPathFindingQuery Query(this, *NavData, Start, End);
```

### 패턴 3: 비동기 경로 요청

메인 스레드 부하를 줄이기 위해 비동기 경로 요청을 사용합니다.

```cpp
void AMyAIController::RequestAsyncPath(FVector Destination)
{
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(GetWorld());

    FPathFindingQuery Query(this, *NavSys->GetDefaultNavDataInstance(),
                             GetPawn()->GetActorLocation(), Destination);

    FNavPathQueryDelegate Delegate;
    Delegate.BindUObject(this, &AMyAIController::OnPathFound);

    NavSys->FindPathAsync(NavSys->GetDefaultNavDataInstance()->GetNavAgentPropertiesRef(),
                           Query, Delegate);
}

void AMyAIController::OnPathFound(uint32 QueryID, ENavigationQueryResult::Type Result,
                                   FNavPathSharedPtr Path)
{
    if (Result == ENavigationQueryResult::Success && Path.IsValid())
    {
        // 경로 사용
    }
}
```
