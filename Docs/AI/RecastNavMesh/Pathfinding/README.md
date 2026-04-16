# 길찾기(Pathfinding) 로직 End-to-End 분석

> **작성일**: 2026-04-16
> **엔진 버전**: UE 5.5
> **관련 이슈**: [#16](https://github.com/jiy12345/UE5TestProject/issues/16)

## 개요

이 문서는 언리얼 엔진의 길찾기 로직을 **End-to-End**로 분석합니다.
AI가 `MoveToLocation()` 명령을 받으면 내부적으로 어떤 코드가 실행되어 실제 이동이 이루어지는지,
Detour 라이브러리의 A* 알고리즘부터 UE의 경로 추적/무효화 메커니즘까지 전체 흐름을 다룹니다.

## 선수 지식

- [01-overview.md](../01-overview.md) — Recast/Detour 개요 및 UE 통합 구조
- [02-architecture.md](../02-architecture.md) — 클래스 계층 구조
- [03-core-classes.md](../03-core-classes.md) — dtNavMesh, dtNavMeshQuery 등 핵심 클래스

## 목차

| 파일 | 내용 |
|------|------|
| [01-pathfinding-overview.md](01-pathfinding-overview.md) | 전체 파이프라인 조감도 |
| [02-detour-a-star.md](02-detour-a-star.md) | Detour A* 알고리즘 심층 분석 |
| [03-funnel-algorithm.md](03-funnel-algorithm.md) | 퍼널 알고리즘 (findStraightPath / String Pulling) |
| [04-ue-pathfinding-pipeline.md](04-ue-pathfinding-pipeline.md) | UE 래퍼 계층 (NavigationSystem → FPImplRecastNavMesh) |
| [05-path-following.md](05-path-following.md) | 경로 추적 (UPathFollowingComponent) |
| [06-path-invalidation.md](06-path-invalidation.md) | 경로 무효화 및 재탐색 메커니즘 |
| [07-practical-guide.md](07-practical-guide.md) | 이해도 확인 실습 |

## 주요 소스 경로

```
Engine/Source/Runtime/Navmesh/Private/Detour/     # Detour 라이브러리 (A*, Funnel)
Engine/Source/Runtime/NavigationSystem/            # UE NavSystem 래퍼
Engine/Source/Runtime/AIModule/                    # AIController, PathFollowingComponent
```
