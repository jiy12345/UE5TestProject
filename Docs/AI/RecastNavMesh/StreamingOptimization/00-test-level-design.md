# 이슈 #15 테스트 레벨 설계

> **작성일**: 2026-04-15
> **관련 이슈**: #15 (WP world + Non-WP navmesh NavModifier 스트리밍 dirty 오버헤드)
> **목적**: 가설 검증 → 완화책 비교 → 엔진 패치 효과 측정의 전 단계에서 사용할 재현 가능 테스트 레벨 설계

## 1. 테스트 목표

Phase별로 다른 측정이 필요:

| Phase | 측정 대상 |
|-------|-----------|
| **Phase 1: 진단** | 현재 프로젝트 설정에서 실제로 스트리밍 dirty가 얼마나 비용 차지하는지 수치화 |
| **Phase 2: 완화책 비교** | 각 완화책 적용 전후 수치 비교 |
| **Phase 3: 엔진 패치 검증** | 패치 전/후 측정 + 리그레션 확인 |

동일 레벨을 모든 Phase에서 사용해 **수치 비교 가능성 보장**이 핵심.

## 2. 레벨 구조 요구사항

### 2-1. 월드 설정

- **World Partition 레벨** — 사용자 프로젝트와 동일
- **Non-WP NavMesh** (`RecastNavMesh-Default` 단일 글로벌)
- **Cell 크기**: 실제 게임과 유사 (예: 실제 게임이 `CellSize=25600` 쓰면 동일)
- **Streaming Distance**: 실제 게임과 유사

### 2-2. 공간 구성

```
[테스트 월드 레이아웃]

     ┌────────┬────────┬────────┬────────┬────────┐
     │ Cell A │ Cell B │ Cell C │ Cell D │ Cell E │  ← 일자 경로 테스트용
     └────────┴────────┴────────┴────────┴────────┘
           │                                    │
   ┌───────┴──────┐                    ┌───────┴──────┐
   │ Dense Cell   │                    │ Sparse Cell  │  ← 밀도 대비
   │ (M 100개)    │                    │ (M 5개)      │
   └──────────────┘                    └──────────────┘
   
   ┌──────────────┐
   │ Control Cell │  ← 비교 베이스라인 (Modifier 0개)
   └──────────────┘
```

### 2-3. Cell 유형

| Cell 유형 | Modifier 배치 | 목적 |
|-----------|--------------|------|
| **Baseline Cell** | 0개 | 스트리밍 자체의 비용 기준선 |
| **Sparse Cell** | 5개 | 낮은 밀도 비교 |
| **Normal Cell** | 25~50개 | 일반 게임 수준 |
| **Dense Cell** | 100~200개 | 극단 상황 측정 |
| **DataLayer Cell** | Runtime DataLayer 내 M 다수 | DataLayer 경로 비교 |

## 3. Modifier 배치 시나리오

각 시나리오는 별도 Cell 또는 별도 테스트 케이스로 분리:

### 시나리오 A: 표준 정적 Modifier (주 타겟)

```
- DataLayer 없음
- 에디터 정적 배치
- BeginPlay 속성 변경 없음
- bake에 포함됨
```

**예상 동작**:
- 현재: 스트리밍마다 register dirty → 재빌드 (중복)
- 최적화 적용: skip → 비용 감소

**배치 방법**:
- `ANavModifierVolume` 액터 N개를 Cell 내 균일 분포
- 크기: 200×200×100 정도 (일반적 크기)
- AreaClass 다양화 (50% Default, 30% Obstacle, 20% Custom)

### 시나리오 B: Editor DataLayer 소속 Modifier

```
- Editor DataLayer에 배치 (bake 시 UP)
- 런타임에 DataLayer 토글 안 됨
- 시나리오 A와 유사한 동작 예상
```

**배치 방법**:
- 별도 Editor DataLayer 생성 (예: `EditorLayer_NavModifiers`)
- 해당 DataLayer에 Modifier 다수 할당

### 시나리오 C: Runtime DataLayer 소속 Modifier (제외 대상)

```
- Runtime DataLayer에 배치 (bake 시 DOWN)
- 런타임에 DataLayer 활성화 시 등장
- 최적화에서 명시 제외되어야 함
```

**배치 방법**:
- Runtime DataLayer 생성 (예: `RuntimeLayer_DynamicModifiers`)
- 해당 DataLayer에 Modifier 다수 할당
- BP 또는 DataLayer Subsystem으로 토글 테스트

### 시나리오 D: Runtime-Spawned Modifier (카테고리 B)

```
- 에디터 배치 없음
- BP/C++ 로직으로 SpawnActor
- 카테고리 B라 최적화와 무관
```

**배치 방법**:
- 스포너 액터를 Cell에 배치
- 플레이어 근접 시 Modifier N개 동시 스폰
- 주기적 Destroy + ReSpawn 패턴

### 시나리오 E: 동적 속성 Modifier (주의 케이스)

```
- 정적 배치 (bake 포함)
- BeginPlay에서 AreaClass/Bounds 변경
- 카테고리 B 경로 (SetAreaClass 등)가 정상 작동해야 함
```

**배치 방법**:
- 커스텀 Blueprint 클래스 `BP_DynamicNavModifier`
- BeginPlay:
  ```
  Wait(Random(0.5, 2.0))
  SetAreaClass(RandomChoice(Default, Obstacle, Null))
  ```
- 의도적으로 기본값과 다른 값 설정

### 시나리오 F: 동적 레벨 생성 (엣지 케이스, 프로젝트에 있다면)

```
- 런타임에 레벨 에셋 로드
- bake에 없던 콘텐츠
- 최적화 skip하면 안 됨
```

**배치 방법**:
- 테스트 버튼/콘솔 커맨드로 `LoadMapAsync` 호출
- 해당 레벨에 Modifier 포함

## 4. 플레이어 이동 시나리오

각 시나리오는 자동화 가능한 스크립트로 구현:

### 시나리오 Ⅰ: 정지 (Baseline)

- 플레이어 고정
- 배경 기준선 CPU 비용 측정

### 시나리오 Ⅱ: 느린 이동 (걷기)

- 일자 경로 (Cell A → B → C → D → E)
- 속도: 600 UU/s (일반 캐릭터 속도)
- 자연스러운 점진적 스트리밍 패턴

### 시나리오 Ⅲ: 빠른 이동 (차량)

- 동일 경로
- 속도: 3000~5000 UU/s
- 빈도 높은 스트리밍 이벤트

### 시나리오 Ⅳ: 순간이동 스팸

- `SetActorLocation`으로 Cell 간 반복 점프
- 간격: 1초
- 대량 Cell 로드/언로드 급발생

### 시나리오 Ⅴ: 밀도 대비

- Baseline Cell 통과 → 수치 측정
- Sparse Cell 통과 → 수치 측정
- Normal Cell 통과 → 수치 측정
- Dense Cell 통과 → 수치 측정
- 선형 증가 여부 확인

### 시나리오 Ⅵ: DataLayer 토글

- 플레이어는 정지
- BP/콘솔에서 Runtime DataLayer 활성/비활성 반복
- Runtime DataLayer 처리 vs 스트리밍 처리 분리 측정

## 5. 측정 포인트

### 5-1. 엔진 stat 기반

```
stat Navigation
stat NavigationSystem
stat NavRegenTime
stat NavRegenPending
```

수집 수치:
- `NavMesh Rebuild Time (ms)`
- `Dirty Area Count`
- `Pending Tile Count`
- `Running Tile Task Count`

### 5-2. 커스텀 로그

```ini
[Core.Log]
LogNavigation=VeryVerbose
LogNavigationDirtyArea=VeryVerbose
LogNavOctree=Verbose
```

로그에서 파싱할 지표:
- 초당 `MarkDirtyTiles` 호출 횟수
- 초당 `AddGeneratedTilesAndGetUpdatedTiles` 호출 횟수
- 각 호출의 지속 시간
- dirty flag 종류별 분포 (Geometry/DynamicModifier/NavigationBounds)

### 5-3. 프레임 시간 프로파일링

- `Unreal Insights` 활용
- 주요 이벤트:
  - `FRecastNavMeshGenerator::TickAsyncBuild`
  - `FRecastNavMeshGenerator::MarkDirtyTiles`
  - `FRecastNavMeshGenerator::AddGeneratedTilesAndGetUpdatedTiles`
  - `FRecastTileGenerator::DoWork` (비동기 태스크)

### 5-4. 커스텀 디버그 컴포넌트

```cpp
// UNavStreamingProfilerComponent
// 이슈 #14의 UNavDebugVisualizerComponent와 유사 설계

void UNavStreamingProfilerComponent::TickComponent(...)
{
    // 매 틱 NavigationSystem 상태 캡처
    PendingDirtyCount = Generator->GetNumRemaningBuildTasks();
    RunningTasksCount = Generator->GetNumRunningBuildTasks();
    AccumDirtyEventsThisSecond++;
    
    // OnScreen 출력
    GEngine->AddOnScreenDebugMessage(...);
    
    // CSV 저장 옵션
    if (bLogToCSV) AppendToCSVFile();
}
```

**CSV 출력 필드**:
```
Timestamp, FrameTime_ms, DirtyCount, PendingTiles, RunningTasks,
TickAsyncBuild_ms, MarkDirtyTiles_ms, AddGeneratedTiles_ms,
PlayerSpeed, ActiveCells, LoadedModifiers
```

## 6. 측정 프로토콜

### 6-1. 준비

1. 테스트 레벨 로드
2. PIE 또는 Standalone 실행
3. 커맨드 `stat startfile` 또는 Insights trace 시작
4. 커스텀 프로파일러 컴포넌트 활성화 (CSV 기록 시작)

### 6-2. 각 시나리오 실행

1. 1분 baseline (정지 상태)
2. 시나리오 실행 (자동 스크립트)
3. 1분 post-baseline
4. `stat stopfile` 또는 Insights 저장

### 6-3. 수집 아티팩트

- Insights trace 파일 (`.utrace`)
- CSV 로그
- 콘솔 출력 (LogNavigation)
- 스크린샷 (시각적 검증)

### 6-4. 재현 가능성 보장

- 시나리오 자동화 (BP 또는 Gauntlet 테스트)
- 랜덤 시드 고정
- 하드웨어/설정 기록
- Git 태그로 빌드 고정

## 7. 비교 베이스라인 집합

각 측정은 다음 조건 매트릭스로 수행:

| 조건 | 변형 |
|------|------|
| **엔진 버전** | 현재 / 패치 적용 |
| **완화책** | 없음 / MaxSimultaneousJobsCount 낮춤 / Invoker 부분 / 엔진 skip 패치 |
| **시나리오** | Ⅰ~Ⅵ |
| **Cell 밀도** | Baseline / Sparse / Normal / Dense |

조합 = 최대 4×4×6×4 = 384개지만, 핵심 조합 20~30개로 압축 가능:

```
[핵심 측정 매트릭스 예시]

1. [현재 엔진, 완화책 없음, Ⅱ, Normal Cell] — 기준선
2. [현재 엔진, 완화책 없음, Ⅲ, Normal Cell] — 속도 영향
3. [현재 엔진, 완화책 없음, Ⅱ, Dense Cell] — 밀도 영향
4. [현재 엔진, Jobs 낮춤, Ⅱ, Normal Cell] — 완화책 1
5. [현재 엔진, Invoker 부분, Ⅱ, Normal Cell] — 완화책 2
6. [패치 적용, 완화책 없음, Ⅱ, Normal Cell] — 엔진 패치 효과
... (중략)
```

## 8. 테스트 레벨 저장 위치 및 명명

### 파일 구조

```
Game/Content/Tests/NavStreamingOptimization/
├── Maps/
│   ├── L_NavStreamTest_Main.umap        (Persistent Level)
│   ├── L_NavStreamTest_SubA.umap        (Cell A)
│   ├── L_NavStreamTest_SubB.umap        (...)
│   ├── L_NavStreamTest_Dense.umap       (Dense Cell)
│   ├── L_NavStreamTest_Sparse.umap      (Sparse Cell)
│   ├── L_NavStreamTest_Baseline.umap    (Control)
│   └── L_NavStreamTest_DataLayer.umap   (Runtime DataLayer test)
│
├── Blueprints/
│   ├── BP_NavStreamTestGameMode.uasset
│   ├── BP_NavStreamTestPlayerController.uasset
│   ├── BP_DynamicNavModifier.uasset     (Scenario E)
│   ├── BP_ModifierSpawner.uasset         (Scenario D)
│   └── BP_TestAutomation.uasset          (Scenarios Ⅰ-Ⅵ)
│
├── DataLayers/
│   ├── DL_Runtime_Modifiers.uasset
│   └── DL_Editor_Modifiers.uasset
│
├── NavMesh/
│   └── (자동 생성된 Non-WP 네비메시 데이터)
│
└── Automation/
    ├── TestScenario_Ⅰ_Stationary.uasset
    ├── TestScenario_Ⅱ_Walking.uasset
    ├── TestScenario_Ⅲ_Vehicle.uasset
    ├── TestScenario_Ⅳ_Teleport.uasset
    ├── TestScenario_Ⅴ_DensityRamp.uasset
    └── TestScenario_Ⅵ_DataLayerToggle.uasset
```

### 관련 C++ 클래스

```
Game/Source/UE5TestProject/Tests/NavStreamingOptimization/
├── NavStreamingProfilerComponent.h/.cpp
├── NavStreamTestHelpers.h/.cpp
└── (이슈 #14의 AGridNavData 파일과는 별도)
```

## 9. 자동화 수준

### 수동 단계
- 레벨 구성 (Modifier 배치, DataLayer 할당)
- 네비메시 bake

### 반자동 단계
- BP `BP_TestAutomation`에서 시나리오 선택 버튼으로 실행
- 결과 CSV 자동 저장

### 완전 자동 (선택)
- Gauntlet 기반 테스트 러너
- CI에서 정기 실행하여 성능 회귀 감지

**Phase 1**에서는 반자동 수준이면 충분. 자동화는 Phase 3 이후 고려.

## 10. 확인 항목 (Level 제작 완료 후)

- [ ] 각 Cell이 예상대로 스트리밍됨 (PIE 플레이 중 Cell 진입/이탈 로그 확인)
- [ ] Modifier 개수가 각 Cell별로 의도대로 배치됨
- [ ] Runtime DataLayer 토글 시 Modifier 표시 여부 정상
- [ ] 네비메시 bake 시점에 모든 Zone UP + Runtime DataLayer DOWN 상태
- [ ] 시나리오 자동화 스크립트가 의도대로 실행됨
- [ ] 프로파일러 컴포넌트가 정상 CSV 기록
- [ ] baseline 측정치가 반복 실행 시 ±10% 이내 안정

## 11. 다음 단계

1. 이 문서 이슈 #15에 링크
2. 레벨 제작 착수 (별도 PR 또는 브랜치)
3. 레벨 완성 후 Phase 1 측정 진행
4. 측정 결과를 `Docs/AI/RecastNavMesh/StreamingOptimization/01-phase1-profiling.md` 등으로 정리
5. Phase 2 완화책 측정 결과를 `02-phase2-mitigations.md`로
6. Phase 3 엔진 패치 결과를 `03-phase3-engine-patch.md`로
7. 최종 결론을 `README.md` 또는 `04-conclusion.md`로

## 참고 자료

- Unreal Insights 공식 가이드 (Epic Games)
- UE Gauntlet Automation Framework 가이드
- 이슈 #9 (관련 버그 분석 — 유사한 프로파일링 접근)
- `Docs/AI/RecastNavMesh/02-architecture.md` §5-3 (설계 가정과 한계)
- `Docs/AI/RecastNavMesh/04-source-analysis.md` §4-2 (DataLayer 시나리오)
