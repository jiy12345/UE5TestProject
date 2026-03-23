#pragma once

#include "Components/ActorComponent.h"
#include "NavDebugVisualizerComponent.generated.h"

/**
 * RecastNavMesh 이해도 확인용 디버그 시각화 컴포넌트.
 * 경로 쿼리 결과를 매 프레임 DrawDebug로 렌더링하고,
 * NavMesh 위에서의 위치 투사 결과를 화면에 출력합니다.
 *
 * 활용:
 *   - AGameMode나 임의 Actor에 부착
 *   - PathStart, PathEnd 설정 후 PIE 실행
 */
UCLASS(ClassGroup=(Navigation), meta=(BlueprintSpawnableComponent))
class UNavDebugVisualizerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UNavDebugVisualizerComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** 경로 쿼리 시작점 액터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathStart = nullptr;

    /** 경로 쿼리 끝점 액터 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    AActor* PathEnd = nullptr;

    /** 디버그 시각화 활성화 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    bool bEnabled = true;

    /** 경로 선 두께 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NavDebug")
    float PathLineThickness = 5.f;

private:
    void DrawNavPath();
    void DrawNavMeshInfo(const FVector& Location);
};
