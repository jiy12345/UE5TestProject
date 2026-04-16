# RecastNavMesh 시스템 분석

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#10](https://github.com/jiy12345/UE5TestProject/issues/10)

## 개요

언리얼 엔진의 NavMesh는 오픈소스 라이브러리 **Recast/Detour**를 기반으로 구축됩니다.
- **Recast**: 월드 지오메트리로부터 NavMesh를 생성하는 빌드 라이브러리
- **Detour**: 생성된 NavMesh에서 경로 찾기(Pathfinding) 및 쿼리를 수행하는 런타임 라이브러리

UE는 이 두 라이브러리를 `ARecastNavMesh`, `FRecastNavMeshGenerator` 등의 클래스로 래핑하여 엔진과 통합합니다.

## 학습 목표

- Recast/Detour 라이브러리의 역할과 UE 통합 구조 이해
- NavMesh 빌드 파이프라인의 전체 흐름 파악
- 타일(Tile) 기반 구조와 런타임 생성 모드 이해
- Nav Area, Nav Modifier, Nav Link 처리 방식 이해
- World Partition 환경에서의 NavMesh 스트리밍 구조 이해

## 목차

| 파일 | 내용 |
|------|------|
| [01-overview.md](01-overview.md) | 개념 소개 및 전체 그림 |
| [02-architecture.md](02-architecture.md) | 아키텍처 및 클래스 계층 구조 |
| [03-core-classes.md](03-core-classes.md) | 핵심 클래스 및 구조체 상세 |
| [04-source-analysis.md](04-source-analysis.md) | 빌드 파이프라인 소스 코드 심층 분석 |
| [05-practical-guide.md](05-practical-guide.md) | 실전 활용 및 커스터마이징 가이드 |
| [references.md](references.md) | 참고 자료 및 소스 파일 경로 |

## 심층 분석

| 폴더 | 내용 |
|------|------|
| [Pathfinding/](Pathfinding/) | 길찾기 로직 End-to-End 분석 (Detour A* → UE PathFollowing) |
| [NavModifierBugAnalysis/](NavModifierBugAnalysis/) | NavModifier 버그 분석 |
| [StreamingOptimization/](StreamingOptimization/) | 스트리밍 최적화 분석 |

## 선수 지식

- UObject 시스템 (AActor, UActorComponent)
- 언리얼 엔진 World / Level 구조
- World Partition / Data Layer (선택)

## 주요 소스 경로

```
Engine/Engine/Source/Runtime/NavigationSystem/   # UE 래퍼 (ARecastNavMesh 등)
Engine/Engine/Source/Runtime/Navmesh/            # Recast/Detour 라이브러리 (ThirdParty 수준)
```
