# 08. 충돌과 물리 재질

> **작성일**: 2026-04-21
> **엔진 버전**: UE 5.7

## 1. 개요 — 렌더 메시와 분리된 충돌

Landscape의 **렌더링**과 **물리 충돌**은 **서로 다른 데이터 구조**로 표현됩니다:

| 측면 | 렌더링 | 물리 충돌 |
|------|-------|---------|
| 컴포넌트 | `ULandscapeComponent` | `ULandscapeHeightfieldCollisionComponent` |
| 해상도 | `ComponentSizeQuads` (예: 127) | `CollisionSizeQuads` (예: 63, 더 낮음) |
| 표현 | GPU Heightmap 텍스처 | Chaos `FHeightField` 객체 |
| 레이어 | Weightmap 블렌딩 | 지배 레이어 → 물리 재질 |

분리된 이유:
- 충돌은 **대부분 수직 레이캐스트**라 고해상도가 필요 없음 (캐릭터 걸음걸이에 cm 수준 세밀도 불필요)
- 저해상도 Heightfield는 **메모리·쿼리 비용 모두 유리**
- 콜리전 데이터는 **런타임 쿠킹** 가능 — DDC 캐싱으로 로드 부하 관리

이 문서는 이 물리 표현의 구체 구조와 생성·교체 트리거를 다룹니다.

## 2. ULandscapeHeightfieldCollisionComponent

### 2.0 1:1 짝 관계 — ULandscapeComponent당 콜리전 컴포넌트 하나

각 `ULandscapeComponent`(렌더용 타일)마다 정확히 하나의 `ULandscapeHeightfieldCollisionComponent`(물리용 타일)가 **1:1 짝**으로 붙습니다. 매핑 구조:

```
ALandscapeProxy
  ├── LandscapeComponents[]         (렌더용, N개)
  │     ├── Comp0 (XY=0,0)          ←─┐
  │     ├── Comp1 (XY=1,0)          ←──┼─ 각각 1:1로
  │     └── ...                         │
  │                                     │
  └── CollisionComponents[]         (물리용, 같은 N개)
        ├── CollComp0 (XY=0,0)      ←─┘  매칭되는 콜리전 컴포넌트
        ├── CollComp1 (XY=1,0)
        └── ...
```

**관계 유지 방법**:
- `ULandscapeComponent::CollisionComponentRef` → 짝 콜리전 컴포넌트 하드 참조
- `ULandscapeHeightfieldCollisionComponent::RenderComponentRef` → 반대 방향 하드 참조
- **동일한 `SectionBaseX/Y` 격자 좌표**로 매칭
- 둘 다 `ULandscapeInfo`의 별도 맵에 등록:
  - `XYtoComponentMap` (렌더 컴포넌트)
  - `XYtoCollisionComponentMap` (콜리전 컴포넌트) — **서버에서도 유효**, 서버는 렌더 컴포넌트 없이 이것만 로드

**예외 — 서버 전용 콜리전**: 데디케이티드 서버는 렌더가 필요 없으므로 `ULandscapeComponent`(렌더용)는 로드하지 않고 `ULandscapeHeightfieldCollisionComponent`만 로드합니다. 이 때문에 서버에서는 `XYtoComponentMap`이 비어 있거나 일부만 채워져 있을 수 있고, 대신 `XYtoCollisionComponentMap`이 물리·내비 쿼리의 주 진입점 역할을 합니다.

### 2.1 선언 및 핵심 멤버

```cpp
// LandscapeHeightfieldCollisionComponent.h:40
UCLASS(MinimalAPI, Within=LandscapeProxy)
class ULandscapeHeightfieldCollisionComponent : public UPrimitiveComponent
{
    // 섹션 좌표 (렌더 컴포넌트와 동일한 XY 격자 좌표)
    UPROPERTY() int32 SectionBaseX;
    UPROPERTY() int32 SectionBaseY;
    
    // 콜리전 해상도
    UPROPERTY() int32 CollisionSizeQuads;
    UPROPERTY() float CollisionScale;                    // = ComponentSizeQuads / CollisionSizeQuads
    UPROPERTY() int32 SimpleCollisionSizeQuads;          // Simple 콜리전용 별도 해상도
    
    // 페인트 레이어 목록 (렌더 WeightmapLayerAllocations와 매칭)
    UPROPERTY()
    TArray<TObjectPtr<ULandscapeLayerInfoObject>> ComponentLayerInfos;
    
    // 각 collision quad의 플래그 (하위 6비트 = 물리 재질 인덱스)
    UPROPERTY()
    TArray<uint8> CollisionQuadFlags;
    
    // 쿠킹된 물리 엔진 데이터
    TArray<uint8> CookedCollisionData;
    UPROPERTY() TArray<TObjectPtr<UPhysicalMaterial>> CookedPhysicalMaterials;
    
    // 캐시된 경계 (쿼리 가속)
    UPROPERTY() FBox CachedLocalBox;
    
    // 런타임 Chaos 핸들 (ref-counted)
    TRefCountPtr<FHeightfieldGeometryRef> HeightfieldRef;
};
```

#### 해상도에 따른 성능 차이 — 실제 얼마나 차이 나는가

쿼리 종류와 지형 크기에 따라 차이가 큰데, 체감·벤치 기준으로:

| 항목 | 풀 해상도 (`CollisionSizeQuads = 127`) | 1/2 해상도 (63) | 1/4 해상도 (31) |
|------|-----|-----|-----|
| **Heightfield 빌드 시간** | 기준 | ~1/4 | ~1/16 |
| **메모리 (컴포넌트당)** | ~512 KB | ~128 KB | ~32 KB |
| **레이캐스트 쿼리 비용** | 기준 | ~1/2 | ~1/4 |
| **캐릭터 이동 정밀도** | cm 단위 | 수~수십 cm | 수십~수백 cm |
| **경사면 표현** | 매끄러움 | 약간 각짐 | 명확히 각짐 |

**주의**: 위 수치는 대략적 감각이며, **GPU가 아니라 CPU 쿼리 기준**입니다 (Heightfield는 CPU에서 쿼리되는 물리 구조).

**왜 차이가 나는가**:
- Chaos Heightfield는 **2D 격자 위의 높이 값 배열 + 이진 서브분할 트리**(BVH 유사)로 저장. 격자 크기가 N이면 트리 노드 수가 O(N²), 깊이 O(log N).
- 레이캐스트는 이 트리를 타고 내려가며 격자 셀 단위로 높이 비교 → **격자 해상도가 쿼리 시간에 직결**.
- 메모리도 격자 크기 제곱에 비례.

**실용 권장**:

| 용도 | 권장 해상도 |
|------|---------|
| **캐릭터 이동 (Simple Collision)** | 렌더의 **1/4~1/8** (`CollisionMipLevel=2~3`) — 빠른 쿼리, 적당한 정밀도 |
| **정밀 레이캐스트 (프로젝타일 적중 등)** | 렌더의 **1/2** 정도 (`CollisionMipLevel=1`) — 정확도 필요한 경우 |
| **모바일** | 렌더의 **1/8** 이상 — CPU 여유 확보 |
| **정밀 메카닉 (퍼즐 지형 등)** | 렌더 해상도와 **동등** (`CollisionMipLevel=0`) — 비싸지만 필요할 때만 |

기본값은 보통 `CollisionMipLevel=1`(절반) + `SimpleCollisionMipLevel=2`(1/4) 조합으로 충분합니다. 프로파일링으로 물리 쿼리 비용이 문제가 될 때 한 단계 더 낮추는 것이 일반적 튜닝입니다.

#### "CookedCollisionData"가 뭐고, 있으면 런타임에 계산 안 하는가

**쿠킹(cooking)** = 지형의 **원시 16비트 높이 데이터**를 **물리 엔진(Chaos)이 레이캐스트에 쓸 수 있는 실제 바이너리 자료구조**로 변환하는 과정. 즉 "데이터 포맷 변환 + 사전 계산".

```
원시 Heightfield 높이 데이터 (uint16 배열)
     ↓ 쿠킹 (CookCollisionData)
Chaos::FHeightField 바이너리 (공간 분할 트리 + 삼각 인덱스 + 물리재질 매핑)
     ↓ 저장
CookedCollisionData: TArray<uint8>
     ↓ 런타임 로드
Chaos::FHeightField 인스턴스 (레이캐스트 즉시 가능)
```

**쿠킹 결과가 저장되어 있으면 런타임에 하는 일**:
- **안 하는 것**: 16비트 높이 → Chaos 자료구조로의 변환 (BVH 빌드, 삼각분할, 재질 인덱싱)
- **하는 것**: CookedCollisionData 바이트 배열을 파싱해 `Chaos::FHeightField` 객체로 복원 (비교적 빠른 역직렬화)
- **효과**: 맵 로드 시 힛치 대폭 감소. 수백 컴포넌트의 콜리전을 초기화해도 부담이 덜함.

**없으면** (런타임 쿠킹 경로):
- 프록시 로드 시 **즉석에서 쿠킹 수행** → 수십~수백 ms 힛치 발생 가능
- 그래서 엔진은 **DDC(Derived Data Cache)에 쿠킹 결과를 캐시**하여 재사용:
  - `SpeculativelyLoadAsyncDDCCollsionData`가 컴포넌트 등록 전에 DDC 비동기 로드 시작
  - 도착했으면 바로 사용, 없으면 쿠킹 후 저장

즉 **CookedCollisionData는 "미리 구운 결과를 디스크에 저장"**으로, **있으면 런타임은 디시리얼라이즈만**, 없으면 쿠킹이 발생합니다. 게임 빌드에서는 쿠킹 과정에서 모든 Landscape 콜리전이 사전에 쿠킹되므로 런타임 쿠킹은 드문 fallback입니다.

### 2.2 FHeightfieldGeometryRef — Chaos 지오메트리 래퍼

#### "Chaos 지오메트리"가 뭔가

**Chaos**는 UE5의 **기본 물리 엔진** 이름입니다. UE4 시대의 PhysX(NVIDIA)를 대체해 UE5부터 표준으로 쓰이며, Epic이 직접 개발한 물리 엔진. 이름은 혼돈 이론(chaotic systems)이 아니라 Epic의 내부 작명.

**Chaos 지오메트리(Chaos Geometry)** = Chaos 엔진이 레이캐스트·겹침 검사·관성 계산 등에 사용하는 **"형태를 표현하는 자료구조"**들의 통칭. 형태마다 전용 클래스가 있음:

| Chaos 지오메트리 타입 | 형태 | 사용처 |
|------|------|------|
| `Chaos::FSphere` | 구 | 캐릭터 간단 충돌, 파티클 |
| `Chaos::FBox` | 박스 | 상자, AABB 근사 |
| `Chaos::FCapsule` | 캡슐 | 캐릭터 콜리전(보통) |
| `Chaos::FConvex` | 볼록 다면체 | 복잡한 단단한 오브젝트 |
| **`Chaos::FHeightField`** | **높이 필드** (2D 격자 + Z값) | **지형 콜리전 (Landscape가 이걸 씀)** |
| `Chaos::FTriangleMesh` | 일반 삼각형 메시 | 복잡한 정적 메시 |

**`Chaos::FHeightField`의 특징**:
- **메모리 효율**: N×N 격자에서 높이값 N²개만 저장 (삼각형 인덱스 저장 안 함 — 격자라는 "규칙"에서 자동 유도)
- **빠른 레이캐스트**: 내부 공간 분할 트리(BVH 유사)로 O(log N) 복잡도의 교차 테스트
- **각 quad별 재질**: `CollisionQuadFlags` 하위 6비트가 각 격자 셀의 물리 재질 인덱스. 히트 위치의 셀 플래그를 조회해 해당 물리 재질 반환
- **"Hole" 지원**: `QF_NoCollision` 플래그로 셀 단위 물리 비활성화 (Visibility 레이어 구멍)

**FHeightfieldGeometryRef의 역할**:
Landscape 콜리전 컴포넌트가 Chaos::FHeightField를 "참조 카운트로 공유 가능하게" 래핑한 구조체. 왜 래핑했는가:
- **스레드 안전성**: 물리 시뮬은 별도 스레드에서 돌아감. Landscape 편집(게임 스레드)과 물리 쿼리(물리 스레드)가 동시에 Heightfield에 접근할 때 **ref-count로 안전하게 생명주기 관리**
- **Simple + Complex 둘 다**: 한 컴포넌트가 두 개의 Heightfield를 묶어 보유 (단순 쿼리용 + 정밀 쿼리용)
- **에디터 전용 버전**: 에디터에서 Visibility 레이어 구멍이 반영된 버전을 추가로 보유 (플레이에서는 Complex + Simple만 사용)

```cpp
// LandscapeHeightfieldCollisionComponent.h:102
struct FHeightfieldGeometryRef : public FThreadSafeRefCountedObject
{
    FGuid Guid;
    TArray<Chaos::FMaterialHandle> UsedChaosMaterials;
    Chaos::FHeightFieldPtr HeightfieldGeometry;          // Complex 콜리전
    Chaos::FHeightFieldPtr HeightfieldSimpleGeometry;    // Simple 콜리전
    
#if WITH_EDITORONLY_DATA
    Chaos::FHeightFieldPtr EditorHeightfieldGeometry;    // 에디터 전용 (Visibility 반영 버전)
#endif
};
```

**하나의 컴포넌트가 세 가지 Heightfield**를 가집니다:
- **Complex**: 기본 쿼리용 (정확한 높이)
- **Simple**: 저해상도 단순 콜리전 (캐릭터 이동 등 빠른 쿼리)
- **Editor**: 에디터에서만 쓰는 변형 (Visibility 레이어 구멍을 반영한 버전)

쿼리 시 `EHeightfieldSource`로 어느 것을 쓸지 선택:

```cpp
// LandscapeHeightfieldCollisionComponent.h:31-37
enum class EHeightfieldSource
{
    None,
    Simple,
    Complex,
    Editor
};
```

#### "쿼리할 때 이 enum을 함께 보내서 어떤 변형을 쓸지 고르는가"

**네, 그런 API들에서 이 enum을 파라미터로 받아 분기**합니다. Landscape 전용 API 예:

```cpp
// LandscapeHeightfieldCollisionComponent.h:335-343
LANDSCAPE_API TOptional<float> GetHeight(float X, float Y, EHeightfieldSource HeightFieldSource);
LANDSCAPE_API UPhysicalMaterial* GetPhysicalMaterial(float X, float Y, EHeightfieldSource HeightFieldSource);

LANDSCAPE_API bool FillHeightTile(TArrayView<float> Heights, int32 Offset, int32 Stride) const;
LANDSCAPE_API bool FillMaterialIndexTile(TArrayView<uint8> Materials, int32 Offset, int32 Stride) const;
```

호출 예:
```cpp
// 정밀 레이캐스트: Complex
float H = CollisionComp->GetHeight(X, Y, EHeightfieldSource::Complex).GetValue();

// 캐릭터 이동: Simple
UPhysicalMaterial* Mat = CollisionComp->GetPhysicalMaterial(X, Y, EHeightfieldSource::Simple);

// 에디터 도구가 Visibility 구멍을 반영하려면: Editor
float EditorH = CollisionComp->GetHeight(X, Y, EHeightfieldSource::Editor).GetValue();
```

내부적으로:
```cpp
switch (Source)
{
    case EHeightfieldSource::Simple:  return HeightfieldRef->HeightfieldSimpleGeometry;
    case EHeightfieldSource::Complex: return HeightfieldRef->HeightfieldGeometry;
    case EHeightfieldSource::Editor:  return HeightfieldRef->EditorHeightfieldGeometry;
}
```

**일반 UE 물리 쿼리 API**(`UWorld::LineTraceSingle` 등)는 이 enum을 직접 받지 않고, 대신 **쿼리 파라미터의 `TraceComplex` 플래그** + 프로젝트 설정의 "Use Complex as Simple"로 간접 선택됩니다:
- `TraceComplex = false` + 기본 채널 설정 → Simple Heightfield 사용
- `TraceComplex = true` → Complex Heightfield 사용
- 이때 내부적으로 위 enum과 매핑됨

즉 enum은 **Landscape 직접 API에서는 파라미터로 노출**되고, **일반 UE 물리 쿼리에서는 Trace 플래그에서 간접 매핑**되는 구조. 외부 게임플레이 코드는 보통 Trace 플래그만 쓰고, enum은 Landscape 내부 분기에만 등장합니다.

### 2.3 Simple vs Complex 콜리전

UE의 물리 쿼리 채널 설정에 따라 **Simple** 또는 **Complex** 지오메트리가 선택됩니다:

| 타입 | 해상도 | 용도 |
|------|------|------|
| Simple | `SimpleCollisionSizeQuads` (더 낮음) | 캐릭터 이동, 간단한 트레이스 |
| Complex | `CollisionSizeQuads` | 정밀 레이캐스트, 프로젝타일 |

프로젝트 설정에서 "Use Complex as Simple"을 체크하면 Simple 쿼리도 Complex를 씁니다 (일부 레거시 설정).

> **소스 확인 위치**
> - `Engine/Source/Runtime/Landscape/Classes/LandscapeHeightfieldCollisionComponent.h:40-344` — 전체 클래스
> - `LandscapeHeightfieldCollisionComponent.h:102-118` — `FHeightfieldGeometryRef`
> - `LandscapeHeightfieldCollisionComponent.h:62-70` — 해상도 필드들

## 3. CollisionQuadFlags — 비트 패킹된 쿼드 속성

> **"쿼드(quad)"란**: 2D 격자의 한 칸 = 4개 정점이 이루는 단위 정사각형. 렌더링 시에는 대각선으로 나뉘어 **삼각형 2개**로 그려집니다. `CollisionSizeQuads = 63`이면 한 컴포넌트의 콜리전 격자가 한 축당 63칸 (= 64개 정점). 콜리전 Heightfield도 이 격자 단위로 Chaos가 교차 테스트를 수행하며, **`CollisionQuadFlags`는 이 각 quad 셀의 8비트 속성**(물리 재질 인덱스 + 대각선 방향 + 구멍 플래그)을 저장합니다. 자세한 quad/subsection 정의는 [04-heightmap-weightmap.md §2.0](04-heightmap-weightmap.md) 참고.

각 collision quad는 8비트 플래그를 가집니다:

```cpp
// LandscapeHeightfieldCollisionComponent.h:180-185
enum ECollisionQuadFlags : uint8
{
    QF_PhysicalMaterialMask = 63,   // 하위 6비트: 이 쿼드의 물리 재질 인덱스 (0~63)
    QF_EdgeTurned = 64,             // 이 쿼드의 대각선이 뒤집혔는지 (triangulation 방향)
    QF_NoCollision = 128,           // 이 쿼드는 콜리전 없음 (Visibility 구멍)
};
```

해석:
- **하위 6비트 (64가지)**: `ComponentLayerInfos[idx]`의 인덱스 — 이 쿼드의 **지배(dominant) 페인트 레이어**를 가리킴. 그 레이어의 `PhysMaterial`을 쓸 것.
- **7번 비트**: 기본 Heightfield에서 quad를 두 삼각형으로 나눌 때의 방향 (0 = `/`, 1 = `\`).
- **8번 비트**: Visibility 레이어가 임계값 이상이어서 이 쿼드는 렌더·콜리전에서 제외 ("구멍").

#### 쿼드 쿼리는 CPU에서 일어나며, 플래그는 quad 단위 (정점 단위 아님)

**쿼리 위치**: 콜리전 쿼리(`LineTrace`, `Overlap`, `GetHeight` 등)는 **모두 CPU에서 수행**됩니다. Chaos 물리 엔진이 CPU 기반이며, `Chaos::FHeightField`도 CPU 메모리에 있는 자료구조입니다.

- 게임 스레드/물리 스레드에서 레이캐스트 호출 → Chaos가 BVH 트리를 따라 quad 셀 단위로 교차 테스트 → 히트 위치의 quad 플래그 조회 → 결과 반환
- GPU는 콜리전 쿼리에 관여하지 않음 (GPU는 렌더링 전담)

**플래그의 적용 단위**: `CollisionQuadFlags`는 **quad(격자 셀) 단위**이지 정점 단위가 아닙니다.

```
CollisionSizeQuads=3 격자 예 (실제로는 더 큼):

정점:    +---+---+---+
quad:    |   |   |   |     ← CollisionQuadFlags[0..2]
         +---+---+---+
         |   |   |   |     ← CollisionQuadFlags[3..5]
         +---+---+---+
         |   |   |   |     ← CollisionQuadFlags[6..8]
         +---+---+---+

배열 크기 = CollisionSizeQuads² = 9
한 quad = 4개 정점이 둘러싸는 셀 1개
```

- 한 quad가 두 삼각형으로 분할되어 그려져도, **같은 quad 안의 두 삼각형은 같은 플래그(같은 물리 재질)** 를 가짐
- 정점 자체에는 플래그 정보 없음 (정점은 단지 위치값)
- 히트 시 "히트 좌표가 어느 quad에 속하는가" 계산 → 그 quad의 플래그 조회

#### "대각선 방향에 따라 뭐가 달라지나"

각 quad는 두 삼각형으로 분할되는데, 대각선이 `/`인지 `\`인지에 따라 **삼각형의 모양과 두 삼각형의 경계가 달라집니다**:

```
QF_EdgeTurned = 0 (대각선 /)         QF_EdgeTurned = 1 (대각선 \)
+---+                                +---+
|  /|                                |\  |
| / |                                | \ |
|/  |                                |  \|
+---+                                +---+
```

이게 의미하는 것:
- **레이가 quad 중앙을 통과할 때 어느 삼각형에 히트하는지가 달라짐** → 두 삼각형의 표면 노멀이 다르면 결과 노멀 벡터가 달라짐
- **두 삼각형이 약간 다른 경사를 가질 때** (네 모서리 높이가 평면 위에 있지 않을 때), 대각선 방향이 **어느 두 정점을 더 가깝게 묶을지** 결정 → 시각적으로 보이는 표면 형태도 미세하게 달라짐

**언제 뒤집히나**: 4개 모서리 높이의 분포에 따라 "더 자연스러운 분할 방향"을 자동 선택. 예를 들어 인접한 두 코너가 매우 높고 반대쪽 두 코너가 낮으면, **높은 두 코너를 잇는 대각선**으로 분할해야 산등성이가 부드럽게 표현됨. 이를 자동 결정해 비트로 저장.

#### "임계값 이상" — 무엇의, 어떤 임계값?

Visibility 레이어 가중치의 임계값입니다. 정확히는:

```cpp
// LandscapeDataAccess.h:19
#define LANDSCAPE_VISIBILITY_THRESHOLD (2.0f/3.0f)
```

즉 **0.667**. Visibility 레이어 가중치는 Weightmap에 저장되어 0~1 범위인데:
- **가중치 ≥ 2/3** → 이 quad는 "지워진 것으로 간주" → `QF_NoCollision` 비트 ON, 렌더에서도 투명 처리
- **가중치 < 2/3** → 평소 그대로 표시

왜 정확히 2/3인가: 페인트로 "구멍을 뚫는" 의도를 나타낼 때 가중치를 1 근처까지 강하게 칠해야 한다는 UX 결정이 들어가 있음. 약하게 칠한 영역(0.5 정도)은 구멍이 아니라 단지 "약간 칠해진" 상태로 처리.

이 플래그는 **런타임 쿠킹된 콜리전 데이터에 같이 직렬화**되며, 물리 쿼리 히트 시 "히트 위치의 quad 플래그 → 물리 재질 인덱스" 조회에 쓰입니다.

#### "런타임 쿠킹"의 정확한 타이밍

문서 §6에서 자세히 다루지만 요약:
- **에디터 저장 시** (보통): 쿠킹된 데이터를 `.uasset`에 같이 저장 → 다음 로드부터는 쿠킹 안 함
- **쿠킹(빌드) 단계**: 게임 빌드 시 모든 콜리전이 사전 쿠킹됨
- **런타임 fallback**: 저장된 데이터가 없거나 무효일 때 (편집 직후 즉시 플레이 등) 비로소 런타임에 쿠킹 발생 — DDC 캐시로 완화 (§6 참고)

게임 빌드에서는 거의 모든 콜리전이 사전 쿠킹되어 있어 런타임 쿠킹은 드물고, 에디터에서도 DDC 덕분에 두 번째 로드부터는 빠릅니다.

## 4. 지배 레이어 → 물리 재질 매핑

### 4.1 DominantLayerData

에디터에서 각 collision vertex마다 "가장 가중치가 큰 레이어"가 무엇인지 미리 계산합니다:

```cpp
// LandscapeHeightfieldCollisionComponent.h:129-130
/** Indices into the ComponentLayers array for the per-vertex dominant layer. Stripped from cooked content */
FByteBulkData                               DominantLayerData;
```

- 타입: `FByteBulkData` (대용량 바이너리, 디스크 직렬화 최적화)
- 각 바이트 = `ComponentLayerInfos[]` 배열의 인덱스
- 쿠킹 시 `CollisionQuadFlags`로 합성되어 포함됨 (그래서 cooked에는 없음)

#### "어디서부터 계산을 하는가" — 입력은 Weightmap

DominantLayer 계산은 **컴포넌트의 Weightmap 데이터를 입력으로** 받습니다. 매핑:

```
ULandscapeComponent
  ├── WeightmapTextures[]              ← 입력
  └── WeightmapLayerAllocations[]      ← 어느 레이어가 어느 채널에 있는지

각 collision vertex (X, Y) 위치마다:
  for each layer in WeightmapLayerAllocations:
      weight = SampleWeightmap(layer.TextureIndex, layer.Channel, X, Y)
      if weight > maxWeight:
          maxWeight = weight
          dominantLayerIdx = layer index in ComponentLayerInfos[]
  
  DominantLayerData[vertexIdx] = dominantLayerIdx (0~63)
```

- Weightmap 텍스처를 CPU 측에서 샘플링해(또는 머지 결과 readback 후 처리) 각 정점 위치의 모든 레이어 가중치를 비교
- 가장 큰 가중치를 가진 레이어를 그 정점의 "지배 레이어"로 결정
- 이후 quad 단위로 모이면 (4개 정점 중 다수결 또는 대표 정점 기준) `CollisionQuadFlags`의 하위 6비트 인덱스로 합성

**즉 "DominantLayer 계산"의 출처는 Weightmap**이고, 계산 시점은 BatchedMerge가 Weightmap을 갱신한 직후 콜리전 재빌드가 트리거될 때.

### 4.1.1 DominantLayer 방식 vs PhysicalMaterialRender 방식 — 무엇이 다른가

두 방식은 **물리 재질 결정 로직이 어디에 사는가**가 다릅니다:

| 측면 | DominantLayer 방식 | PhysicalMaterialRender 방식 |
|------|-------------------|---------------------------|
| **결정 로직** | 페인트 레이어 가중치 단순 비교 (max) | 재질 그래프(머티리얼) 출력 |
| **입력** | Weightmap 텍스처 | 머티리얼이 보는 모든 입력 (Weightmap + 높이 + 노멀 + 시드 + 함수 등) |
| **계산 위치** | CPU (Weightmap 샘플링) | **GPU** (재질 셰이더를 RT에 렌더) |
| **유연성** | 항상 "가중치 큰 레이어 = 재질" 단순 매핑 | 임의 로직 가능 (예: "노멀이 수직에 가까우면 풀, 경사 30도 이상이면 자갈") |
| **빌드 비용** | 빠름 | 느림 (RT 렌더 + 비동기 readback) |
| **세밀도** | 정점 단위 | 픽셀 단위 (RT 해상도) |

**언제 어느 쪽이 활성화되나**:
- **기본**: DominantLayer 방식 — 모든 Landscape에 자동 적용
- **추가 활성화**: 머티리얼 그래프에 **"Output Topology Hash" 또는 "Physical Material Output" 노드**를 추가하면 PhysicalMaterialRender 방식이 함께 동작
- 두 방식이 **서로 배타가 아니라 보완**: PhysicalMaterialRender 결과가 있으면 그게 우선 사용, 없으면 DominantLayer로 fallback

**DominantLayer = "페인트 레이어 가중치만 보고 결정"**이라 단순하고 빠름. **PhysicalMaterialRender = "셰이더가 자유롭게 결정"**이라 강력하지만 GPU 렌더 + readback 비용 발생.

### 4.1.2 "런더 물리 정보"는 어디 저장되고 어디서 쿼리되나

저장 위치:
- `ULandscapeHeightfieldCollisionComponent::PhysicalMaterialRenderData` (`FByteBulkData`) — GPU 렌더 결과 readback 후 CPU 측 보관, 에디터 전용 (cooked에는 없음)
- `ULandscapeHeightfieldCollisionComponent::PhysicalMaterialRenderObjects` (`TArray<UPhysicalMaterial*>`) — `RenderData`의 인덱스가 가리키는 실제 PhysicalMaterial 자산 목록

쿠킹 시:
- 위 데이터들이 `CollisionQuadFlags`(quad 단위)로 합성되고 `CookedPhysicalMaterials`(런타임용 인덱스 배열)에 매핑되어 직렬화
- 따라서 게임 빌드(cooked)에는 `RenderData/RenderObjects` 자체는 없고 `CollisionQuadFlags + CookedPhysicalMaterials` 조합으로 동등 정보 보유

**쿼리 위치 — 런타임에 어디서**:

| API | 위치 | 호출자 |
|-----|------|------|
| `ULandscapeHeightfieldCollisionComponent::GetPhysicalMaterial(X, Y, Source)` | 게임 스레드 | Landscape 직접 API |
| `UWorld::LineTraceSingleByChannel(...)` 등 | 게임/물리 스레드 | 일반 게임플레이 코드 (히트 결과의 `PhysMaterial` 필드) |
| `Chaos::FHeightField::QueryPosition` | 물리 스레드 | 물리 시뮬 내부 |

내부 흐름:
```
쿼리 위치 (X, Y) → quad 인덱스 계산 → CollisionQuadFlags[idx] → 하위 6비트 인덱스
                                                              ↓
                    ComponentLayerInfos[idx]→PhysMaterial  또는  CookedPhysicalMaterials[idx]
                                                              ↓
                                                   결과 UPhysicalMaterial*
```

**핵심**: 이 쿼리는 **CPU에서 일어남**. GPU는 빌드 시점에만 관여 (PhysicalMaterialRenderData 생성 시).

### 4.2 PhysicalMaterialRenderData — GPU 기반 물리 재질 맵

5.7에서는 더 정교한 방식도 제공: **재질 그래프에서 `OutputTopologyHash` 같은 GPU 연산으로 물리 재질 인덱스를 픽셀 단위로 렌더**합니다:

```cpp
// LandscapeHeightfieldCollisionComponent.h:132-137
/** Indices for physical materials generated by the render material. Stripped from cooked content */
FByteBulkData                               PhysicalMaterialRenderData;

/** Physical materials objects referenced by the indices in PhysicalMaterialRenderData. Stripped from cooked content */
UPROPERTY()
TArray<TObjectPtr<UPhysicalMaterial>>       PhysicalMaterialRenderObjects;
```

즉 **`DominantLayer` 방식(정적, 페인트 레이어 기반)**과 **`PhysicalMaterialRender` 방식(동적, 재질 셰이더 출력 기반)** 두 가지가 병존합니다. 후자는 더 세밀하지만 빌드 시간이 추가됩니다.

빌드 태스크 `FLandscapePhysicalMaterialRenderTask`가 이를 처리합니다:

```cpp
// LandscapePhysicalMaterial.h:16
class FLandscapePhysicalMaterialRenderTask
{
public:
    bool Init(ULandscapeComponent const* LandscapeComponent, uint32 InHash);
    void Release();
    
    bool IsValid() const;
    bool IsComplete() const;
    bool IsInProgress() const;
    
    void Tick();
    void Flush();
    
    TArray<uint8> const& GetResultIds() const;
    TArray<UPhysicalMaterial*> const& GetResultMaterials() const;
    
    uint32 GetHash() const { return Hash; }
};
```

동작: 재질 셰이더를 RT에 렌더해 각 픽셀의 물리 재질 ID를 얻고, 비동기 readback으로 CPU로 가져옵니다. 결과가 Heightfield 빌드 시 `CollisionQuadFlags`에 반영됩니다.

#### 빌드 태스크가 실제로 도는 타이밍

`FLandscapePhysicalMaterialRenderTask`는 **에디터 시점에만** 실행되며, 다음 트리거에 발동:

| 트리거 | 상황 |
|------|------|
| **Weightmap 편집 완료** | 페인트 레이어 가중치 변경 → 픽셀 단위 물리 재질 결과가 달라짐 → 재계산 필요 |
| **머티리얼 변경** | Landscape 머티리얼이 바뀌면 GPU 렌더 결과 자체가 달라짐 |
| **`bPendingPhysicalMaterialInvalidation` 플래그 ON** | 컴포넌트가 "내 물리 재질 데이터 무효" 상태일 때 다음 틱에 태스크 시작 |
| **빌드 메뉴 "Build Physical Material"** | 사용자가 명시적으로 일괄 빌드 요청 (`UE::Landscape::BuildPhysicalMaterial`) |
| **쿠킹(cook) 단계** | 패키징 전 모든 Landscape의 물리 재질이 최신인지 확인하고 필요 시 빌드 |

**비동기 진행**: 태스크 풀(`FLandscapePhysicalMaterialRenderTaskPool`)에서 동시 실행 가능. 각 태스크는 `Tick()`으로 진행 상태를 체크하고 `IsComplete()`가 true일 때 결과 회수.

**런타임에는 안 도나**: 기본적으로 안 돕니다. 게임 빌드에는 `FLandscapePhysicalMaterialRenderTask` 코드 자체가 `#if WITH_EDITOR` 가드 하에 있어 컴파일 안 됨. 런타임 실시간 페인팅 같은 특수 시나리오를 만들지 않는 한 발동 안 함.

#### "왜 GPU에서 계산해야 하는가" — 셰이더 그래프 평가의 비용

질문: "그냥 페인트 레이어 가중치만 보고 결정하면 될 텐데(DominantLayer 방식), 왜 비싸게 GPU 렌더 후 CPU readback?"

답: **머티리얼 그래프가 단순 가중치 비교를 넘어선 임의 로직을 표현 가능하기 때문**입니다. PhysicalMaterialRender 방식은 다음을 지원해야 합니다:

```
머티리얼 그래프 (개념)
  Input:  WorldPos, WorldNormal, Weightmap[], Heightmap, Time, RandomSeed, ...
  Logic:
    if (WorldNormal.z < 0.7)        // 경사 30도 이상
        return PM_Rocky;
    if (Heightmap < 100)             // 저지대
        if (Weightmap[Snow] > 0.5)
            return PM_Snow;          // 눈 덮인 저지대
        return PM_Grass;
    if (Weightmap[Sand] > Weightmap[Grass] && WorldPos.x > 5000)
        return PM_Sand;
    return PM_Grass;
  Output: PhysicalMaterial ID per pixel
```

이런 임의 분기·조건·텍스처 샘플링을 **CPU로 흉내내려면**:
- 모든 Weightmap 텍스처를 CPU에 로드 + 노멀 텍스처 + 머티리얼 그래프를 인터프리팅하는 가상 머신
- 각 위치마다 그래프 노드를 순회하며 평가 — **느림** (인터프리터 오버헤드)
- 픽셀 수가 많으면(컴포넌트당 16K+ 정점) CPU 시간 폭증

**GPU 활용 이유**:
- GPU는 머티리얼 그래프를 **이미 컴파일된 셰이더**로 가지고 있음 (렌더링용으로)
- 같은 셰이더를 **별도 RT에 한 번 더 렌더**해 "물리 재질 ID 출력"만 얻으면 끝 — 이미 최적화된 빠른 경로
- 결과 RT를 CPU로 **비동기 readback** (몇 프레임 지연 허용 가능)

**대안과 비교**:
- **CPU에서 머티리얼 인터프리트**: 너무 느려 실용 불가
- **단순 weightmap-max 사용 (= DominantLayer 방식)**: 빠르지만 머티리얼 그래프의 임의 로직 표현 불가
- **GPU 렌더 + readback** (현재 방식): 빌드 시 일회성 비용으로 임의 로직 결과 획득 → 좋은 트레이드오프

즉 GPU는 **"머티리얼이라는 이미 컴파일된 평가기"**를 재활용하는 수단이고, CPU readback은 **"그 결과를 물리 쿼리에 쓰기 위해 CPU 메모리로 옮기는"** 단계. 빌드 시 한 번만 하므로 런타임 영향 없음.

> **소스 확인 위치**
> - `LandscapeHeightfieldCollisionComponent.h:180-185` — `ECollisionQuadFlags`
> - `LandscapeHeightfieldCollisionComponent.h:128-137` — `DominantLayerData`, `PhysicalMaterialRenderData`
> - `Engine/Source/Runtime/Landscape/Classes/LandscapePhysicalMaterial.h:16-61` — `FLandscapePhysicalMaterialRenderTask`

## 5. 콜리전 생성·교체 트리거

### 5.1 RecreateCollision

```cpp
// LandscapeHeightfieldCollisionComponent.h:318
/** Recreate heightfield and restart physics */
LANDSCAPE_API virtual bool RecreateCollision();
```

이 함수가 콜리전 갱신의 핵심 진입점입니다. 호출되면:
1. 기존 `HeightfieldRef`의 Chaos Heightfield 참조 해제 (지연 소멸)
2. 새 높이 데이터로 `CookCollisionData` 또는 `GenerateCollisionData` 호출
3. `CreateCollisionObject`로 Chaos 지오메트리 생성
4. 물리 상태 재생성 (`OnCreatePhysicsState`)

### 5.2 어떤 경우에 호출되나

- **Heightmap 편집 완료 후**: `ULandscapeComponent::bPendingCollisionDataUpdate`가 true → 다음 틱에 `RecreateCollision`
- **Weightmap 편집 완료 후** (물리 재질이 바뀔 수 있음): `bPendingPhysicalMaterialInvalidation`
- **Visibility 레이어 변경**: 구멍이 생기거나 사라짐 → `CollisionQuadFlags`의 `QF_NoCollision` 비트 갱신

#### "물리 재질이 바뀐다"의 의미와 영향

**무엇이 바뀌는가**: 같은 위치의 quad가 가리키는 물리 재질이 다른 것으로 바뀜.

```
[이전]: 위치 (X,Y)의 quad → 풀(Grass) 페인트 레이어가 지배 → PhysMaterial = PM_Grass
[Weightmap 편집: 그 위치에 자갈 페인트로 덮음]
[이후]: 같은 quad → 자갈(Rock)이 지배 → PhysMaterial = PM_Rock
```

**언제 바뀌나**:
- 사용자가 Weightmap을 페인트해 지배 레이어가 바뀜
- 머티리얼 그래프 변경으로 PhysicalMaterialRender 결과가 바뀜
- 새 페인트 레이어가 추가/제거되어 인덱싱이 변경됨

**바뀌었을 때 일어나는 일**:
1. `bPendingPhysicalMaterialInvalidation = true` 표시
2. 다음 에디터 틱에 `FLandscapePhysicalMaterialRenderTask` 시작 (PhysicalMaterialRender 방식 사용 시) 또는 DominantLayer 재계산
3. 결과로 `CollisionQuadFlags`의 하위 6비트 인덱스 갱신 + 필요 시 `CookedPhysicalMaterials` 배열 변경
4. `RecreateCollision()`이 호출되어 Chaos Heightfield의 `UsedChaosMaterials` 배열 갱신
5. 물리 상태 재등록 (`OnDestroyPhysicsState` → `OnCreatePhysicsState`)

**쿼리 결과는 어떻게 바뀌나**:
- 쿼리 시점이 바뀜 이후라면 → 새 인덱스를 따라 새 PhysicalMaterial 반환
- 진행 중인 쿼리(드물지만 멀티스레드에서 가능)는 ref-counted Chaos 객체 덕에 안전 — 이전 버전을 마저 처리한 후 새 버전으로 자동 교체

**영향 범위**: 변경된 컴포넌트만. Landscape 전체가 영향받지 않고, 편집된 영역의 quad 플래그와 그 영역에 해당하는 콜리전 컴포넌트의 Heightfield만 갱신.

### 5.3 UpdateHeightfieldRegion — 부분 갱신

전체를 재생성하지 않고 **일부만** 갱신:

```cpp
// LandscapeHeightfieldCollisionComponent.h:249
/** Modify a sub-region of the physics heightfield. Note that this does not update the physical material */
void UpdateHeightfieldRegion(int32 ComponentX1, int32 ComponentY1, int32 ComponentX2, int32 ComponentY2);
```

스컬프트 브러시 중간 업데이트(매 프레임 호출될 수 있음)에서 사용됩니다. 물리 재질은 건드리지 않아 빠름 — 물리 재질이 바뀌는 경우는 `RecreateCollision`이 따로 호출됩니다.

#### 부분 갱신 가능 여부 판정 기준

엔진은 다음 기준으로 "부분 갱신 vs 전체 재생성"을 결정합니다:

| 조건 | 결과 |
|------|------|
| **Heightmap만 변경 + 변경 영역이 사각형으로 묘사 가능** | 부분 갱신 (`UpdateHeightfieldRegion(x1, y1, x2, y2)`) |
| **물리 재질 변경 의심** (Weightmap 편집, 페인트 레이어 변경) | 전체 재생성 (`RecreateCollision`) — 부분 갱신 함수가 물리 재질을 처리 안 함 |
| **Visibility 레이어 변경** | 전체 재생성 — `QF_NoCollision` 플래그가 quad 단위로 다시 평가되어야 함 |
| **콜리전 해상도 자체 변경** | 전체 재생성 — 격자 크기 자체가 달라짐 |
| **편집 영역이 매우 큰 경우** (예: 컴포넌트 전체) | 정책상 전체 재생성으로 단순화 가능 |

**더티 영역 추적**:
- `ULandscapeComponent::LayerDirtyCollisionHeightData: FIntRect` — 페인트 레이어 시스템에서 콜리전 더티 영역을 사각형으로 누적
- 브러시 한 번의 스트로크가 끝나면 이 사각형을 인자로 `UpdateHeightfieldRegion` 호출
- 사각형이 컴포넌트 전체를 덮으면 사실상 전체 재생성과 동등

**부분 갱신 vs 전체 재생성 비용 차이**:
- 부분 갱신: 사각형 영역의 높이 데이터만 Chaos Heightfield에 patch (BVH 일부만 재구성)
- 전체 재생성: Chaos Heightfield 전체 새로 빌드 + 물리 상태 재등록

브러시 드래그처럼 매 프레임 발생하는 작은 변경에는 부분 갱신, 더 큰 의미의 편집 종료 시점에는 전체 재생성으로 자연스럽게 분기됩니다.

### 5.3 UpdateHeightfieldRegion — 부분 갱신

전체를 재생성하지 않고 **일부만** 갱신:

```cpp
// LandscapeHeightfieldCollisionComponent.h:249
/** Modify a sub-region of the physics heightfield. Note that this does not update the physical material */
void UpdateHeightfieldRegion(int32 ComponentX1, int32 ComponentY1, int32 ComponentX2, int32 ComponentY2);
```

스컬프트 브러시 중간 업데이트(매 프레임 호출될 수 있음)에서 사용됩니다. 물리 재질은 건드리지 않아 빠름 — 물리 재질이 바뀌는 경우는 `RecreateCollision`이 따로 호출됩니다.

## 6. DDC 캐싱 — 로딩 힛치 방지

Heightfield를 런타임에 쿠킹(Chaos 지오메트리로 인코딩)하는 건 비용이 꽤 듭니다. 맵 로드 시 수백 개 컴포넌트를 모두 쿠킹하면 **힛치**가 생기므로 **DDC(Derived Data Cache)**를 활용합니다:

```cpp
// LandscapeHeightfieldCollisionComponent.h:147-153
/** 
 *  Flag to indicate that the next time we cook data, we should save it to the DDC.
 *  Used to ensure DDC is populated when loading content for the first time. 
 */
mutable bool                                bShouldSaveCookedDataToDDC[2];

/**
  * Async DCC load for cooked collision representation. We speculatively
  * load this to remove hitch when streaming 
  */
mutable TSharedPtr<FAsyncPreRegisterDDCRequest>    SpeculativeDDCRequest;

// 메서드
// LandscapeHeightfieldCollisionComponent.h:240
void SpeculativelyLoadAsyncDDCCollsionData();
```

**Speculative Load 흐름**:
1. 컴포넌트 로드 시작 전에 `SpeculativelyLoadAsyncDDCCollsionData`가 "이 콜리전 해시의 DDC 데이터가 있을 것 같다" → 비동기 로드 시작
2. 실제 등록 시점(`OnRegister`) 도달 시 이미 로드 완료되어 있으면 즉시 사용
3. 없으면 쿠킹 실행 → 이후 DDC에 저장

이로써 **에디터 재시작 시 또는 스트리밍 시** 콜리전 생성에 드는 시간이 최소화됩니다.

#### "있을 것 같다"를 어떻게 아는가 — Speculative의 실체

"투기적(speculative)" 로드가 동작하는 원리는 **콘텐츠 해시 기반 예측**입니다.

**핵심 아이디어**:
- 콜리전 컴포넌트는 로드 시점에 **자기 데이터의 해시 (`CollisionHash`)를 계산**할 수 있음 — 이 해시는 높이 데이터·물리 재질 배열·레이어 정보 등 쿠킹 입력으로부터 결정됨
- DDC는 **해시를 키로 한 key-value 캐시** → "이 해시에 해당하는 쿠킹된 바이너리가 이전에 저장되어 있으면 가져와라" 질의가 가능
- "있을 것 같다"의 의미: **해시가 계산되면, 그 해시가 과거 어느 시점에 저장되었는지 보장은 없지만, 같은 내용을 이전에도 열었던 개발자/빌드 파이프라인이라면 확률적으로 높음**

**따라서 실제 흐름**:

```cpp
// 1. 컴포넌트 로드 — 아직 Register 전 (OnRegister 호출 안 됨)
void ULandscapeHeightfieldCollisionComponent::PostLoad()
{
    // 2. 콘텐츠로부터 해시를 계산 (`CollisionHash` 채움)
    ComputeCollisionHash();
    
    // 3. DDC에 "이 해시의 쿠킹 데이터 있나?" 비동기 질의 시작
    //    → 있으면 백그라운드 스레드에서 로드가 진행됨 (OnRegister까지 걸리는 시간 동안)
    //    → 없으면 아무 일도 안 일어남 (무비용)
    SpeculativelyLoadAsyncDDCCollsionData();
}

// 4. 이후 액터 Register 시점
void ULandscapeHeightfieldCollisionComponent::OnRegister()
{
    // 5. DDC 비동기 로드 결과 체크
    if (SpeculativeDDCRequest && SpeculativeDDCRequest->Poll()) {
        // 6a. 이미 완료 — 그대로 Chaos 객체로 복원 (빠름)
        CreateCollisionObjectFromDDC(SpeculativeDDCRequest->GetData());
    } else {
        // 6b. 아직 진행 중이거나 DDC 미스 — 쿠킹 실행 또는 대기
        CookCollisionData(...);
    }
}
```

**"있을 것 같다"가 맞춰지는 확률**:

| 상황 | DDC hit 확률 |
|------|-----------|
| **같은 개발자가 같은 머신에서 두 번째 로드** | 매우 높음 (로컬 DDC에 이전 쿠킹 결과 있음) |
| **팀 공유 DDC 서버를 쓰는 프로젝트** | 높음 (다른 팀원이 이미 쿠킹한 적 있음) |
| **처음 컴포넌트를 만든 직후** | 낮음 (DDC 미스 → 쿠킹 발생 → 결과 저장) |
| **Landscape를 대규모 편집한 직후** | 해시가 바뀐 경우 낮음, 안 바뀐 부분은 높음 |
| **CI/쿠킹 빌드 과정** | 매우 높음 (빌드가 모든 해시를 미리 쿠킹·저장) |

**핵심 포인트** — "있을 것 같다"의 비용 균형:
- **DDC hit이면**: 로드 스레드에서 디스크/원격 fetching만 하면 되므로 비용 낮음. 로드 힛치 크게 감소.
- **DDC 미스면**: 비동기 질의가 "데이터 없음"을 반환. 그 동안 다른 스레드 유휴 자원만 썼을 뿐이라 **불이익 없음**.
- 즉 **Speculative = "낮은 비용으로 높은 이득을 노리는 낙관적 프리페치"**. 실패해도 손해 없고, 성공하면 힛치 제거. 근거가 "확실한 예측"이 아니라 "비동기 질의는 싸다"는 계산 위에서 동작하는 것입니다.

**DDC 자체에 대해**: Derived Data Cache = UE가 "원본 에셋으로부터 파생된 데이터(쿠킹 결과, 셰이더 컴파일 결과, Nanite 빌드 등)"를 키-값으로 저장하는 **영속 캐시**. 로컬(디스크) 또는 공유(네트워크) 형태로 존재. 키는 입력 해시, 값은 파생 바이너리. 쿠킹·셰이더 컴파일 같은 고비용 작업을 여러 빌드·개발자 간 공유해 재사용.

> **소스 확인 위치**
> - `LandscapeHeightfieldCollisionComponent.h:318` — `RecreateCollision`
> - `LandscapeHeightfieldCollisionComponent.h:249` — `UpdateHeightfieldRegion`
> - `LandscapeHeightfieldCollisionComponent.h:147-153` — DDC 필드들
> - `LandscapeHeightfieldCollisionComponent.h:240` — `SpeculativelyLoadAsyncDDCCollsionData`
> - `LandscapeHeightfieldCollisionComponent.h:245-246` — `GenerateCollisionObjects` / `CookCollisionData`

## 7. 내비메시 통합

```cpp
// LandscapeHeightfieldCollisionComponent.h:215
//~ Begin UPrimitiveComponent Interface
virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;

//~ Begin INavRelevantInterface Interface
virtual bool SupportsGatheringGeometrySlices() const override { return true; }
virtual void GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const override;
virtual ENavDataGatheringMode GetGeometryGatheringMode() const override;
virtual void PrepareGeometryExportSync() override;

// LandscapeHeightfieldCollisionComponent.h:176-178
int32 HeightfieldRowsCount;
int32 HeightfieldColumnsCount;
FNavHeightfieldSamples CachedHeightFieldSamples;     // 내비메시 빌드용 캐시된 샘플
```

NavMesh 빌드 시 Recast가 Landscape 콜리전을 "**타일 단위로 geometry slice 가져오기**" 방식으로 씁니다 (`GatherGeometrySlice`). 이는 월드 전체 콜리전을 한꺼번에 스캔하지 않고 **필요한 영역만** 내비 빌더가 요청하는 패턴 — 대규모 월드에서 메모리·시간을 절약합니다.

#### "필요한 영역"이 NavMeshBoundVolume이 둘러싼 영역인가

**네, 정확히 그 의미입니다.** NavMesh가 생성될 영역 = `ANavMeshBoundsVolume`이 정의한 월드 공간 박스 ∩ Landscape 콜리전.

흐름:
1. 월드에 `ANavMeshBoundsVolume` 액터(들)를 배치 → "이 영역 안에서만 NavMesh 빌드"라고 Recast에게 알림
2. NavMesh가 그 영역을 **Recast 타일 그리드**로 분할 (보통 64m 또는 128m 정사각형)
3. 각 타일의 빌드는 자기 영역에 해당하는 **지오메트리 슬라이스**가 필요
4. Recast가 해당 영역에 있는 컴포넌트(이 경우 `ULandscapeHeightfieldCollisionComponent`)에게 `GatherGeometrySlice(GeomExport, SliceBox)` 호출
5. Landscape 콜리전 컴포넌트는 **자기 콜리전 데이터 중 SliceBox와 겹치는 부분만** Recast에게 export
6. Recast가 이 슬라이스를 voxelize → walkable surface 추출 → NavMesh 폴리곤 생성

**Lazy export**: `ENavDataGatheringMode::Lazy` 모드면 Recast가 **타일을 빌드하기 직전에야** 지오메트리를 요청 → 월드 전체 콜리전을 미리 메모리에 올리지 않아 빌드 메모리 절약. World Partition + dynamic NavMesh에서 특히 유용.

**`CachedHeightFieldSamples` 역할**:
- Lazy 모드에서는 같은 영역이 여러 번 요청될 수 있음 (이웃 타일의 경계 처리 등)
- 매번 Heightfield 데이터를 다시 추출하면 비효율 → 캐시
- `FNavHeightfieldSamples`에 변환된 NavMesh-friendly 형식으로 보관

**볼륨이 없으면**: NavMesh 빌드 영역이 정의되지 않아 NavMesh가 만들어지지 않음. 그래서 NavMesh를 쓰는 프로젝트는 반드시 `ANavMeshBoundsVolume` 또는 (World Partition의 경우) `ANavigationDataChunkActor`로 영역을 명시해야 합니다.

**볼륨이 Landscape 일부만 덮으면**: 그 부분만 NavMesh가 생기고 나머지 영역은 NavMesh 없음 → AI는 그쪽으로 이동 불가. 일반적으로 플레이 가능 영역 전체를 덮도록 볼륨을 배치합니다.

`CachedHeightFieldSamples`는 실제 Heightfield 쿼리를 피하는 캐시로, 내비메시 octree가 lazy geometry export 모드일 때 활용됩니다.

## 8. 서버에서의 콜리전

Landscape의 **렌더 컴포넌트(`ULandscapeComponent`)는 서버에 로드되지 않지만, 콜리전 컴포넌트는 로드**됩니다. 서버는 지형을 그릴 필요가 없지만 쿼리/물리는 필요하기 때문.

`ULandscapeInfo`에 서버용 콜리전 컴포넌트 맵이 별도로 있는 이유도 여기서 옵니다:

```cpp
// LandscapeInfo.h:148
/** Map of the offsets (in component space) to the collision components. Should always be valid. */
TMap<FIntPoint, ULandscapeHeightfieldCollisionComponent*> XYtoCollisionComponentMap;
```

서버는 `XYtoComponentMap`(렌더) 없이도 `XYtoCollisionComponentMap`(콜리전)만 가지고 "(x, y) 지형 쿼리"를 처리할 수 있습니다.

## 9. ULandscapeMeshCollisionComponent — Deprecated (5.7)

과거에는 **XY Offset**(격자가 평면이 아닌 비틀린 지형)을 지원하기 위해 TriangleMesh 기반 콜리전 컴포넌트가 있었습니다:

```cpp
// LandscapeMeshCollisionComponent.h:24
UCLASS()
class UE_DEPRECATED(5.7, "RetopologizeTool/XYOffset deprecated with the removal of non-edit layer landscapes")
ULandscapeMeshCollisionComponent_DEPRECATED : public ULandscapeHeightfieldCollisionComponent
{
    UPROPERTY()
    FGuid MeshGuid;
    
    struct FTriMeshGeometryRef : public FRefCountedObject
    {
        FGuid Guid;
        TArray<Chaos::FMaterialHandle> UsedChaosMaterials;
        Chaos::FTriangleMeshImplicitObjectPtr TrimeshGeometry;
    };
    
    TRefCountPtr<FTriMeshGeometryRef> MeshRef;
};
```

5.7부터 비-edit-layer Landscape가 제거되면서 **XYOffset / RetopologizeTool도 deprecated**되었고, 새 프로젝트에서는 Heightfield 콜리전만 사용합니다. 구 프로젝트의 `_DEPRECATED` 접미사가 붙은 이 클래스는 마이그레이션 호환을 위해 남아 있을 뿐.

## 10. 콜리전 데이터 플로우 — 편집에서 쿼리까지

```mermaid
graph TD
    A[Heightmap/Weightmap 편집] --> B[BatchedMerge 완료]
    B --> C[ULandscapeComponent 업데이트]
    C --> D{변경 종류?}
    
    D -->|높이 변경| E[bPendingCollisionDataUpdate 세팅]
    D -->|물리 재질 변경| F[bPendingPhysicalMaterialInvalidation 세팅]
    D -->|Visibility 변경| G[CollisionQuadFlags QF_NoCollision 갱신]
    
    E --> H[다음 틱: RecreateCollision]
    F --> I[FLandscapePhysicalMaterialRenderTask 시작]
    G --> H
    I --> J[RT에서 물리 재질 렌더 → 비동기 readback]
    J --> H
    
    H --> K[CookCollisionData: 16비트 높이 + quad flags → Chaos 바이너리]
    K --> L[DDC에 저장]
    K --> M[GenerateCollisionObjects: Chaos::FHeightField 생성]
    M --> N[FHeightfieldGeometryRef 교체]
    N --> O[OnCreatePhysicsState: 물리 씬에 등록]
    
    O --> P[런타임 쿼리]
    P --> Q[GetHeight, GetPhysicalMaterial, 레이캐스트 등]
    
    style A fill:#fff3e0
    style H fill:#ffe0b2
    style P fill:#e8f5e9
```

## 11. 요약

| 주제 | 답 |
|------|---|
| 콜리전 해상도는 왜 렌더보다 낮나? | 쿼리 비용·메모리 절약, 수직 레이캐스트는 저해상도로 충분 |
| 물리 재질은 어떻게 결정되나? | (1) 지배 페인트 레이어의 `PhysMaterial`, 또는 (2) 재질 셰이더의 GPU 출력 |
| Simple vs Complex 차이? | Simple=저해상도(캐릭터 이동), Complex=고해상도(정밀 쿼리) |
| 구멍은 어떻게 표현되나? | `CollisionQuadFlags`의 `QF_NoCollision` 비트 + Editor용 별도 Heightfield |
| 편집 시 언제 재생성되나? | 편집 완료 후 다음 틱, `RecreateCollision()` 호출 |
| 부분 갱신 가능? | 예, `UpdateHeightfieldRegion(x1,y1,x2,y2)` (물리 재질 제외) |
| 로딩 힛치는 어떻게 피하나? | DDC 캐시 + `SpeculativelyLoadAsyncDDCCollsionData` |
| 서버에서도 콜리전만 로드되나? | 예, 렌더 컴포넌트는 스킵, `XYtoCollisionComponentMap` 사용 |
| 내비메시는 어떻게 쿼리하나? | `GatherGeometrySlice` 기반 타일 단위 lazy export |
| MeshCollisionComponent는? | 5.7에서 deprecated, Heightfield만 사용 |

이로써 Landscape 시스템의 주요 축(아키텍처, 편집, 렌더, 스트리밍, 충돌)을 모두 훑었습니다. 소스 파일 경로 총정리와 추가 참고 자료는 [references.md](references.md)에서.
