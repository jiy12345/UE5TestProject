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

### 2.1 기본 개념

인접한 두 폴리곤이 공유하는 에지를 **포탈(portal)**이라 합니다.
퍼널 알고리즘은 시작점에서 각 포탈을 바라보며, 시야각(퍼널)을 점진적으로 좁혀가며
방향 전환이 필요한 꼭짓점만 웨이포인트로 추출합니다.

```
시작점(Apex)에서 바라본 퍼널:

           Left₁ ────── Left₂
          /                   \
   Apex ◄                     ► 목표
          \                   /
           Right₁ ───── Right₂

퍼널이 뒤집히면(Left가 Right를 넘어가면) → 꼭짓점을 웨이포인트로 확정
```

### 2.2 핵심 판정: dtTriArea2D

퍼널의 좁힘/뒤집힘은 **2D 삼각형 면적의 부호**로 판정합니다.

```cpp
// dtTriArea2D(a, b, c) = (b-a) × (c-a) 의 z성분
// > 0: c가 a→b 직선의 왼쪽
// = 0: 일직선
// < 0: c가 a→b 직선의 오른쪽
```

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
