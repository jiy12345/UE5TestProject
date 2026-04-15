# 1. 버그 설명

> **작성일**: 2026-02-18 | **엔진 버전**: UE 5.7

## 현상

### 재현 조건

- **NavMesh 생성 타입**: `DynamicModifiersOnly` (`ERuntimeGenerationType::DynamicModifiersOnly`)
- **NavModifierVolume 배치**: World Partition **Data Layer** 안
- **대조군**: AlwaysLoaded 레이어에 배치된 NavModifierVolume (정상 동작)

### 증상

| 상황 | 결과 |
|------|------|
| PIE (에디터 플레이) | NavModifier 정상 적용 (경로 우회) |
| 쿠킹 빌드 1회차 실행 | **간헐적으로** PASS |
| 쿠킹 빌드 2회차 이후 | NavModifier 미적용 (직선 경로) |

- 원본 프로젝트에서는 **아예 네비에 영향이 없었음** (스트리밍 자체는 발생함)
- 테스트 프로젝트에서는 **간헐적으로 PASS** — 이 우연성이 중요한 단서

## 테스트 구성 (NavTestGameMode)

```
GameMode 시작
    │
    ├─ [PRE-CHECK] 데이터 레이어 활성화 전 PathPoints 측정
    │
    ├─ SetDataLayerRuntimeState(Activated)  ← DataLayerActivateDelay초 후
    │
    ├─ [VERIFY] VerifyDelay초 후 PathPoints 재측정
    │       PathPoints==2  → 직선(modifier 미적용) → FAIL
    │       PathPoints>2   → 우회(modifier 적용)   → PASS
    │
    └─ [MONITOR] MonitorDuration초 동안 매 프레임 PathPoints 변화 기록
```

### 실행 인자 (DefaultEngine.ini GameDefaultMap 기준)
```
-NavTestDelay=5 -NavTestVerifyDelay=5 -NavTestMonitorDuration=15
```

## 핵심 관찰

1. **스트리밍 자체는 성공**: DL 활성화 시 NavModifierVolume이 로드되고 nav octree에 등록됨
2. **TileGen에서 Modifiers=1**: `AfterGatherGeometry Modifiers=1` 로그로 확인 (modifier는 인식됨)
3. **그럼에도 결과적으로 경로가 직선**: TileGen 결과물에 modifier가 반영되지 않거나, 반영된 타일이 나중에 덮어써짐
