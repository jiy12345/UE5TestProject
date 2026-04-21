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

### 2.0 기본 용어: quad와 서브섹션

본격적인 크기 파라미터를 보기 전에 두 용어를 짚어둡니다.

#### Quad (쿼드)

**"쿼드"** = 2D 격자의 한 칸. 4개의 정점(좌상·우상·좌하·우하)이 이루는 **단위 정사각형**입니다. 렌더링 시에는 보통 대각선으로 나뉘어 **삼각형 2개**로 그려집니다.

```
  정점 A ─── 정점 B          정점 A ─── 정점 B
    │   quad   │       =>     │  ╱ │
    │          │              │ ╱  │   (삼각형 ABC + BCD로 분할)
  정점 C ─── 정점 D          정점 C ─── 정점 D
```

즉 `ComponentSizeQuads = 63`은 "한 컴포넌트가 한 축당 63칸(= 64개 정점)"이라는 뜻. `N × N quads` 컴포넌트는 `(N+1) × (N+1)` 정점으로 그려집니다.

#### 서브섹션 (Subsection)

**컴포넌트를 다시 1/2/4조각으로 쪼갠 내부 단위**입니다. `NumSubsections=2`면 컴포넌트는 내부적으로 2×2 = 4개 서브섹션으로 나뉘고, 각 서브섹션이 **독립된 렌더 배치 요소**가 됩니다.

왜 서브섹션이 있는가:

1. **서브섹션마다 다른 LOD를 가질 수 있음** — 카메라가 컴포넌트의 한쪽에 가깝고 반대쪽은 멀면, 가까운 서브섹션은 고해상도, 먼 서브섹션은 저해상도. 컴포넌트 전체를 한 LOD로 묶는 것보다 세밀.
2. **LOD 전환 모핑 경계를 좁힘** — 전체 컴포넌트 단위로 모핑하면 전환 구간이 크고 눈에 띄기 쉬움. 서브섹션 단위면 전환이 더 국소적.
3. **렌더/콜리전 영역 쪼개기** — 더 세밀한 프러스텀 컬링과 부분 갱신 가능.

즉 "컴포넌트 = 편집·저장·소유의 단위, 서브섹션 = 렌더링의 하위 세분화 단위"로 이해하면 됩니다.

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

#### 왜 "밉 체인"이 2의 거듭제곱 크기를 요구하는가

**밉맵(mipmap)**은 한 텍스처의 **해상도를 반씩 줄여가며 미리 계산해 둔 축소본들**입니다 (예: 128×128 → 64×64 → 32×32 → ... → 2×2 → 1×1). **밉 체인**은 이 축소본들의 전체 시퀀스이고, 거리가 먼 픽셀은 저해상도 밉을 샘플링해 속도·품질을 향상시킵니다.

```
Mip 0: 128×128  ┐
Mip 1: 64×64    │
Mip 2: 32×32    ├  밉 체인 (각 단계마다 한 축 해상도가 1/2)
Mip 3: 16×16    │
...             │
Mip 7: 1×1      ┘
```

2의 거듭제곱 요구 이유:
- 밉이 **정확히 절반**이 되려면 (예: 128 → 64 → 32) 원 크기가 2의 거듭제곱이어야 나눗셈이 정수로 떨어짐. 그렇지 않으면 (예: 100 → 50 → 25 → 12.5 …) 픽셀 경계가 불규칙해짐.
- 하드웨어의 **텍스처 필터링(예: Tri-linear)** 이 두 인접 밉 간 보간을 쓰는데, 크기가 규칙적이어야 서브픽셀 정합이 깔끔.
- 일부 고수 하드웨어는 non-power-of-2 밉도 지원하지만 모바일·오래된 GPU에서 문제·성능저하 소지.

Landscape에서 `SubsectionSizeQuads + 1`이 2의 거듭제곱이라는 건, **서브섹션 한 장의 Heightmap 블록 크기(정점 기준 = `SubsectionSizeQuads+1`)가 2의 거듭제곱**이 되어 그대로 밉 체인 생성이 깔끔하게 이루어집니다.

전형적인 조합:

| SubsectionSizeQuads | NumSubsections | ComponentSizeQuads | Heightmap 텍스처 해상도 |
|---|---|---|---|
| 7 (2³−1) | 1 | 7 | 8×8 |
| 15 (2⁴−1) | 1 | 15 | 16×16 |
| 31 (2⁵−1) | 2 | 62 → **63** (공유 경계 포함) | 64×64 |
| 63 (2⁶−1) | 2 | 126 → **127** (공유 경계 포함) | 128×128 |

#### 왜 이 조합들이 "전형적"인가

위 4개가 자주 쓰이는 이유:

1. **밉 체인 제약을 만족**: `SubsectionSizeQuads + 1`이 2의 거듭제곱 (8/16/32/64).
2. **`NumSubsections ∈ {1, 2}` 제약**: 2×2가 상한이므로 자연스럽게 제한됨.
3. **메모리·성능 실용 범위**: 
   - 너무 작은 컴포넌트(예: 7×7): 타일 수가 폭증 → 드로우 콜/레지스트리/스트리밍 오버헤드.
   - 너무 큰 컴포넌트(예: 255×255): 단일 Heightmap 텍스처가 너무 커서 메모리 피크, LOD 세밀 조절 어려움.
4. **127×127**이 가장 흔한 실사용 기본값: 컴포넌트당 약 4MB 정도 텍스처(Heightmap+Weightmap 합산), km² 월드에서 타일 수가 적당(100~400개).
5. **63×63**은 작은 지형/프로토타입/모바일에서 채택.

즉 "전형"은 제약 조건을 만족하는 **실용 스위트 스팟들**이고, 다른 조합(예: `SubsectionSizeQuads=127, NumSubsections=2 → 255×255`)도 기술적으로는 가능하지만 관리 비용이 커서 잘 쓰지 않습니다.

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

#### 독립 드로우 호출은 성능 손해가 아닌가

**예, 약간은 손해입니다.** 하지만 다음 이점으로 상쇄됩니다:

- **서브섹션별 독립 LOD**: 같은 컴포넌트에서도 가까운 서브섹션은 고해상도, 먼 쪽은 저해상도 → 전체 정점/픽셀 처리량이 줄어듦.
- **더 세밀한 프러스텀 컬링**: 컴포넌트의 일부 서브섹션만 화면 밖이면 그 서브섹션은 그리지 않음.
- **LOD 전환 모핑 구간이 짧아짐**: 사용자 눈에 pop이 덜 보임.

실제 드로우 콜 수를 계산해보면:
- 컴포넌트 400개 × `NumSubsections=2` → 1600 드로우 콜 (LOD당)
- 대부분 같은 SharedBuffers / 같은 머티리얼 인스턴스를 공유 → **드로우 콜 자체의 setup 비용은 작음**
- 현대 GPU/엔진은 이 정도의 드로우 콜을 쉽게 처리 (특히 Nanite 경로는 이 이슈가 아예 소멸)

**1×1 서브섹션 vs 2×2 서브섹션 선택 기준**:
- 컴포넌트당 정점 수가 많고 카메라가 비교적 먼 상황이 많다면 **2×2** (세밀 LOD 이점이 큼)
- 작은 지형/모바일처럼 컴포넌트 자체가 작다면 **1×1** (과분할 오버헤드 회피)

성능이 중요하면 **Nanite 경로**를 켜는 것이 가장 확실 — 드로우 콜 수 자체가 개별 서브섹션 기반 모델에서 벗어납니다 ([06-rendering-pipeline.md §3](06-rendering-pipeline.md) 참고).

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

#### 높이를 더 세밀하게 조절하고 싶으면 — 정밀도 트레이드오프

질문: "로컬 ±256cm / 0.78cm 단계만으로는 부족한 장면이 있다. 어떻게 대처?"

선택지는 **범위와 정밀도의 트레이드오프**입니다.

| 목적 | 방법 | 효과 |
|------|------|------|
| **더 큰 Z 범위** | 액터 `Scale Z` 증가 | 범위 × N배, 단계도 × N배로 거칠어짐 (예: Scale Z=100 → 범위 ±256m, 단계 ~0.78m) |
| **더 세밀한 단계** | 액터 `Scale Z` 감소 | 단계 × 1/N로 세밀, 범위도 × 1/N로 좁아짐 (예: Scale Z=0.5 → 범위 ±128cm, 단계 ~0.39cm) |
| **근본적 정밀도 향상** | 없음 (엔진 상수) | `LANDSCAPE_ZSCALE=1/128`, uint16 저장은 엔진 하드코딩이라 사용자가 못 바꿈 |

**기본 스케일의 유효 단계 계산**:
```
범위:    ±32768 × LANDSCAPE_ZSCALE = ±32768 × (1/128) = ±256 cm
최소 단계: 1 × LANDSCAPE_ZSCALE   = 1/128            ≈ 0.78125 cm  (7.8 mm)
```

실무 대응:
- **캐릭터 높이 단계에 0.78cm**가 보통은 충분 (발 밑에서 그 정도 차이는 육안 불가).
- **산악 지형 (높이차 수백 m)**: `Scale Z=100`으로 표현. 단계는 78cm로 커지지만 산 절벽은 원래 그렇게 세밀하지 않으니 체감 차이 없음.
- **평탄한 저지대에 미세한 굴곡이 필요한 경우**: `Scale Z=0.5`~`1`로 유지하고, 미세 변화는 **Normal Map(재질 레벨)**로 표현하는 것이 관례. 높이맵은 "큰 형상"을, Normal Map은 "미세한 요철"을 담당.
- **정말 mm 단위 정밀도가 필요한 특수 메카닉(예: 퍼즐 지형 맞춤)**: Landscape보다 **StaticMesh + 수동 메시 편집** 고려. Landscape는 이런 초정밀 용도로 설계되지 않음.

참고로 `LANDSCAPE_ZSCALE`이 왜 `1/128`이냐: `uint16 범위(65536) / 512 = 128`에서 나온 값으로, "기본 스케일에서 ±256cm 범위 / 0.78cm 단계"가 대부분 야외 지형에 적절한 감각이라는 경험적 선택입니다.

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
