# DrawDebug 시각화 패턴

`DrawDebugLine`, `DrawDebugSphere`, `AddOnScreenDebugMessage` 등 UE 디버그 시각화 도구의 실무 패턴. 모든 모듈에서 공통 사용.

## TL;DR

- 표면에 그릴 땐 **Z+10cm 오프셋** — z-fighting 회피 관습
- 구체 segments는 **6~8** — 디버그용 저해상도 (성능 절충, 매 프레임 N개 그리니까)
- `AddOnScreenDebugMessage`의 **Key**가 같으면 덮어씀 (매 프레임 갱신용), 다르면 누적 (여러 슬롯)
- Duration **`0.f`** — 1프레임만 (Tick에서 매 프레임 호출하면 지속 표시)
- Duration **`-1.f`** — 영구 (명시적으로 지울 때까지)
- 색상 표준: **초록 = 성공·정상**, **노란 = 중간·웨이포인트·경고**, **빨간 = 실패·위험**, **시안 = 정보·하이라이트**

## Recipes

### 매 프레임 시각화 (Tick에서 호출)

```cpp
void UMyComp::TickComponent(float DT, ELevelTick TickType, FActorComponentTickFunction* This)
{
    Super::TickComponent(DT, TickType, This);

    DrawDebugLine(GetWorld(),
        StartLocation, EndLocation,
        FColor::Green,
        false,    // bPersistentLines (false면 다음 호출까지만)
        -1.f,     // LifeTime (-1이면 1프레임 — bPersistentLines=false 조합)
        0,        // DepthPriority
        5.f);     // Thickness
}
```

- `bPersistentLines = false` + `LifeTime = -1.f` 조합 = **이번 호출 후 1프레임만 표시**
- Tick에서 매 프레임 호출하므로 항상 보이는 것처럼 작동

### 일시적 표시 (한 번 호출, 일정 시간 유지)

```cpp
DrawDebugLine(GetWorld(), Start, End, FColor::Red,
    false,     // bPersistentLines
    3.0f,      // 3초 동안 표시
    0, 5.f);
```

### 영구 표시 (명시적으로 지울 때까지)

```cpp
DrawDebugLine(GetWorld(), Start, End, FColor::Yellow,
    true,      // bPersistentLines = true
    -1.f,
    0, 5.f);

// 지울 때
FlushPersistentDebugLines(GetWorld());
```

### 화면 출력 메시지 (HUD 텍스트)

```cpp
if (GEngine)
{
    GEngine->AddOnScreenDebugMessage(
        1,                                      // Key — 같은 key는 덮어씀
        0.f,                                    // Duration (Tick에서 매 프레임 호출 시 0.f)
        FColor::Green,
        FString::Printf(TEXT("Health: %.0f"), Health));
}
```

#### Key 슬롯 관리

- Key가 같으면 같은 줄 덮어씀 — **매 프레임 갱신 가능** (값이 바뀌는 표시)
- Key가 다르면 새 줄 추가 — **여러 슬롯**으로 다양한 정보 동시 표시
- Key `-1`은 항상 새 줄 (덮어쓰기 X) — 로그 누적

예시: 다른 정보는 다른 Key
```cpp
GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Green, TEXT("Path OK"));
GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Cyan, TEXT("NavMesh OK"));
GEngine->AddOnScreenDebugMessage(3, 0.f, FColor::Yellow, TEXT("Enemies: 5"));
```

## 표면 z-fighting 회피 — Z 오프셋 관습

지형 표면, NavMesh, 바닥 등 거의 같은 z 좌표에 선을 그리면:
- 같은 z 평면 → 깊이 비교에서 경합 → 깜빡임 또는 묻힘 (z-fighting)
- 시각화가 잘 안 보이거나 화면마다 다르게 표시됨

**해결**: 약간 위로 올려서 그림
```cpp
const FVector Z_OFFSET(0, 0, 10);   // 10cm 위
DrawDebugLine(GetWorld(),
    PathPoint1.Location + Z_OFFSET,
    PathPoint2.Location + Z_OFFSET,
    FColor::Green, false, -1.f, 0, 5.f);
```

오프셋 크기:
- **5~10cm**: 일반 표면 (지형, NavMesh)
- **20~30cm**: 큰 객체 위 (캐릭터 머리 위 표시 등)
- 너무 크면 부정확해 보이고, 너무 작으면 여전히 묻힐 수 있음

## 구체 segments — 성능 절충

```cpp
DrawDebugSphere(GetWorld(), Center, 15.f /*radius*/, 6 /*segments*/, Color, false, -1.f);
//                                                   ^^^
```

| segments | 모양 | 용도 |
|---|---|---|
| 4 | 마름모 (저해상도) | 빠른 위치 마커 |
| **6~8** | **다각형 (디버그 표준)** | **일반 시각화** |
| 12+ | 매끄러운 구 | 정밀 시각화 (드물게) |
| 24+ | 거의 완벽한 구 | 거의 안 씀 (비용만 큼) |

매 프레임 N개 그릴 때 segments 차이가 합산되어 성능 영향. **저해상도가 디버그 표준**.

## 색상 관습 (표준)

| 색상 | 의미 | 사용 예 |
|---|---|---|
| **Green** | 성공, 정상, OK | 경로 찾음, NavMesh 투사 성공, AI 시야 내 적 |
| **Yellow** | 중간, 웨이포인트, 경고 | 경로 경유점, 주의 영역, AI 미식별 적 |
| **Red** | 실패, 위험, 에러 | 경로 실패, 충돌 영역, 적 hit |
| **Cyan** | 정보, 하이라이트 | NavMesh 투사 위치, 디버그 마커 |
| **Magenta** | 디버그 강조, 임시 | 임시 표시, "여기 봐" 마커 |
| **White** | 일반, 기본 | 정보 표시, 라벨 |
| **Blue** | 물 / 시원함 / 보조 | NavArea_Water, 보조 정보 |
| **Orange** | 경고, 위협 | 곧 위험할 영역, AI 경계 |

이 표준은 강제는 아니지만 **팀/엔진 코드 전반에서 동일하게 쓰면 디버그 화면 가독성** ↑.

## Pitfalls

### 증상: DrawDebug가 PIE에선 보이는데 Standalone에선 안 보임

- 원인: `UE_BUILD_SHIPPING` 빌드에선 DrawDebug 함수가 컴파일 제외
- 확인: Development/Debug 빌드인지 확인
- 해결: 게임 출시 빌드용 시각화는 별도 메커니즘 (UI 위젯 등)

### 증상: PIE 끝나도 선이 화면에 남음

- 원인: `bPersistentLines = true`로 그린 선
- 해결: `FlushPersistentDebugLines(GetWorld())` 호출. 또는 처음부터 false + Tick 갱신

### 증상: AddOnScreenDebugMessage가 화면에 안 보임

- 원인 1: `GEngine` nullptr (모듈 로드 전 등)
- 원인 2: 콘솔에서 `DisplayAll` 비활성화
- 원인 3: PIE 시작 직후 GEngine null 가능 — `if (GEngine)` 가드 필수
- 확인: `if (GEngine) ...` 패턴 적용

### 증상: 선이 깜빡이거나 묻혀서 안 보임 (지형 위)

- 원인: z-fighting (선이 표면과 같은 z)
- 해결: Z+10cm 오프셋 (위 §"표면 z-fighting 회피")

### 증상: Tick에서 매 프레임 그리는데 화면이 누적됨

- 원인: `LifeTime`을 0.f가 아닌 양수로 설정 → 매 프레임 호출 × 지속 시간 = 누적
- 확인: `DrawDebugLine(..., false, -1.f, ...)` 또는 `DrawDebugLine(..., false, 0.f, ...)`로 1프레임만
- 해결: Tick 갱신이면 LifeTime = `-1.f` (또는 작은 양수), bPersistentLines = false

### 증상: AddOnScreenDebugMessage 메시지가 매 프레임 새로 쌓임

- 원인: Key를 매번 다르게 (또는 -1로) 설정
- 해결: 고정 Key 지정 — 같은 정보면 같은 Key로 덮어쓰기

## References

- 출처: #19 NavDebugVisualizer 단계 3
- 관련 실무 문서: [`NavigationSystem/PathQuery.md`](NavigationSystem/PathQuery.md) (DrawDebug를 NavMesh 시각화에 적용한 예)
- 엔진 소스:
  - `Engine/Source/Runtime/Engine/Public/DrawDebugHelpers.h` — DrawDebug API 전체
  - `Engine/Source/Runtime/Engine/Classes/Engine/Engine.h` — `AddOnScreenDebugMessage`
