# 04. Heightmap·Weightmap 데이터 레이아웃

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7

## 1. 개요 — 왜 텍스처인가

Landscape는 지형의 **고도**와 **레이어 가중치**를 정점 배열이 아니라 **2D 텍스처**에 저장합니다. 이 선택의 결과는 다음과 같습니다:

- **GPU가 직접 높이를 읽음** — 버텍스 셰이더가 Heightmap을 `Texture2D::Sample`로 샘플링해 정점 Z를 계산. CPU → GPU 정점 업로드 없음.
- **정점 수와 메모리가 디커플링됨** — 컴포넌트 하나의 정점 수는 고정이고, 해상도는 텍스처 크기로 결정.
- **편집이 텍스처 페인팅과 동일** — 브러시로 픽셀에 값을 쓰는 것이 곧 높이 편집.
- **LOD가 밉맵으로 자연스러움** — Heightmap 텍스처의 더 낮은 밉을 샘플링하면 저해상도 지형.
- **Runtime Virtual Texture/Nanite 경로와 자연스럽게 통합** — 어차피 텍스처 기반이니 RVT 피드도 쉬움.

이 문서는 그 텍스처에 **어떤 비트가 어디에 들어가는지**, **컴포넌트 기하 구조가 텍스처 픽셀과 어떻게 맞물리는지**를 다룹니다.

## 2. 컴포넌트 기하 — 섹션/서브섹션

### 2.1 세 가지 크기 파라미터

`ULandscapeComponent`는 세 가지 크기 파라미터로 자신의 기하를 정의합니다:

```cpp
// LandscapeComponent.h:427-435
UPROPERTY()
int32 ComponentSizeQuads;     // 이 컴포넌트의 전체 quad 수 (한 축당)

UPROPERTY()
int32 SubsectionSizeQuads;    // 서브섹션 하나의 quad 수 (SubsectionSizeQuads+1은 2의 거듭제곱)

UPROPERTY()
int32 NumSubsections;         // 서브섹션 개수 (한 축당 1 또는 2)
```

제약 조건:

```
ComponentSizeQuads = SubsectionSizeQuads × NumSubsections
SubsectionSizeQuads + 1 = 2^n  (밉 체인 생성 위해 2의 거듭제곱이어야 함)
NumSubsections ∈ {1, 2}        (1×1 또는 2×2 서브섹션)
```

전형적인 조합:

| SubsectionSizeQuads | NumSubsections | ComponentSizeQuads | Heightmap 텍스처 해상도 |
|---|---|---|---|
| 7 (2³−1) | 1 | 7 | 8×8 |
| 15 (2⁴−1) | 1 | 15 | 16×16 |
| 31 (2⁵−1) | 2 | 62 → **63** (공유 경계 포함) | 64×64 |
| 63 (2⁶−1) | 2 | 126 → **127** (공유 경계 포함) | 128×128 |

### 2.2 왜 서브섹션이 2×2면 "2 서브섹션 × (SubsectionSizeQuads+1) 픽셀"인가

각 서브섹션은 **자기 영역을 완결된 텍스처 블록**으로 가집니다. 즉 인접 서브섹션과 경계 정점을 공유하지 않고 **각자 +1 픽셀을 별도로 가짐**. 이는 LOD 전환 시 서브섹션 간 이음새를 부드럽게 보간하기 위한 설계 선택입니다.

```
2×2 서브섹션, SubsectionSizeQuads=31인 경우 Heightmap 텍스처:

  ┌──────────────32───────────────┬──────────────32──────────────┐
  │  서브섹션 (0,0)                │  서브섹션 (1,0)               │
  │  32×32 픽셀                    │  32×32 픽셀                   │
  │  (자체 경계 포함)               │  (자체 경계 포함)              │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  ├────────────────────────────────┼──────────────────────────────┤
  │  서브섹션 (0,1)                │  서브섹션 (1,1)               │
  │  32×32 픽셀                    │  32×32 픽셀                   │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  │                                │                               │
  └────────────────────────────────┴──────────────────────────────┘

  텍스처 총 크기: 64×64 (= (31+1) × 2)
  전체 정점 수: 64×64 = 4096
  그러나 렌더링 시 경계 정점은 인접 "컴포넌트"와 공유됨
```

실제 렌더링에서 각 서브섹션은 **독립된 드로우 호출**로 처리되며, 서로 다른 LOD를 가질 수 있습니다. 이웃 컴포넌트 사이의 경계도 **정점 공유**로 틈을 방지합니다 (다음 §2.3 참고).

### 2.3 컴포넌트 간 경계 정점 공유

이웃한 두 컴포넌트는 **경계에서 한 줄의 정점을 공유**합니다:

```
  컴포넌트 A (63×63 quads)       컴포넌트 B (63×63 quads)
  ┌──────────────────────┐       ┌──────────────────────┐
  │ 0 1 2 ... 62 63      │<-────>│ 0 1 2 ... 62 63      │
  │                       │ 공유 경계      │                       │
  │  A[X=63, Y=*]         │  ==   │  B[X=0, Y=*]         │
  └──────────────────────┘       └──────────────────────┘
```

이 공유 덕분에 이웃 컴포넌트 간 **이음새(seam)**가 생기지 않지만, 편집 시에는 "A 오른쪽 끝을 편집하면 B 왼쪽 끝도 같이 갱신해야" 하는 복잡성이 생깁니다. 이를 처리하는 유틸이 `ULandscapeHeightmapTextureEdgeFixup`입니다.

BatchedMerge에서 **노멀 계산을 위해 3×3 이웃 컴포넌트가 모두 필요한 이유**도 이 공유 경계 때문입니다 ([05-edit-layers.md](05-edit-layers.md) §노멀 계산 참고).

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h:427-435` — 세 가지 크기 파라미터
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h:118-137` — `FLandscapeVertexRef` (컴포넌트 내 정점 식별자, `(X, Y, SubX, SubY)`)
> - `Engine/Source/Runtime/Landscape/Public/LandscapeEdgeFixup.h` — `ULandscapeHeightmapTextureEdgeFixup`

## 3. Heightmap 텍스처

### 3.1 포맷과 패킹

Heightmap은 **32비트 컬러 텍스처(`UTexture2D`, BGRA8)** 한 장에 **16비트 높이 + 노멀 XY**를 함께 패킹합니다.

```
 Heightmap 텍스처 (픽셀 하나 = FColor 4바이트)
  ┌────┬────┬────┬────┐
  │ R  │ G  │ B  │ A  │
  ├────┼────┼────┼────┤
  │  16비트 높이    │  노멀 XY        │
  │  (R<<8 | G)    │  (X=B, Y=A)     │
  └────────────────┴─────────────────┘
```

- **R 채널**: 16비트 높이의 상위 8비트 (`Height >> 8`)
- **G 채널**: 16비트 높이의 하위 8비트 (`Height & 0xFF`)
- **B 채널**: 노멀 X (−1..+1을 0..255로 매핑)
- **A 채널**: 노멀 Y (−1..+1을 0..255로 매핑, 노멀 Z는 GPU에서 계산)

### 3.2 16비트 높이 → 월드 Z 변환

```cpp
// LandscapeDataAccess.h:26-50
inline constexpr int32 MaxValue = 65535;
inline constexpr float MidValue = 32768.f;

#define LANDSCAPE_ZSCALE     (1.0f / 128.0f)   // = 0.0078125
#define LANDSCAPE_INV_ZSCALE 128.0f

FORCEINLINE float GetLocalHeight(uint16 Height)
{
    return (static_cast<float>(Height) - MidValue) * LANDSCAPE_ZSCALE;
}

FORCEINLINE uint16 GetTexHeight(float Height)
{
    return static_cast<uint16>(FMath::RoundToInt(
        FMath::Clamp<float>(Height * LANDSCAPE_INV_ZSCALE + MidValue, 0.f, MaxValue)));
}

FORCEINLINE FColor PackHeight(uint16 Height)
{
    FColor Color(ForceInit);
    Color.R = Height >> 8;
    Color.G = Height & 255;
    return MoveTemp(Color);
}

FORCEINLINE float UnpackHeight(const FColor& InHeightmapSample)
{
    uint16 Height = (uint16)(InHeightmapSample.R << 8) | (uint16)InHeightmapSample.G;
    return GetLocalHeight(Height);
}
```

수식으로:

```
월드 Z (로컬, cm) = (uint16 Height − 32768) / 128
                  = (uint16 Height − 32768) × 0.0078125

uint16 Height = Clamp(월드 Z × 128 + 32768, 0, 65535)
```

| uint16 값 | 로컬 Z (cm) | 의미 |
|---|---|---|
| 0 | −256 | 최저점 (clamp됨) |
| 32768 (MidValue) | 0 | 정확히 중간 높이 |
| 65535 (MaxValue) | +255.9921875 | 최고점 (clamp됨) |

즉 **컴포넌트 로컬 Z 범위는 약 ±256cm 기본**입니다. 실제 월드 Z 범위는 액터의 **Scale Z**로 확장합니다 — 예를 들어 `Scale Z = 100`이면 ±25600cm (±256m) 높이 표현 가능.

### 3.3 노멀 패킹

```cpp
// LandscapeDataAccess.h:52-59
FORCEINLINE FVector UnpackNormal(const FColor& InHeightmapSample)
{
    FVector Normal;
    Normal.X = 2.f * static_cast<float>(InHeightmapSample.B) / 255.f - 1.f;
    Normal.Y = 2.f * static_cast<float>(InHeightmapSample.A) / 255.f - 1.f;
    Normal.Z = FMath::Sqrt(FMath::Max(1.0f - (FMath::Square(Normal.X) + FMath::Square(Normal.Y)), 0.0f));
    return Normal;
}
```

- X/Y만 저장하고 **Z는 단위 벡터 조건 `X² + Y² + Z² = 1`로 역산**
- 노멀 Z는 항상 양수라는 암묵 가정 (지형은 "위를 향함")
- 음수 Z 노멀(오버행)은 이 표현으로는 불가능 — 그런 경우는 StaticMesh로 풀어야 함

왜 2채널에만 저장하는가: Z를 별도 저장할 만큼 정밀도가 필요 없고, **텍스처 2채널을 높이에 양보**해야 16비트 정밀도가 나옵니다. 0..255 → −1..+1 매핑이므로 노멀 XY는 약 1/127 정밀도.

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Public/LandscapeDataAccess.h:13-14` — `LANDSCAPE_ZSCALE` / `LANDSCAPE_INV_ZSCALE`
> - `LandscapeDataAccess.h:26-36` — `MaxValue`, `MidValue`, `GetLocalHeight`, `GetTexHeight`
> - `LandscapeDataAccess.h:38-50` — `PackHeight`, `UnpackHeight`
> - `LandscapeDataAccess.h:52-59` — `UnpackNormal`

### 3.4 HeightmapScaleBias — 텍스처 공유

한 `ULandscapeComponent`가 **Heightmap 텍스처 전체를 독점하지 않을 수** 있습니다. 여러 작은 컴포넌트가 **하나의 큰 텍스처를 영역 분할해 공유**할 수 있으며, 각 컴포넌트가 "내 데이터가 텍스처의 어느 영역에 있는지"를 `HeightmapScaleBias`로 표현합니다:

```cpp
// LandscapeComponent.h:478-479
/** UV offset to Heightmap data from component local coordinates */
UPROPERTY()
FVector4 HeightmapScaleBias;
```

의미: `FVector4(ScaleU, ScaleV, BiasU, BiasV)`. 컴포넌트 로컬 UV가 `(u, v)`일 때 실제 텍스처 샘플 좌표는 `(u * ScaleU + BiasU, v * ScaleV + BiasV)`.

왜 이렇게 하는가: 컴포넌트가 작아서 텍스처 1장이 너무 낭비일 때(예: 32×32 컴포넌트 4개가 하나의 64×64 텍스처를 4등분) 메모리 절약. 단 GPU 렌더링 시 UV 변환 비용이 약간 추가됨.

## 4. Weightmap 텍스처

### 4.1 레이어 할당 모델

"레이어"는 페인트 도구의 한 개 브러시 종류입니다 (풀, 바위, 흙, 눈 등). 각 레이어는 컴포넌트별로 **0..1 가중치 맵**을 가지며, 재질에서 이를 블렌딩해 최종 외관을 결정합니다.

문제: 한 컴포넌트에 페인트 레이어가 N개 있으면 **N장의 텍스처**가 필요한가? → **아닙니다.** RGBA8 텍스처 한 장은 4개 채널이므로 **4개 레이어를 담을 수 있음**. 그래서 다음 구조:

```cpp
// LandscapeComponent.h:554-556, 559-560
/** List of layers, and the weightmap and channel they are stored */
UPROPERTY()
TArray<FWeightmapLayerAllocationInfo> WeightmapLayerAllocations;

/** Weightmap texture reference */
UPROPERTY()
TArray<TObjectPtr<UTexture2D>> WeightmapTextures;
```

- `WeightmapTextures[]`: RGBA8 텍스처들 (1장, 2장, ... 레이어 수에 따라)
- `WeightmapLayerAllocations[]`: 각 레이어가 어느 텍스처의 어느 채널에 들어있는지

### 4.2 FWeightmapLayerAllocationInfo

```cpp
// LandscapeComponent.h:141-187
USTRUCT()
struct FWeightmapLayerAllocationInfo
{
    UPROPERTY() TObjectPtr<ULandscapeLayerInfoObject> LayerInfo;
    UPROPERTY() uint8 WeightmapTextureIndex;   // WeightmapTextures[] 인덱스
    UPROPERTY() uint8 WeightmapTextureChannel; // 0=R, 1=G, 2=B, 3=A

    bool IsAllocated() const
    {
        return (WeightmapTextureChannel != 255 && WeightmapTextureIndex != 255);
    }
};
```

- **255**가 "비할당" 표식 — 레이어는 선언되었지만 아직 이 컴포넌트에 실제 채널이 없는 상태
- 레이어가 필요해지면 남는 채널을 할당 (없으면 새 텍스처 추가)

### 4.3 레이어 할당의 동역학

페인트 작업 시 레이어 할당이 **런타임에 동적으로 늘어납니다**:

```
[초기 상태: 레이어 0개 페인팅됨]
  WeightmapTextures = []
  WeightmapLayerAllocations = []

[풀 레이어 페인트]
  WeightmapTextures = [tex0(RGBA8)]
  WeightmapLayerAllocations = [{Grass, 0, 0}]  // tex0, R 채널

[바위 추가 페인트]
  WeightmapTextures = [tex0]
  WeightmapLayerAllocations = [{Grass, 0, 0}, {Rock, 0, 1}]  // tex0, G 채널

[눈, 흙, 모래, 이끼 추가: 4개 더]
  → tex0의 남은 2개 채널 (B, A) 사용 후, tex1 신설
  WeightmapTextures = [tex0, tex1]
  WeightmapLayerAllocations = [
    {Grass, 0, 0}, {Rock, 0, 1}, {Dirt, 0, 2}, {Sand, 0, 3},
    {Snow, 1, 0}, {Moss, 1, 1}
  ]
```

할당 로직은 `ReallocateLayersWeightmaps` (`LandscapeEditLayers.cpp`)에서 관리되며, BatchedMerge 파이프라인이 레이어 추가/제거 시 적절히 업데이트합니다.

### 4.4 Weightmap 가중치 합 규칙

각 픽셀의 모든 레이어 가중치는 **합이 1이 되도록 정규화**됩니다 (additive 블렌딩 모드 기준). 예:

```
픽셀 (32, 48)에서:
  Grass = 0.5
  Rock  = 0.3
  Dirt  = 0.2
  (합 = 1.0)
```

페인트 도구는 한 레이어를 칠할 때 다른 레이어들의 가중치를 **비례하여 감소**시켜 합을 1로 유지합니다. 이 정책이 있기 때문에 재질 블렌딩이 "각 레이어 × 가중치" 합으로 간단해집니다.

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeComponent.h:141-187` — `FWeightmapLayerAllocationInfo`
> - `LandscapeComponent.h:554-556` — `WeightmapLayerAllocations`
> - `LandscapeComponent.h:559-560` — `WeightmapTextures[]`
> - `LandscapeComponent.h:469-475` — `WeightmapScaleBias`, `WeightmapSubsectionOffset` (텍스처 공유 시 UV 변환)

## 5. Visibility Layer — 구멍(Hole) 표현

Landscape에 터널이나 구멍을 뚫고 싶을 때는 **별도의 Visibility 레이어**를 사용합니다. 이 레이어가 특정 픽셀에서 임계값 이상이면 해당 quad를 **렌더·콜리전에서 제외**합니다.

```cpp
// LandscapeDataAccess.h:19
#define LANDSCAPE_VISIBILITY_THRESHOLD (2.0f/3.0f)
```

Visibility 레이어는 일반 페인트 레이어와 같은 Weightmap 채널을 사용하지만, 재질에서 `HoleMaterial` 로직과 결합되어 투명 처리됩니다.

## 6. 편집 레이어별 데이터 — LayersData (Editor Only)

Edit Layers 시스템은 **각 편집 레이어가 컴포넌트별로 자신만의 기여 데이터를 보유**합니다 (최종 병합 전). 이를 담는 구조가 `LayersData`:

```cpp
// LandscapeComponent.h:520-521
#if WITH_EDITORONLY_DATA
/** Edit Layers that have data for this component store it here */
UPROPERTY()
TMap<FGuid, FLandscapeLayerComponentData> LayersData;
#endif
```

- 키: `FGuid` — 해당 편집 레이어의 식별자
- 값: `FLandscapeLayerComponentData` — 이 편집 레이어가 이 컴포넌트에 기여하는 Heightmap/Weightmap 데이터

**중요**: `HeightmapTexture`, `WeightmapTextures[]`는 **모든 편집 레이어가 합쳐진 최종 결과**이고, `LayersData`는 **개별 레이어의 원본**입니다. 사용자가 "Edit Layer 1만 편집" 모드로 작업할 때는 `LayersData[Layer1]`에 쌓이고, BatchedMerge 파이프라인이 모든 레이어를 합쳐 최종 텍스처를 만듭니다.

자세한 병합 흐름은 [05-edit-layers.md](05-edit-layers.md)에서.

> **소스 확인 위치**
> - `LandscapeComponent.h:520-521` — `LayersData` 맵
> - `LandscapeComponent.h:525-526` — `ObsoleteEditLayerData` (마이그레이션 시 사용되는 과도기 저장소)

## 7. CollisionMipLevel — 콜리전용 저해상도

콜리전은 **Heightmap의 특정 밉 레벨을 쓰는 별도 해상도**로 만들어집니다:

```cpp
// LandscapeComponent.h:597-602
/** Heightfield mipmap used to generate collision */
UPROPERTY(EditAnywhere, Category=LandscapeComponent)
int32 CollisionMipLevel;

/** Heightfield mipmap used to generate simple collision */
UPROPERTY(EditAnywhere, Category=LandscapeComponent)
int32 SimpleCollisionMipLevel;
```

예: `ComponentSizeQuads = 127`, `CollisionMipLevel = 1`이면 콜리전은 약 63 해상도. 캐릭터는 고해상도 렌더 메시 위에서 걷지만 **물리 쿼리는 저해상도 높이 필드**에서 수행 → 성능 이득.

자세한 콜리전 생성 흐름은 [08-collision-physics.md](08-collision-physics.md)에서.

## 8. 정리 — 한 컴포넌트의 데이터가 어디에 있는가

```
ULandscapeComponent
  │
  ├─ HeightmapTexture (UTexture2D, BGRA8)
  │    └─ 이 컴포넌트가 혼자 쓰거나 다른 컴포넌트와 공유
  │       (공유하면 HeightmapScaleBias로 UV 영역 분할)
  │
  ├─ WeightmapTextures[] (UTexture2D, RGBA8)
  │    └─ 4채널 × N장 = 최대 4N개 레이어 담을 수 있음
  │
  ├─ WeightmapLayerAllocations[] (FWeightmapLayerAllocationInfo)
  │    └─ 각 레이어가 어느 텍스처·채널에 있는지 인덱스
  │
  ├─ LayersData (TMap<FGuid, FLandscapeLayerComponentData>) [Editor Only]
  │    └─ 편집 레이어별 원본 기여 데이터 (병합 전)
  │
  ├─ CollisionMipLevel / SimpleCollisionMipLevel
  │    └─ 콜리전용 저해상도 밉 선택 인덱스
  │
  └─ MaterialInstances[] / LODIndexToMaterialIndex
       └─ LOD별 재질 인스턴스 (Weightmap 블렌딩 포함)
```

이 중 **편집 시점에 BatchedMerge가 손대는 것**은 `HeightmapTexture`, `WeightmapTextures[]`, `WeightmapLayerAllocations[]`이고, **런타임 렌더가 샘플링하는 것**도 같은 것들입니다. `LayersData`는 에디터 전용 "편집 중인 상태"입니다.

다음 문서에서는 이 편집 상태가 어떻게 GPU로 보내져 최종 텍스처로 병합되는지를 다룹니다 → [05-edit-layers.md](05-edit-layers.md).
