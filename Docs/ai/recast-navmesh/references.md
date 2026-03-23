# References

> **작성일**: 2026-03-23
> **엔진 버전**: UE 5.7

## 엔진 소스 파일

| 파일 | 경로 | 주요 내용 |
|------|------|----------|
| `RecastNavMesh.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMesh.h` | ARecastNavMesh 클래스 (라인 572) |
| `RecastNavMeshGenerator.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavMesh/RecastNavMeshGenerator.h` | FRecastNavMeshGenerator (라인 784), FRecastBuildConfig (라인 55) |
| `RecastNavMeshGenerator.cpp` | `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshGenerator.cpp` | 빌드 파이프라인, IsGameStaticNavMesh (라인 5105), MarkDirtyTiles (라인 ~6560) |
| `NavigationData.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavigationData.h` | ANavigationData (라인 545), ERuntimeGenerationType (라인 528) |
| `NavigationSystem.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavigationSystem.h` | UNavigationSystemV1 |
| `NavigationTypes.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavigationTypes.h` | ENavigationDirtyFlag, FNavigationRelevantData |
| `NavigationTypes.cpp` | `Engine/Source/Runtime/NavigationSystem/Private/NavigationTypes.cpp` | IsInBaseNavmesh() (라인 50) |
| `NavigationDirtyAreasController.cpp` | `Engine/Source/Runtime/NavigationSystem/Private/NavigationDirtyAreasController.cpp` | WP Dirty Area 필터 (라인 148) |
| `RecastNavMeshDataChunk.cpp` | `Engine/Source/Runtime/NavigationSystem/Private/NavMesh/RecastNavMeshDataChunk.cpp` | AttachTiles/DetachTiles (라인 203) |
| `NavigationOctree.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavigationOctree.h` | FNavigationOctree |
| `DetourNavMesh.h` | `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMesh.h` | dtNavMesh (라인 502), dtMeshTile (라인 421), dtPoly (라인 205) |
| `DetourNavMeshQuery.h` | `Engine/Source/Runtime/Navmesh/Public/Detour/DetourNavMeshQuery.h` | dtNavMeshQuery |
| `Recast.h` | `Engine/Source/Runtime/Navmesh/Public/Recast/Recast.h` | rcConfig, rcContext |
| `NavArea.h` | `Engine/Source/Runtime/NavigationSystem/Public/NavAreas/NavArea.h` | UNavArea |

## 공식 문서

- [Unreal Engine Navigation System Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-system-in-unreal-engine)
- [Navigation Mesh in Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/navigation-mesh)
- [Nav Modifier Volume](https://dev.epicgames.com/documentation/en-us/unreal-engine/nav-modifier-volume-in-unreal-engine)
- [World Partition - NavMesh](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition---navigation-in-unreal-engine)

## Recast/Detour 원본

- [Recast Navigation GitHub](https://github.com/recastnavigation/recastnavigation)
- [Mikko Mononen 블로그](https://digestingduck.blogspot.com/) — 설계 의도 및 알고리즘 설명

## 관련 이슈

- [#9 DynamicModifiersOnly 쿠킹 빌드 버그 분석](https://github.com/jiy12345/UE5TestProject/issues/9) — 이 분석의 직접적 배경
- [#10 RecastNavMesh 분석](https://github.com/jiy12345/UE5TestProject/issues/10) — 현재 이슈
- [#11 Detour Crowd 분석](https://github.com/jiy12345/UE5TestProject/issues/11) — 다음 단계
