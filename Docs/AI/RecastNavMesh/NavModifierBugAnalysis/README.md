# DynamicModifiersOnly 쿠킹 빌드 NavModifier 미동작 버그 분석

> **작성일**: 2026-02-15
> **최종 수정**: 2026-02-18
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#9](https://github.com/jiy12345/UE5TestProject/issues/9)

## 개요

World Partition Data Layer 안의 `NavModifierVolume`이 `DynamicModifiersOnly` 모드에서 **PIE에서는 정상 동작하지만 쿠킹 빌드에서는 NavMesh에 전혀 영향을 주지 않는 버그** 분석 문서입니다.

## 선수 지식

- [RecastNavMesh 시스템 분석](../README.md) — 상위 문서
- World Partition / Data Layer 개념

## 목차

| 파일 | 내용 |
|------|------|
| [01-bug-description.md](01-bug-description.md) | 버그 현상 및 재현 조건 |
| [02-navmesh-system.md](02-navmesh-system.md) | NavMesh 시스템 구조 이해 |
| [03-log-analysis.md](03-log-analysis.md) | 로그 분석 및 재현 |
| [04-root-cause.md](04-root-cause.md) | 원인 분석 (AttachTiles 오버라이트) |
| [05-reproduction-and-fix.md](05-reproduction-and-fix.md) | 재현 조건 및 수정 방향 |
| [06-source-analysis-detail.md](06-source-analysis-detail.md) | 소스 코드 심층 분석 (5가지 원인 상세) |
| [07-pie-vs-cooked-loading.md](07-pie-vs-cooked-loading.md) | PIE vs 쿠킹 빌드 액터 로딩 차이 |
| [08-test-plan.md](08-test-plan.md) | 상세 테스트 플랜 |
| [09-reproduction-scenarios.md](09-reproduction-scenarios.md) | 재현 시나리오 분석 |

## 결론 요약

가장 유력한 원인은 **원인 1+5 결합**:

1. **원인 1**: 데이터 레이어 Nav Modifier dirty area 처리 시 `NavDataChunkActor`가 미로드 → `CompressedTileCacheLayers` 비어있음 → 빈 타일 생성
2. **원인 5**: 이후 `NavDataChunkActor` 로드 시 `bExcludeLoadedData=true` 필터로 Nav Modifier가 재처리 대상에서 제외됨

**PIE에서 동작하는 이유**: 에디터 NavMesh 완전 빌드 후 플레이 → `CompressedTileCacheLayers`가 항상 존재

## 핵심 소스 파일

| 파일 | 역할 |
|------|------|
| `NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` | TileGen 로직, MarkDirtyTiles |
| `NavigationSystem/Private/NavMesh/RecastNavMesh.cpp` | OnStreamingNavDataAdded, AttachNavMeshDataChunk |
| `NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp` | AttachTiles (핵심 버그 위치) |
| `NavigationSystem/Private/NavigationDirtyAreasController.cpp` | Dirty Area 필터링 |
| `NavigationSystem/Private/NavigationDataHandler.cpp` | bLoadedData 설정 |
