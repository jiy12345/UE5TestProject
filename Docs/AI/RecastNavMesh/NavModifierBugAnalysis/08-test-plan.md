# DynamicModifiersOnly 쿠킹 빌드 Nav Modifier 미동작 — 상세 테스트 플랜

> **작성일**: 2026-02-17
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#9](https://github.com/jiy12345/UE5TestProject/issues/9)
> **선행 문서**: [소스 코드 심층 분석](06-source-analysis-detail.md), [PIE vs 쿠킹 빌드 차이](07-pie-vs-cooked-loading.md), [테스트 계획 (개요)](nav-modifier-cooked-build-test-plan.md)
> **최종 수정**: 2026-02-17 — 실제 테스트 환경 기반으로 좌표/설정 반영

---

## 목적

DynamicModifiersOnly 모드에서 데이터 레이어에 속한 Nav Modifier가 **PIE에서는 동작하지만 쿠킹 빌드에서는 동작하지 않는 문제**의 원인을 체계적으로 검증한다.

---

## 작업 유형 범례

각 단계마다 아래 태그로 작업 유형을 표기한다:

| 태그 | 의미 | 도구 |
|------|------|------|
| `[에디터]` | 언리얼 에디터에서 수동 작업 (레벨 디자인, 설정) | UE5 에디터 |
| `[C++]` | C++ 소스 코드 작성/수정 | IDE (Visual Studio, Rider 등) |
| `[BP]` | 블루프린트 에셋 생성/설정 | UE5 에디터 Blueprint Editor |
| `[Config]` | ini 파일 또는 실행 인자 설정 | 텍스트 에디터 |
| `[Console]` | 런타임 콘솔 명령 | 게임 내 콘솔 (`~` 키) |
| `[확인]` | 시각적/로그 확인 작업 | 눈 또는 로그 뷰어 |

---

## Step 1: 테스트 맵 생성 `[에디터]`

1. Content Browser → 우클릭 → Level → **Empty Open World** 템플릿으로 World Partition 레벨 생성
   - 일반 레벨이 아닌 **World Partition 레벨**이어야 데이터 레이어가 동작함
   - 생성 후 World Settings에 **World Partition** 섹션이 보이면 정상
2. 이름: `L_NavModifierTest`
3. 맵을 열고 World Settings 확인:
   - Navigation System Config가 기본값인지 확인 (기본값이면 RecastNavMesh 사용)
   - 새로 만든 레벨이면 기본값 그대로 두면 됨

---

## Step 2: 데이터 레이어 생성 `[에디터]`

데이터 레이어 2개를 생성한다. **에셋을 먼저 만든 뒤** Outliner에서 인스턴스를 생성해야 한다.

| 데이터 레이어 이름 | 용도 | Initial Runtime State |
|---------------------|------|----------------------|
| `DL_NavModifier` | Nav Modifier Volume 배치용 | **Unloaded** |
| `DL_Reference` | 비교 검증용 (모디파이어 없음) | **Unloaded** |

**생성 방법**:
1. **Data Layer Asset 생성**: Content Browser → 우클릭 → Data Layer → Data Layer Asset → 이름 지정 (`DL_NavModifier`, `DL_Reference`)
2. **Data Layer Instance 생성**: Outliner → Data Layers 탭 → 우클릭 → Create Data Layer → 위에서 만든 에셋 참조
3. 각 레이어 선택 → Details → `Initial Runtime State` → **Unloaded**

---

## Step 3: 바닥 및 네비메시 볼륨 배치 `[에디터]`

### 3-1. 바닥

Empty Open World 템플릿으로 생성한 경우 **랜드스케이프가 이미 바닥으로 깔려 있으므로** 별도 바닥 메시를 추가할 필요 없음. 랜드스케이프 위에 네비메시가 정상 생성된다.

일반 레벨인 경우:
1. Place Actors → Basic → **Cube** 또는 **Plane** 드래그 배치
2. 스케일: 바닥으로 쓸 수 있을 만큼 넓게 (예: Cube를 `(100, 100, 1)`로 스케일)
3. **데이터 레이어 할당 안 함** → 항상 로드되는 base geometry

### 3-2. NavMeshBoundsVolume

1. Place Actors → Volumes → **NavMeshBoundsVolume** 드래그 배치
2. 크기: 바닥 전체 + Nav Modifier 배치 영역을 충분히 포함
3. NavMeshBoundsVolume을 배치하면 `RecastNavMesh-Default` 액터가 자동 생성됨

---

## Step 4: RecastNavMesh 설정 `[에디터]`

Step 3에서 자동 생성된 `RecastNavMesh-Default` 액터 선택 → Details 패널:

| 속성 | 값 | 이유 |
|------|------|------|
| `Runtime Generation` | **DynamicModifiersOnly** | 테스트 대상 모드 |

나머지 속성(Cell Size, Agent Radius 등)은 기본값 유지.

---

## Step 5: Nav Modifier Volume 배치 `[에디터]`

### 5-1. 테스트 대상 Volume (DL_NavModifier 레이어)

Place Actors → Volumes → **Nav Modifier Volume** 으로 3개 배치:

| 이름 | Bounds Min | Bounds Max | Area Class | 데이터 레이어 |
|------|-----------|-----------|------------|---------------|
| `NavMod_A` | (7940, 12230, -80) | (8140, 12430, 120) | `NavArea_Null` | `DL_NavModifier` |
| `NavMod_B` | (10420, 12410, -100) | (10620, 12610, 100) | `NavArea_Null` | `DL_NavModifier` |
| `NavMod_C` | (9170, 12990, -70) | (9370, 13190, 130) | `NavArea_Null` | `DL_NavModifier` |

> 위 좌표는 실제 배치 후 PIE 로그(MarkDirtyTiles)에서 확인한 값. 배치 위치는 자유이며, 실제 좌표를 Step 12의 TestPoints에 반영하면 됨.

**각 Volume 배치 후 설정**:
1. Details → **Area Class** → `NavArea_Null` (이동 불가 영역 = 시각적으로 확인 용이)
2. Details → **Data Layer Assets** → `DL_NavModifier` 에셋 할당
3. Details → 아래 속성이 기본값인지 확인:

| 속성 | 값 | 이유 |
|------|------|------|
| `Fill Collision Underneath For Navmesh` | **false** | Geometry 플래그 방지 (원인 3 배제) |
| `Mask Fill Collision Underneath For Navmesh` | **false** | Geometry 플래그 방지 |
| `Nav Mesh Resolution` | **None** (코드상 `Invalid`) | Geometry 플래그 방지 |

### 5-2. 대조군 Volume (데이터 레이어 없음)

| 이름 | Bounds Min | Bounds Max | Area Class | 데이터 레이어 |
|------|-----------|-----------|------------|---------------|
| `NavMod_AlwaysLoaded` | (8820, 11290, -120) | (9020, 11490, 80) | `NavArea_Null` | **없음** |

### 5-3. 시각적 마커 배치 (선택)

각 Nav Modifier 위치에 작은 Cube (밝은 색 머티리얼) 배치 → 런타임에서 위치 식별용. 데이터 레이어 없이 항상 로드.

---

## Step 6: World Settings BaseNavmeshDataLayers 확인 `[에디터]`

**이 확인은 원인 2의 사전 배제이자, 테스트 셋업의 정합성 검증이다.**

1. World Settings → Navigation → **Base Navmesh Data Layers** 배열 확인
2. `DL_NavModifier`가 이 배열에 **포함되어 있으면 안 된다**
3. `DL_Reference`도 **포함되어 있으면 안 된다**

```
✅ 올바른 상태: 배열이 비어있거나, 테스트와 무관한 레이어만 포함
❌ 잘못된 상태: DL_NavModifier가 포함됨 → 반드시 제거
```

**BaseNavmeshDataLayers의 의미**:

| BaseNavmeshDataLayers | PIE | 쿠킹 빌드 |
|---|---|---|
| **포함됨** | 모디파이어 효과가 base navmesh에 구워짐. 런타임 토글 무시됨 | 동일 |
| **포함 안 됨** | 런타임 토글 정상 동작 | 버그 발생 가능 (테스트 대상) |

> **만약 포함되어 있다면**: 이것만으로 **원인 2 확정**. 제거 후 진행.

---

## Step 7: 에디터 네비메시 빌드 확인 `[에디터]` `[확인]`

### 7-1. DL_NavModifier 에디터 가시성 끄기

DynamicModifiersOnly 모드에서는 에디터에서 빌드한 네비메시가 base navmesh로 사용된다. DL_NavModifier의 Nav Modifier가 에디터에서 보이는 상태로 빌드하면 그 효과가 base navmesh에 구워져 버린다.

**Data Layers 창 → `DL_NavModifier` → Editor → `Is Initially Loaded` → false**

이렇게 하면 에디터에서 해당 레이어의 액터가 로드되지 않아 base navmesh에 포함되지 않는다.

> **주의**: 이 상태에서는 해당 액터를 편집할 수 없으므로, 배치/설정을 모두 완료한 뒤에 끌 것.

### 7-2. 네비메시 확인

1. 에디터 뷰포트에서 **Show → Navigation** 켜기 (단축키: `P`)
2. 확인 사항:

| 위치 | 기대 상태 | 비정상이면? |
|------|-----------|-------------|
| 바닥 전체 | 녹색 네비메시 생성됨 | NavMeshBoundsVolume 크기/위치 재확인 |
| `NavMod_AlwaysLoaded` 영역 | **빈 구멍** (NavArea_Null) | Area Class 설정 재확인 |
| `NavMod_A/B/C` 영역 | 네비메시 **정상 채워짐** (레이어 언로드) | DL_NavModifier의 Editor Is Initially Loaded가 false인지 확인 |

---

## Step 8: PIE 사전 검증 `[에디터]` `[Console]` `[확인]`

코드 수정 없이, PIE에서 데이터 레이어 토글이 네비메시에 정상 반영되는지 확인한다.

### 8-1. PIE 실행

에디터에서 **Alt+P** 또는 Play 버튼으로 PIE 시작.

### 8-2. 데이터 레이어 상태 확인 `[Console]`

```
wp.Runtime.DumpDataLayers
```

`DL_NavModifier`가 `Unloaded` 상태인지 확인.

### 8-3. 데이터 레이어 활성화 `[Console]`

```
wp.Runtime.ToggleDataLayerActivation DL_NavModifier
```

### 8-4. 결과 확인 `[확인]`

1~2초 대기 후:

| 위치 | 기대 결과 |
|------|-----------|
| `NavMod_AlwaysLoaded` | 처음부터 빈 구멍 ✓ |
| `NavMod_A` | 활성화 후 빈 구멍 ✓ |
| `NavMod_B` | 활성화 후 빈 구멍 ✓ |
| `NavMod_C` | 활성화 후 빈 구멍 ✓ |

**모두 정상이면**: PIE 동작 확인 완료 → Step 9로.
**비정상이면**: 에디터 설정(Step 1~7)에 문제 있음 → 되돌아가서 수정.

---

## Step 9: 로그 설정 `[Config]`

`Game/Config/DefaultEngine.ini`에 다음 추가:

```ini
[Core.Log]
LogNavigationDirtyArea=VeryVerbose
LogNavigation=VeryVerbose
LogNavOctree=Verbose
```

이 설정은 PIE와 쿠킹 빌드 모두에 적용된다.

---

## Step 10: PIE 로그 확인 (베이스라인) `[에디터]` `[확인]`

Step 9의 로그 설정 후 PIE를 다시 실행하고 데이터 레이어를 토글한다. Output Log에서 다음을 확인:

### 10-1. 정상 동작 시 반드시 출력되어야 하는 로그

```
검색어: "AddAreas called: Flags=2" (Flags=2 = DynamicModifier)
→ Nav Modifier의 dirty area가 DynamicModifier 플래그로 추가됨
```

```
검색어: "MarkDirtyTiles PASSED"
→ dirty tile 처리가 정상 통과됨
```

### 10-2. 출력되면 안 되는 로그

```
검색어: "Ignoring dirtyness"
→ 이것이 출력되면 원인 2 (BaseNavmeshDataLayers) — Step 6으로 돌아가서 확인
```

### 10-3. PIE 베이스라인에서 주목할 값

```
bUseWPDynamicMode=0  — PIE에서는 WP 동적 모드 필터가 꺼져 있음
bIsFromVisibilityChange=0  — PIE에서는 visibility change로 취급 안 됨
```

> 쿠킹 빌드에서는 이 값들이 달라질 수 있으며, 그것이 버그의 원인일 수 있음.

### 10-4. 베이스라인 로그 저장

PIE의 정상 로그를 **텍스트 파일로 저장** → 나중에 쿠킹 빌드 로그와 비교할 베이스라인.

---

## Step 11: 게임 코드 작성 — 데이터 레이어 토글 + 자동 검증 GameMode `[C++]`

쿠킹 빌드에서 **로그만으로 결과를 판단**할 수 있도록, 데이터 레이어 토글 후 자동으로 경로 쿼리를 수행하고 PASS/FAIL을 출력하는 GameMode를 만든다.

**구현 완료된 파일**:
- `Game/Source/UE5TestProject/NavTestGameMode.h` — `FNavTestPoint` 구조체 + `ANavTestGameMode` 클래스
- `Game/Source/UE5TestProject/NavTestGameMode.cpp` — 3단계 자동 테스트 (PRE-CHECK → ACTIVATE → VERIFY)

**주요 기능**:
- `DataLayerActivateDelay`: 데이터 레이어 활성화 딜레이 (초). `-NavTestDelay=` 실행 인자로 오버라이드 가능.
- `VerifyDelay`: 활성화 후 검증 대기 (초). `-NavTestVerifyDelay=` 로 오버라이드 가능.
- `NavModifierDataLayer`: `TSoftObjectPtr<UDataLayerAsset>` (BP에서 할당)
- `TestPoints`: `TArray<FNavTestPoint>` (BP에서 설정)
- 데이터 레이어 상태 조회: `GetDataLayerInstanceFromAsset()` → `GetDataLayerInstanceRuntimeState()`
- 경로 쿼리: `FindPathSync()` 로 Volume 관통 경로 쿼리 → 실패/우회 시 모디파이어 적용으로 판정

**Build.cs 수정 불필요** — `NavigationSystem`과 `Engine` 모듈이 이미 의존성에 포함.

---

## Step 12: GameMode 블루프린트 생성 및 에셋 할당 `[BP]`

### 12-1. Blueprint 서브클래스 생성

1. Content Browser → 우클릭 → Blueprint Class
2. Parent Class → `NavTestGameMode` (C++에서 만든 클래스)
3. 이름: `BP_NavTestGameMode`

### 12-2. 프로퍼티 할당

1. `BP_NavTestGameMode` 더블클릭 → Blueprint Editor 열기
2. Details 패널 (또는 Class Defaults):
   - `Data Layer Activate Delay` → `5.0` (기본값, 필요 시 변경)
   - `Verify Delay` → `5.0` (활성화 후 검증까지 대기)
   - `Nav Modifier Data Layer` → **`DL_NavModifier`** 에셋 선택
   - `Test Points` → 아래 4개 항목 추가:

#### Test Points 설정 (실제 배치 좌표 기반)

> 아래 좌표는 PIE 로그의 MarkDirtyTiles Bounds에서 계산한 값. Volume 반경 100 + 여유 150 = Center에서 X축 ±250 오프셋.

| Index | Name | Center | PathStart | PathEnd | bIsDataLayerModifier |
|-------|------|--------|-----------|---------|----------------------|
| 0 | `NavMod_A` | `(8040, 12330, 20)` | `(7790, 12330, 20)` | `(8290, 12330, 20)` | `true` |
| 1 | `NavMod_B` | `(10520, 12510, 0)` | `(10270, 12510, 0)` | `(10770, 12510, 0)` | `true` |
| 2 | `NavMod_C` | `(9270, 13090, 30)` | `(9020, 13090, 30)` | `(9520, 13090, 30)` | `true` |
| 3 | `NavMod_AlwaysLoaded` | `(8920, 11390, -20)` | `(8670, 11390, -20)` | `(9170, 11390, -20)` | `false` |

> **PathStart/PathEnd**: Volume 외부의 양쪽 지점. Volume을 관통하는 직선 경로를 쿼리하여, NavArea_Null이 적용되면 경로가 실패하거나 우회해야 정상.
> **좌표는 실제 배치 위치에 따라 달라짐.** 다시 배치한 경우 PIE 로그의 Bounds 값을 기반으로 재계산할 것.

### 12-3. World Settings에 GameMode 설정 `[에디터]`

1. `L_NavModifierTest` 맵 열기
2. World Settings → **GameMode Override** → `BP_NavTestGameMode` 선택

### 12-4. Project Settings 기본 맵 설정 `[에디터]`

쿠킹 빌드에서 기본 맵으로 사용하려면:
1. Project Settings → Maps & Modes → **Game Default Map** → `L_NavModifierTest`

또는 실행 시 커맨드라인으로 맵 이름을 지정:
```
UE5TestProject.exe L_NavModifierTest ...
```

---

## Step 13: 쿠킹 빌드 실행 및 버그 재현 `[에디터]` `[확인]`

### 13-1. 쿠킹

1. File → Package Project → Windows
2. Build Configuration: **Development** (로그 + 콘솔 사용 가능)
3. 패키징 완료 대기

### 13-2. 실행

```
UE5TestProject.exe L_NavModifierTest -log -LogCmds="LogNavigationDirtyArea VeryVerbose, LogNavigation VeryVerbose, LogNavOctree Verbose, LogTemp Warning"
```

### 13-3. 네비메시 시각화 `[Console]`

게임 시작 후 콘솔(`~` 키)에서:

```
show Navigation
```

### 13-4. 관찰 `[확인]`

1. 게임 시작 직후 — 네비메시 초기 상태 확인
2. `NavMod_AlwaysLoaded` 위치에 빈 구멍이 있는지 확인 (대조군)
3. 5초 후 `[NavTest] Data layer ... ACTIVATED!` 로그 확인
4. 추가 5~10초 대기
5. `NavMod_A/B/C` 위치의 네비메시 변경 여부 확인

### 13-5. 결과 판단

| 액터 | PIE 결과 | 쿠킹 빌드 결과 (정상) | 쿠킹 빌드 결과 (버그) |
|------|---------|-------------------|-------------------|
| `NavMod_AlwaysLoaded` | 빈 구멍 ✓ | 빈 구멍 ✓ | 빈 구멍 ✓ |
| `NavMod_A` | 빈 구멍 ✓ | 빈 구멍 ✓ | 변화 없음 ✗ |
| `NavMod_B` | 빈 구멍 ✓ | 빈 구멍 ✓ | 변화 없음 ✗ |
| `NavMod_C` | 빈 구멍 ✓ | 빈 구멍 ✓ | 변화 없음 ✗ |

**버그 재현 성공** → Step 14로.
**버그 재현 실패 (정상 동작)** → Step 5의 Nav Modifier 설정이 실제 프로젝트와 다를 수 있음. 실제 프로젝트의 Nav Modifier 속성을 복제하여 재테스트.

---

## Step 14: 쿠킹 빌드 로그 분석 `[확인]`

로그 파일 위치: `Saved/Logs/UE5TestProject.log`

### 14-1. Nav Modifier 등록 확인

```
검색어: "[registered]"
```

- 출력됨 → Nav Modifier 스폰 및 옥트리 등록 성공
- 출력 안 됨 → **데이터 레이어 로딩 자체가 실패** → 데이터 레이어 설정 재확인

### 14-2. dirty area 경로 확인

```
검색어: "Ignoring dirtyness"
```

- 출력됨 → **원인 2 확정**: BaseNavmeshDataLayers에 포함됨 → Step 6으로 돌아가서 수정

```
검색어: "Adding dirty area"
```

- 출력됨 → dirty area 생성까지는 성공, 이후 단계 (타일 리빌드)에서 실패
- 출력 안 됨 → dirty area 생성 자체가 실패 (GATE 3, 4 등 다른 필터)

### 14-3. PIE 로그(Step 10)와 비교

**PIE에서는 있었지만 쿠킹 빌드에서는 없는 로그**가 어디서 끊기는지 확인 → 실패 지점 파악.

특히 **bUseWPDynamicMode**와 **bIsFromVisibilityChange** 값이 PIE와 다른지 확인.

---

## Step 15: 원인 3 에디터 확인 `[에디터]`

**소요**: 2분, 소스 수정 불필요

`NavMod_A/B/C` 각각의 Details 패널에서 속성 재확인:

| 속성 | 기대 값 | true이면? |
|------|---------|-----------|
| `Fill Collision Underneath For Navmesh` | **false** | Geometry 플래그 추가됨 |
| `Mask Fill Collision Underneath For Navmesh` | **false** | Geometry 플래그 추가됨 |
| `Nav Mesh Resolution` | **None** | Geometry 플래그 추가됨 |

- 하나라도 비정상 → 수정 후 재쿠킹 → 재테스트
- 모두 정상 → Step 16으로

---

## Step 16: 엔진 소스에 디버그 로그 추가 `[C++]`

**이 단계부터 엔진 소스 수정이 필요하다.** 5개 로그 포인트를 추가한다. 모든 로그에는 `[NavDebug]` 접두사와 프레임 번호가 포함되어 로그만으로 시간 순서와 원인을 파악할 수 있다.

### 16-1. 로그 포인트 A — Nav Modifier 옥트리 등록 및 bLoadedData 설정

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationDataHandler.cpp`
**위치**: 기존 `bLoadedData` 설정 코드 직후 (약 171행)

```cpp
// ★ [NavDebug] 로그 포인트 A — 옥트리 등록 시 bLoadedData 값
{
    const FBox ElementBounds = OctreeElement.Bounds.GetBox();
    UE_LOG(LogNavigation, Warning,
        TEXT("[NavDebug-A][Frame:%llu] OCTREE REGISTER: '%s'\n"
        "    bLoadedData=%d | bIsFromVisibilityChange=%d | IsFromLevelVisibilityChange=%d\n"
        "    HasDynamicModifiers=%d | HasGeometry=%d\n"
        "    Bounds: Min(%.0f, %.0f, %.0f) Max(%.0f, %.0f, %.0f)\n"
        "    DirtyFlag: DynamicModifier=%d Geometry=%d NavigationBounds=%d"),
        GFrameCounter,
        *NavigationElement.GetPathName(),
        OctreeElement.Data->bLoadedData,
        DirtyElement.bIsFromVisibilityChange,
        NavigationElement.IsFromLevelVisibilityChange(),
        OctreeElement.Data->HasDynamicModifiers(),
        OctreeElement.Data->HasGeometry(),
        ElementBounds.Min.X, ElementBounds.Min.Y, ElementBounds.Min.Z,
        ElementBounds.Max.X, ElementBounds.Max.Y, ElementBounds.Max.Z,
        OctreeElement.Data->GetDirtyFlag() & ENavigationDirtyFlag::DynamicModifier ? 1 : 0,
        OctreeElement.Data->GetDirtyFlag() & ENavigationDirtyFlag::Geometry ? 1 : 0,
        OctreeElement.Data->GetDirtyFlag() & ENavigationDirtyFlag::NavigationBounds ? 1 : 0
    );
}
```

### 16-2. 로그 포인트 B — Dirty Area 필터링 (AddAreas 게이트)

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavigationDirtyAreasController.cpp`
**위치**: `AddAreas()` 함수 내부, 각 필터 게이트에 추가 (약 148-180행)

```cpp
// ★ [NavDebug] 로그 포인트 B-1 — WP Dynamic Mode 게이트
if (bUseWorldPartitionedDynamicMode)
{
    if (bIsFromVisibilityChange)
    {
        if (bIsIncludedInBaseNavmesh)
        {
            UE_LOG(LogNavigation, Warning,
                TEXT("[NavDebug-B1][Frame:%llu] DIRTY AREA SKIPPED (BaseNavmesh filter)\n"
                "    Source: '%s'\n"
                "    >>> CAUSE 2: Data layer is in BaseNavmeshDataLayers <<<"),
                GFrameCounter,
                *DirtyArea.GetSourceDescription()
            );
            return; // 기존 return
        }
    }
}
```

```cpp
// ★ [NavDebug] 로그 포인트 B-2 — Dirty Area 정상 추가 시
UE_LOG(LogNavigation, Warning,
    TEXT("[NavDebug-B2][Frame:%llu] DIRTY AREA ADDED\n"
    "    Source: '%s'\n"
    "    Flags: DynamicModifier=%d Geometry=%d NavigationBounds=%d\n"
    "    bIsFromVisibilityChange=%d | bIsInBaseNavmesh=%d | bLoadedData=%d"),
    GFrameCounter,
    *DirtyArea.GetSourceDescription(),
    DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier),
    DirtyArea.HasFlag(ENavigationDirtyFlag::Geometry),
    DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds),
    bIsFromVisibilityChange, bIsIncludedInBaseNavmesh, bLoadedData
);
```

### 16-3. 로그 포인트 C — MarkDirtyTiles 필터링

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp`
**위치**: MarkDirtyTiles 내 DynamicModifier 필터 (약 6588행)

```cpp
// ★ [NavDebug] 로그 포인트 C — DynamicModifier 필터
if (bGameStaticNavMesh && (!DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier)
    || DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds)))
{
    UE_LOG(LogNavigation, Warning,
        TEXT("[NavDebug-C][Frame:%llu] DIRTY TILE SKIPPED (DynamicModifier filter)\n"
        "    Source: '%s'\n"
        "    bGameStaticNavMesh=%d | DynamicModifier=%d | NavigationBounds=%d\n"
        "    >>> %s <<<"),
        GFrameCounter,
        *DirtyArea.GetSourceDescription(),
        bGameStaticNavMesh,
        DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier),
        DirtyArea.HasFlag(ENavigationDirtyFlag::NavigationBounds),
        !DirtyArea.HasFlag(ENavigationDirtyFlag::DynamicModifier)
            ? TEXT("CAUSE 3: DynamicModifier flag missing")
            : TEXT("NavigationBounds flag is set")
    );
    continue; // 기존 continue
}
```

### 16-4. 로그 포인트 D — CompressedTileCacheLayers 상태

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp`
**위치**: `FRecastTileGenerator::Setup()` 내 `GetTileCacheLayers()` 호출 직후 (약 1808행)

```cpp
// ★ [NavDebug] 로그 포인트 D — 타일 캐시 상태
UE_LOG(LogNavigation, Warning,
    TEXT("[NavDebug-D][Frame:%llu] TILE REBUILD Tile(%d,%d)\n"
    "    bGeometryChanged=%d | CompressedLayers=%d | DirtyAreas=%d\n"
    "    %s"),
    GFrameCounter,
    TileX, TileY,
    bGeometryChanged,
    CompressedLayers.Num(),
    DirtyAreas.Num(),
    CompressedLayers.Num() == 0 && !bGeometryChanged
        ? TEXT(">>> CAUSE 1: CompressedLayers EMPTY during modifier-only rebuild! <<<")
        : TEXT("OK: CompressedLayers available")
);
```

### 16-5. 로그 포인트 E — OnStreamingNavDataAdded 재처리 필터

**파일**: `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMesh.cpp`
**위치**: `OnStreamingNavDataAdded()` 내 `FindElementsInNavOctree()` 호출 전후 (약 3208-3220행)

```cpp
// ★ [NavDebug] 로그 포인트 E — NavDataChunk 로드 후 재처리
UE_LOG(LogNavigation, Warning,
    TEXT("[NavDebug-E][Frame:%llu] REPROCESS SEARCH RESULT: Found %d elements\n"
    "    %s"),
    GFrameCounter,
    NavElements.Num(),
    NavElements.Num() == 0
        ? TEXT(">>> CAUSE 5: No elements found for reprocessing! <<<")
        : TEXT("OK: Elements found, will create dirty areas for reprocessing")
);
```

### 16-6. 로그 검색 가이드

| 검색어 | 출력 시 의미 |
|--------|-------------|
| `CAUSE 1` | CompressedTileCacheLayers가 비어있는 상태에서 모디파이어 리빌드 시도 |
| `CAUSE 2` | BaseNavmeshDataLayers 필터에 의해 dirty area 무시됨 |
| `CAUSE 3` | DynamicModifier 플래그 누락으로 MarkDirtyTiles에서 스킵 |
| `CAUSE 5` | bExcludeLoadedData 필터에 의해 재처리 대상에서 제외됨 |

---

## Step 17: 엔진 빌드 및 재쿠킹 `[C++]` `[에디터]`

1. 엔진 소스 빌드 (NavigationSystem 모듈만 재빌드 가능)
2. 에디터 재시작
3. Development 빌드로 재쿠킹

---

## Step 18: 쿠킹 빌드 로그 분석 (엔진 로그 포인트) `[확인]`

Step 13과 동일하게 실행 후, 로그를 분석한다.

### 18-1. 빠른 판단: CAUSE 키워드 검색

```
검색어: "CAUSE"
```

| 출력된 키워드 | 원인 | 즉시 조치 |
|--------------|------|-----------|
| `CAUSE 2` | BaseNavmeshDataLayers 필터 | Step 6에서 배열 수정 |
| `CAUSE 3` | DynamicModifier 플래그 누락 | Step 15에서 속성 수정 |
| `CAUSE 1` | CompressedLayers 비어있음 (타이밍) | 원인 5와 함께 확인 |
| `CAUSE 5` | bExcludeLoadedData 필터 (재처리 누락) | 원인 1과 함께 확인 |
| `CAUSE 1` + `CAUSE 5` 둘 다 | **가장 유력: 타이밍 + 재처리 누락 결합** | 해결 방안 A 또는 B 적용 |

### 18-2. 상세 분석: 프레임 번호로 타이밍 추적

```
검색: "[NavDebug-A]" → Nav Modifier 등록 프레임
검색: "[NavDebug-E]" → NavDataChunkActor 로드 프레임
```

| Nav Modifier 등록 (A) | NavDataChunk 로드 (E) | 의미 |
|----------------------|---------------------|------|
| Frame 100 | Frame 200 | Nav Modifier가 먼저 → 타일 캐시 없이 리빌드 시도 (원인 1 의심) |
| Frame 200 | Frame 100 | NavDataChunk가 먼저 → 타일 캐시 존재 상태에서 리빌드 (정상 동작해야 함) |
| 같은 프레임 | 같은 프레임 | 같은 레벨 로딩 → BeginPlay 순서에 따라 결정 |

### 18-3. 자동 검증 로그 (NavTest SUMMARY) 확인

```
검색: "[NavTest]" "[SUMMARY]"
```

### 18-4. 전체 로그 분석 흐름도

```
1. "CAUSE" 검색
   ├─ 있으면 → 원인 확정, 해당 원인 조치로 이동
   └─ 없으면 → 2번으로

2. "[NavTest][SUMMARY]" 검색
   ├─ "ALL TESTS PASSED" → 버그 미재현, 테스트 환경 차이 확인
   ├─ "DATA LAYER MODIFIERS FAILED" → 버그 재현됨, 3번으로
   └─ "BOTH CONTROL AND DATA LAYER FAILED" → 네비메시 자체 문제, 맵 설정 확인

3. "[NavDebug-A]" Frame 번호 수집 (Nav Modifier 등록 시점)
4. "[NavDebug-E]" Frame 번호 수집 (NavDataChunk 로드 시점)
5. "[NavDebug-D]" 에서 CompressedLayers 값 확인
6. "[NavDebug-B2]" 에서 dirty area 추가 확인
7. 프레임 순서 + CompressedLayers 상태로 원인 특정
```

---

## Step 19: 변형 시나리오 — 딜레이 테스트 `[확인]`

원인이 타이밍인지 아닌지를 명확히 구분하기 위한 핵심 테스트.
**재쿠킹 없이** 실행 인자로 딜레이를 오버라이드할 수 있다.

### 19-1. 딜레이별 실행

```bash
# 즉시 활성화 — 스트리밍과 최대 경합
UE5TestProject.exe L_NavModifierTest -NavTestDelay=0 -log -LogCmds="..."

# 충분한 대기 — 모든 셀 로드 완료 후 활성화
UE5TestProject.exe L_NavModifierTest -NavTestDelay=30 -log -LogCmds="..."

# 완전 안정 — 이래도 안 되면 타이밍 외 원인
UE5TestProject.exe L_NavModifierTest -NavTestDelay=60 -log -LogCmds="..."
```

### 19-2. 결과 판단

| 딜레이 0s | 딜레이 30s | 딜레이 60s | 결론 |
|-----------|-----------|-----------|------|
| 미동작 | **동작** | **동작** | **원인 1 (타이밍)**: NavDataChunk 로드 후 활성화하면 정상 |
| 미동작 | 미동작 | **동작** | 타이밍이지만 스트리밍이 매우 느린 환경 |
| 미동작 | 미동작 | 미동작 | **타이밍과 무관** → 원인 5 (bExcludeLoadedData 필터) 또는 다른 원인 |

---

## Step 20: 추가 변형 시나리오 `[에디터]` `[확인]`

원인 범위를 좁히기 위한 추가 테스트. 필요 시 선택적으로 수행.

### 20-1. 데이터 레이어 초기 활성화 `[에디터]`

`DL_NavModifier`의 `Initial Runtime State`를 **Activated**로 변경 → 재쿠킹 → 테스트.
- 게임 시작 시 즉시 로드 → NavigationDataChunkActor와의 경합 극대화
- 원인 1이라면 여기서도 실패할 가능성 높음

### 20-2. 같은 위치에 데이터 레이어 없는 Volume 추가 `[에디터]`

`NavMod_A`와 동일한 위치에 **데이터 레이어가 없는** Nav Modifier 하나 더 배치.
- 쿠킹 빌드에서 이 Volume만 동작하면 → 데이터 레이어 로딩 메커니즘 특화 문제 확증

### 20-3. NavModifierComponent로 교체 `[에디터]`

1. 빈 Actor 배치 → `UNavModifierComponent` 추가
2. `DL_NavModifier` 데이터 레이어 할당
3. Volume과 동일한 결과인지 확인

---

## Step 21: 원인 4 검증 — 언로드/재로드 `[C++]` `[Console]` `[확인]`

**Step 18에서 원인 1/5가 아닌 것으로 판명된 경우에만 실행.**

### 21-1. 시나리오 재현 `[Console]`

1. 쿠킹 빌드 실행
2. 콘솔: `wp.Runtime.ToggleDataLayerActivation DL_NavModifier` (활성화)
3. 10초 대기
4. 콘솔: `wp.Runtime.ToggleDataLayerActivation DL_NavModifier` (비활성화)
5. 10초 대기
6. 콘솔: `wp.Runtime.ToggleDataLayerActivation DL_NavModifier` (재활성화)
7. 네비메시 확인

---

## Step 22: 결과 기록 `[확인]`

### 결과 기록 템플릿

```markdown
## 테스트 결과

### 환경
- 엔진 버전: UE 5.7
- 빌드 타입: Development
- 플랫폼: Windows

### Step 8 (PIE) — NavTest SUMMARY 로그 복사

(NavTest SUMMARY 로그 전체를 여기에 붙여넣기)


### Step 13 (쿠킹 빌드) — NavTest SUMMARY 로그 복사

(NavTest SUMMARY 로그 전체를 여기에 붙여넣기)


### Step 14 (기본 로그 검색 결과)
- "CAUSE" 검색 결과: [없음 / CAUSE 1 / CAUSE 2 / CAUSE 3 / CAUSE 5]
- "[registered]" 출력: [예/아니오]
- "DIRTY AREA SKIPPED" 출력: [예/아니오]
- "DIRTY AREA ADDED" 출력: [예/아니오]

### Step 18 (엔진 로그 — 프레임 번호)
- Nav Modifier 등록 프레임 ([NavDebug-A]): Frame [번호]
- NavDataChunk 로드 프레임 ([NavDebug-E]): Frame [번호]
- 로딩 순서: [Nav Modifier 먼저 / NavDataChunk 먼저 / 같은 프레임]

### Step 19 (딜레이 테스트 — NavTest SUMMARY 기준)
- -NavTestDelay=0  → PASS: [n] / FAIL: [n]
- -NavTestDelay=30 → PASS: [n] / FAIL: [n]
- -NavTestDelay=60 → PASS: [n] / FAIL: [n]

### 확정 원인
[CAUSE 번호 및 설명]

### 적용할 수정 방안
[방안 선택 및 이유]
```

---

## 전체 흐름 요약

```
[에디터] Step 1~7: 테스트 맵 셋업
  ├─ Step 1:  맵 생성 (Empty Open World → World Partition)
  ├─ Step 2:  데이터 레이어 에셋 + 인스턴스 생성
  ├─ Step 3:  바닥 (랜드스케이프 또는 Cube) + NavMeshBoundsVolume 배치
  ├─ Step 4:  RecastNavMesh → DynamicModifiersOnly 설정
  ├─ Step 5:  Nav Modifier Volume 4개 배치 (3개 테스트 + 1개 대조군)
  ├─ Step 6:  BaseNavmeshDataLayers 확인 (원인 2 사전 배제)
  └─ Step 7:  DL_NavModifier 에디터 가시성 끄기 + 네비메시 빌드 확인

[에디터+Console] Step 8: PIE 검증
  └─ 콘솔 명령으로 데이터 레이어 토글 → 동작 확인

[Config] Step 9: 로그 설정 (DefaultEngine.ini)

[에디터] Step 10: PIE 로그 베이스라인 수집

[C++] Step 11: NavTestGameMode C++ 코드 작성 (구현 완료)
[BP]  Step 12: BP_NavTestGameMode 생성 → 에셋/좌표 할당
  └─ [에디터] World Settings → GameMode Override 설정

[에디터+확인] Step 13: 쿠킹 빌드 → 실행 → 버그 재현
[확인] Step 14: 쿠킹 빌드 기본 로그 분석
[에디터] Step 15: 원인 3 속성 확인 (에디터)

[C++] Step 16: 엔진 소스에 디버그 로그 5개 추가
[C++] Step 17: 엔진 빌드 + 재쿠킹
[확인] Step 18: 엔진 로그 분석 → 원인 판단

[확인] Step 19: 딜레이 변형 테스트 (실행 인자로 재쿠킹 불필요)
[에디터+확인] Step 20: 추가 변형 시나리오 (선택)
[Console+확인] Step 21: 원인 4 검증 (조건부)

[확인] Step 22: 결과 기록
```
