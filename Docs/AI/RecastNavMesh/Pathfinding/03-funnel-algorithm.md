# 03. 퍼널(Funnel) 알고리즘 — findStraightPath

> **작성일**: 2026-04-16
> **엔진 버전**: UE 5.5

## 1. 개요

A* 탐색의 결과인 **폴리곤 코리더**(polygon corridor)는 "어떤 폴리곤을 순서대로 통과하는가"만 알려줍니다.
실제 이동에 필요한 **웨이포인트**(방향 전환 지점)를 생성하는 것이 `findStraightPath()`의 역할입니다.

이 함수는 **Simple Stupid Funnel Algorithm (SSFA)**을 구현합니다.

```
입력: startPos, endPos, path[] (폴리곤 코리더)
출력: dtQueryResult (웨이포인트 좌표 + 플래그 목록)
```

> **소스 확인 위치**
> - `Engine/Source/Runtime/Navmesh/Private/Detour/DetourNavMeshQuery.cpp:2538` — `findStraightPath()` 함수

---

## 2. 퍼널 알고리즘 원리

### 2.1 목표를 한 문장으로

**"폴리곤 코리더를 지나면서, 가능한 한 회전(꺾임) 없이 가는 최단 경로를 찾는다."**

2D 평면에서 최단 경로는 직선입니다. 코리더 안에서 완전한 직선이 불가능한 이유는 폴리곤 경계가 길을 막기 때문. 그래서 "길을 막는 꼭짓점마다 최소한으로 꺾어서 목표까지 간다" — 이게 퍼널 알고리즘이 하는 일입니다.

### 2.2 핵심 개념 3가지

| 개념 | 정의 | 예시 |
|------|------|------|
| **포탈(Portal)** | 인접한 두 폴리곤이 공유하는 **에지(선분)** | 두 사각형 방이 붙어 있다면 그 경계 선분 |
| **꼭짓점(Vertex)** | 포탈의 **양 끝점** (왼쪽 끝 / 오른쪽 끝) | 포탈 에지의 두 점 |
| **Apex** | 현재까지 경로에서 **"마지막으로 확정된 웨이포인트"**. 다음 구간의 시작점이자 시야를 쏘는 눈의 위치 | 처음엔 startPos, 꺾일 때마다 갱신 |

**시작점과 목표점은 에지가 아니라 점**입니다. 시작점은 첫 번째 Apex이자 첫 번째 웨이포인트, 목표점은 마지막 웨이포인트. 중간에 만나는 **포탈들만 에지**입니다.

```
코리더를 시야 쪽에서 본 모습:

 startPos ────[ Polygon A ]──── Portal₁ ────[ Polygon B ]──── Portal₂ ────[ Polygon C ]──── endPos
     ●                            | |                            | |                            ●
   (Apex)                       L₁──R₁                         L₂──R₂
                               (left₁)(right₁)                (left₂)(right₂)
```

각 포탈에는 **왼쪽 끝(Left)**과 **오른쪽 끝(Right)**이 있습니다. Apex에서 각 포탈의 L과 R을 바라보는 두 시선이 "깔때기(funnel)" 형태를 이룹니다.

### 2.3 왜 "방향 전환"이 필요해지는가

코리더가 직선이라면 퍼널이 좁아지기만 하고 끝까지 통과할 수 있어 **웨이포인트는 시작/끝 2개만** 나옵니다.

하지만 코리더가 휘어 있으면 어느 순간 **다음 포탈이 지금까지의 시야 바깥으로 튀어나옵니다.** 이 순간은 "지금의 Apex에서 직선으로는 그 포탈을 통과할 수 없다"는 뜻. 즉 길이 꺾인다는 신호입니다.

이때 알고리즘은 **지금 시야를 만들어낸 꼭짓점(마지막까지 유효했던 L 또는 R)**을 **새 웨이포인트로 확정**하고, 그 꼭짓점을 새로운 Apex로 삼아 다시 시야를 쏩니다. 이게 "방향 전환"이고, 확정된 그 꼭짓점이 "방향 전환이 필요한 꼭짓점"입니다.

```
단순 예: ㄱ 자 모양 코리더

 start ─────── Portal₁ ─────── Portal₂ ─────── end
                                 │                
                    Portal₃ ─────┤
                                 │
                               end

직선 경로는 꼭짓점 V에 막혀서 불가능 → V가 웨이포인트로 확정
  start ─────*(V)─────→ end
             ↑
      방향 전환 꼭짓점
```

### 2.4 핵심 판정: dtTriArea2D

퍼널이 좁아지는지 뒤집히는지 판정하려면 **"점이 어느 쪽에 있는가"**를 빠르게 알아야 합니다.
`dtTriArea2D`는 이걸 **2D 외적(signed area)**으로 계산합니다.

```cpp
// a, b, c는 3개의 2D 점 (XY 평면, Z 무시)
// 반환값은 세 점이 만드는 삼각형의 "부호 있는 면적 × 2"
dtReal dtTriArea2D(const dtReal* a, const dtReal* b, const dtReal* c);

// 계산식: (b.x - a.x) * (c.z - a.z) - (c.x - a.x) * (b.z - a.z)
//        ← 벡터 (b-a) × (c-a) 의 y성분 (UE는 Y가 상방이지만 Recast는 2D 투영이므로 z축을 z로 사용)
```

**기하학적 의미**:

| 반환값 | 의미 |
|--------|------|
| `> 0` | **c가 a→b 직선의 왼쪽(CCW)** 에 있음 |
| `= 0` | 세 점이 **일직선** 위에 있음 |
| `< 0` | **c가 a→b 직선의 오른쪽(CW)** 에 있음 |

```
      c (> 0, 왼쪽)
       │
   a ──┼──→ b
       │
      c (< 0, 오른쪽)
```

**퍼널에서의 활용**:
- `dtTriArea2D(apex, portalRight, newRight)` ≤ 0 → 새 Right가 기존 Right보다 **안쪽(좁게)** 쪽에 있음 → 퍼널 좁힘 가능
- `dtTriArea2D(apex, portalLeft, newRight)` > 0 → 새 Right가 **Left를 넘어 반대편으로 튀어나감** → 퍼널 뒤집힘 → 방향 전환!

이 한 줄짜리 연산으로 "점이 시야 안인지, 시야 밖인지, 반대편으로 넘어갔는지"를 O(1)로 판정합니다. 삼각함수/각도 비교보다 훨씬 빠릅니다.

### 2.5 퍼널이 "뒤집힌다"는 것 — 시각적 + 수학적

**시각적 설명**:

```
[1] 초기 퍼널 (좁아지는 정상 상태)
                                 
      L ◀── 왼쪽 경계                  
       ╲                               
        ╲                              
  Apex ● ──────────── 목표 방향         
        ╱                              
       ╱                               
      R ◀── 오른쪽 경계                


[2] 새 포탈(L', R') 평가 — 좁아지는 경우
                                 
      L                L'               
       ╲             ╱  (L' 안쪽으로 들어옴)
        ╲          ╱    → L 갱신: L ← L'
  Apex ● ────── 
        ╲          ╲   
         ╲          R' (R'도 안쪽)
          R              → R 갱신: R ← R'


[3] 뒤집힘(inversion) — 새 R'이 L보다 왼쪽으로 튀어나옴
                                 
       R' ← 새 Right가 Left보다 왼쪽!
      ╱                               
      ╱                               
     L                              
     │                              
  Apex ●                            
     │                              
      ╲                             
       ╲                            
        R                           

→ 직선으로 그려보면 R'이 L의 왼쪽을 지나감
→ Apex에서 R'으로 가려면 반드시 L 꼭짓점을 "안고 돌아야" 함
→ 결정: L을 웨이포인트로 확정, L을 새 Apex로 설정, 처음부터 다시 시야 쏘기
```

**수학적 설명 (dtTriArea2D 부호 기준)**:

퍼널이 정상(좁아지는) 상태일 때 세 조건:
```
dtTriArea2D(apex, portalLeft, portalRight) ≥ 0   // Right가 Left의 오른쪽 (퍼널 방향 유지)
```

새 Right 후보 `R'`을 평가할 때:
```
조건 A: dtTriArea2D(apex, portalRight, R') ≤ 0
   → R'이 portalRight의 오른쪽이거나 일직선 = R이 더 안쪽으로 좁힐 수 있는 후보

조건 B: dtTriArea2D(apex, portalLeft, R') > 0
   → R'이 portalLeft의 왼쪽으로 튀어나감 = ★ 뒤집힘 발생 ★
```

조건 A + 조건 B가 동시에 성립하면 수학적으로 "R'이 퍼널 안으로 들어오는 게 아니라 Left의 반대편으로 튀어나간다"는 의미. 이 순간:

```cpp
// DetourNavMeshQuery.cpp:2710-2738 (오른쪽 뒤집힘 케이스)
// portalLeft를 웨이포인트로 확정
appendVertex(portalLeft, flags, leftPolyRef, result);

// portalLeft를 새 Apex로 설정
dtVcopy(portalApex, portalLeft);
apexIndex = leftIndex;

// 퍼널 초기화
dtVcopy(portalLeft, portalApex);
dtVcopy(portalRight, portalApex);

// 해당 인덱스부터 다시 순회
i = apexIndex;
continue;
```

왼쪽 뒤집힘도 대칭으로 처리됩니다 (Right 꼭짓점을 웨이포인트로 확정, Right을 새 Apex).

> **직관 요약**: "뒤집힘"은 "직선 시야 안에 다음 포탈을 담을 수 없다"는 기하학적 신호이며, 수학적으로는 두 외적의 부호가 서로 어긋나는 순간을 잡아냅니다.

---

## 3. findStraightPath() 단계별 분석

### 3.1 초기화 (라인 2538-2579)

```cpp
// DetourNavMeshQuery.cpp:2538-2579
dtStatus dtNavMeshQuery::findStraightPath(
    const dtReal* startPos, const dtReal* endPos,
    const dtPolyRef* path, const int pathSize,
    dtQueryResult& result, const int options) const
{
    // 시작점/끝점을 폴리곤 경계에 투사 (클램핑)
    dtReal closestStartPos[3], closestEndPos[3];
    closestPointOnPolyBoundary(path[0], startPos, closestStartPos);
    closestPointOnPolyBoundary(path[pathSize-1], endPos, closestEndPos);
    
    // 시작점을 첫 번째 웨이포인트로 추가
    appendVertex(closestStartPos, DT_STRAIGHTPATH_START, path[0], result);
    
    // 퍼널 초기화
    dtReal portalApex[3], portalLeft[3], portalRight[3];
    dtVcopy(portalApex, closestStartPos);     // 퍼널 꼭짓점 = 시작점
    dtVcopy(portalLeft, portalApex);          // 왼쪽 경계 = 시작점
    dtVcopy(portalRight, portalApex);         // 오른쪽 경계 = 시작점
    int apexIndex = 0, leftIndex = 0, rightIndex = 0;
```

### 3.2 포탈 순회 (라인 2589-2634)

코리더의 각 폴리곤 쌍에 대해 공유 에지(포탈)의 양 끝점을 가져옵니다:

```cpp
// DetourNavMeshQuery.cpp:2589-2626
for (int i = 0; i < pathSize; ++i)
{
    dtReal left[3], right[3];
    unsigned char fromType, toType;
    
    if (i+1 < pathSize)
    {
        // 다음 포탈의 왼쪽/오른쪽 끝점 가져오기
        getPortalPoints(path[i], path[i+1], left, right, fromType, toType);
        
        // 시작 직후 포탈이 너무 가까우면 건너뛰기
        if (i == 0 && toType == DT_POLYTYPE_GROUND)
        {
            dtReal t;
            if (dtDistancePtSegSqr2D(portalApex, left, right, t) < dtSqr(0.001f))
                continue;
        }
    }
    else
    {
        // 마지막: 끝점을 left/right 모두에 설정
        dtVcopy(left, closestEndPos);
        dtVcopy(right, closestEndPos);
    }
```

### 3.3 퍼널 좁히기 — 오른쪽 (라인 2699-2738)

```cpp
// DetourNavMeshQuery.cpp:2699-2738
// 오른쪽 경계 업데이트 시도
if (dtTriArea2D(portalApex, portalRight, right) <= 0.0f)
{
    // Case A: 새 Right가 기존 Right보다 안쪽 → 퍼널 좁히기
    if (dtVequal(portalApex, portalRight) 
        || dtTriArea2D(portalApex, portalLeft, right) > 0.0f)
    {
        dtVcopy(portalRight, right);   // 오른쪽 경계 업데이트
        rightPolyRef = (i+1 < pathSize) ? path[i+1] : 0;
        rightIndex = i;
    }
    // Case B: 새 Right가 Left를 넘어감 → 퍼널 뒤집힘!
    else
    {
        // Left 꼭짓점을 웨이포인트로 확정
        appendVertex(portalLeft, flags, leftPolyRef, result);
        
        // 퍼널을 Left 꼭짓점에서 재시작
        dtVcopy(portalApex, portalLeft);
        apexIndex = leftIndex;
        dtVcopy(portalLeft, portalApex);
        dtVcopy(portalRight, portalApex);
        leftIndex = apexIndex;
        rightIndex = apexIndex;
        
        i = apexIndex;  // 해당 인덱스부터 다시 순회
        continue;
    }
}
```

### 3.4 퍼널 좁히기 — 왼쪽 (라인 2740-2780)

오른쪽과 대칭적인 로직입니다:

```cpp
// DetourNavMeshQuery.cpp:2740+ (오른쪽과 대칭)
if (dtTriArea2D(portalApex, portalLeft, left) >= 0.0f)
{
    // Case A: 새 Left가 기존 Left보다 안쪽 → 퍼널 좁히기
    if (dtVequal(portalApex, portalLeft)
        || dtTriArea2D(portalApex, portalRight, left) < 0.0f)
    {
        dtVcopy(portalLeft, left);     // 왼쪽 경계 업데이트
        leftIndex = i;
    }
    // Case B: 새 Left가 Right를 넘어감 → 퍼널 뒤집힘!
    else
    {
        // Right 꼭짓점을 웨이포인트로 확정
        appendVertex(portalRight, flags, rightPolyRef, result);
        
        // 퍼널을 Right 꼭짓점에서 재시작
        dtVcopy(portalApex, portalRight);
        apexIndex = rightIndex;
        // ... (재시작)
        
        i = apexIndex;
        continue;
    }
}
```

### 3.5 시각적 이해

```
Step 1: 초기 상태               Step 2: 포탈₁로 퍼널 좁히기
                                
    L₀     R₀                      L₁     R₀
    |       |                      /       |
    Apex ──────                  Apex ──────
                                  (L 업데이트, R 유지)

Step 3: 포탈₂에서 뒤집힘 발생    Step 4: 웨이포인트 확정
                                
    L₁     R₂                  L₁ = 새 Apex(웨이포인트)
      \   /                       |
       \ /                     새 퍼널 시작
        X (Left가 Right를 넘음)
```

---

## 4. 포탈 포인트 계산

`getPortalPoints()`는 두 인접 폴리곤이 공유하는 에지의 양 끝점을 반환합니다.

### 4.1 일반 폴리곤 (DT_POLYTYPE_GROUND)

공유하는 에지의 두 정점을 left/right로 반환합니다.
타일 경계를 걸치는 경우 `bmin`/`bmax` (0-255로 정규화된 포탈 폭)로 에지를 클램핑합니다.

### 4.2 오프메시 포인트 연결 (DT_POLYTYPE_OFFMESH_POINT)

점프대, 순간이동 등의 포인트 연결에서는 left와 right가 동일한 점입니다.

### 4.3 오프메시 세그먼트 연결 (DT_POLYTYPE_OFFMESH_SEGMENT, UE 확장)

UE가 추가한 **세그먼트 오프메시 링크**는 4개의 정점(시작 에지 2개 + 끝 에지 2개)을 가집니다.
진입점에서 가장 가까운 출구점을 `dtVlerp()`로 계산하여 포탈을 잠급니다(lock):

```cpp
// DetourNavMeshQuery.cpp:2639-2648
if (fromType == DT_POLYTYPE_OFFMESH_SEGMENT)
{
    dtDistancePtSegSqr2D(segmentLinkEntryPoint, left, right, segt);
    dtReal lockedPortal[3];
    dtVlerp(lockedPortal, left, right, segt);
    dtVcopy(left, lockedPortal);
    dtVcopy(right, lockedPortal);  // 점으로 축소
}
```

> **소스 확인 위치**
> - `getPortalPoints()`: `DetourNavMeshQuery.cpp:3099-3251`
> - 세그먼트 링크 처리: `DetourNavMeshQuery.cpp:2637-2696`

---

## 5. 웨이포인트 플래그

각 웨이포인트에는 유형을 나타내는 플래그가 부여됩니다:

| 플래그 | 값 | 의미 |
|--------|-----|------|
| `DT_STRAIGHTPATH_START` | 0x01 | 경로 시작점 |
| `DT_STRAIGHTPATH_END` | 0x02 | 경로 끝점 |
| `DT_STRAIGHTPATH_OFFMESH_CONNECTION` | 0x04 | 오프메시 연결 진입점 |

---

## 6. 옵션: 영역 교차점 추가

`findStraightPath()`의 `options` 파라미터로 추가 웨이포인트 생성을 제어합니다:

| 옵션 | 효과 |
|------|------|
| `DT_STRAIGHTPATH_AREA_CROSSINGS` | **영역이 바뀌는** 포탈에 웨이포인트 추가 |
| `DT_STRAIGHTPATH_ALL_CROSSINGS` | **모든** 포탈에 웨이포인트 추가 |

이 옵션이 활성화되면 `appendPortals()`가 호출되어, 직선 경로와 포탈 에지의 교차점을 계산합니다:

```cpp
// DetourNavMeshQuery.cpp:2711-2716
if (options & (DT_STRAIGHTPATH_AREA_CROSSINGS | DT_STRAIGHTPATH_ALL_CROSSINGS))
{
    stat = appendPortals(apexIndex, leftIndex, portalLeft, path, result, options);
}
```

> **소스 확인 위치**
> - `appendPortals()`: `DetourNavMeshQuery.cpp:2448-2510`
> - `appendVertex()`: `DetourNavMeshQuery.cpp:2427-2446`

---

## 7. 알고리즘 복잡도

| 항목 | 복잡도 | 설명 |
|------|--------|------|
| 시간 | O(n²) worst, O(n) amortized | n = 코리더 폴리곤 수. 뒤집힘 시 인덱스 되감기 발생 |
| 공간 | O(n) | 결과 웨이포인트 수 ≤ 코리더 폴리곤 수 |

퍼널이 뒤집힐 때 `i = apexIndex`로 되감기하지만, 전체적으로 각 포탈은 최대 2번만 처리되므로
분할상환(amortized) 기준으로 O(n)입니다.
