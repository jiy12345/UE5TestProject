# World Partition 참고 자료

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7

## 엔진 소스 코드

### Runtime 핵심 파일

**메인 시스템**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartition.h` - UWorldPartition 클래스
- `Engine/Source/Runtime/Engine/Private/WorldPartition/WorldPartition.cpp` - 구현

**Runtime Hash**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeHash.h` - 추상 클래스
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeSpatialHash.h` - Spatial Hash 구현
- `Engine/Source/Runtime/Engine/Private/WorldPartition/WorldPartitionRuntimeSpatialHash.cpp`

**Streaming**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionStreamingPolicy.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionStreamingSource.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeCell.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionRuntimeLevelStreamingCell.h`

**Actor Descriptor**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionActorDesc.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionHandle.h`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/ActorDescContainer.h`

**Subsystem**:
- `Engine/Source/Runtime/Engine/Public/WorldPartition/WorldPartitionSubsystem.h`
- `Engine/Source/Runtime/Engine/Private/WorldPartition/WorldPartitionSubsystem.cpp`

### Editor 파일

**에디터 시스템**:
- `Engine/Source/Editor/UnrealEd/Public/WorldPartition/WorldPartitionEditorHash.h`
- `Engine/Source/Editor/WorldPartitionEditor/` - 전체 에디터 모듈

**HLOD**:
- `Engine/Plugins/Editor/WorldPartitionHLODUtilities/Source/Public/WorldPartition/HLOD/`
- `Engine/Source/Runtime/Engine/Public/WorldPartition/HLOD/HLODLayer.h`

## 공식 문서

### Unreal Engine 공식 문서
- [World Partition Overview](https://docs.unrealengine.com/5.7/en-US/world-partition-in-unreal-engine/)
- [World Partition in Unreal Engine](https://docs.unrealengine.com/5.7/en-US/WorldFeatures/WorldPartition/)
- [Migrating Worlds to World Partition](https://docs.unrealengine.com/5.7/en-US/migrating-worlds-to-world-partition-in-unreal-engine/)

### API Reference
- [UWorldPartition API](https://docs.unrealengine.com/5.7/en-US/API/Runtime/Engine/WorldPartition/UWorldPartition/)
- [UWorldPartitionSubsystem API](https://docs.unrealengine.com/5.7/en-US/API/Runtime/Engine/WorldPartition/UWorldPartitionSubsystem/)

## 학습 자료

### Epic Games 공식 비디오
- [Introduction to World Partition](https://www.youtube.com/watch?v=PLN_qBpYBBc) - Unreal Engine 5 Release
- [World Partition - Build Bigger: Change the Game](https://www.youtube.com/watch?v=B0M-5XN-Eg8)
- [UE5 World Building: World Partition, Nanite, and Lumen](https://dev.epicgames.com/community/learning/)

### 커뮤니티 자료
- [Unreal Engine Forums - World Partition](https://forums.unrealengine.com/tags/c/development-discussion/world-building/16/world-partition)
- [Reddit - r/unrealengine World Partition discussions](https://www.reddit.com/r/unrealengine/search/?q=world%20partition)

## 샘플 프로젝트

### 엔진 내장 샘플
- `Engine/Content/WorldPartition/` - 샘플 맵 및 애셋
- Content Examples 프로젝트 (Marketplace에서 무료)

### Epic Games 샘플 프로젝트
- [Lyra Starter Game](https://www.unrealengine.com/marketplace/en-US/product/lyra) - World Partition 활용 예제
- [Valley of the Ancient Sample](https://www.unrealengine.com/marketplace/en-US/product/ancient-valley) - 대규모 오픈 월드

## 관련 시스템 문서

### Data Layers
- [../DataLayers/README.md](../DataLayers/README.md) - Data Layers 시스템 분석

### Level Streaming (기존 방식)
- [Level Streaming Overview](https://docs.unrealengine.com/5.7/en-US/level-streaming-in-unreal-engine/)

### HLOD
- [Hierarchical Level of Detail](https://docs.unrealengine.com/5.7/en-US/hierarchical-level-of-detail-in-unreal-engine/)

### Nanite (관련 최적화 기술)
- [Nanite Virtualized Geometry](https://docs.unrealengine.com/5.7/en-US/nanite-virtualized-geometry-in-unreal-engine/)

## 개발 도구

### 디버깅 명령어

콘솔에서 사용 가능한 명령어:

```
# World Partition 상태 표시
wp.Runtime.ToggleDrawRuntimeHash2D

# Cell 경계 표시
wp.Runtime.ToggleDrawRuntimeCellsDetails

# Streaming Source 표시
wp.Runtime.ToggleDrawStreamingSources

# 로딩 상태 로그
wp.Runtime.EnableStreamingSourceLogging 1

# 성능 통계
stat WorldPartition
```

### Editor Settings

**Project Settings → World Partition**:
- Enable Streaming
- Default Grid Size
- Loading Range

### Content Browser 필터
- Data Layers: `Class:DataLayerAsset`
- World Partition Maps: `WorldPartitionEnabled:True`

## 문제 해결

### 공통 이슈

**이슈**: World Partition이 활성화되지 않음
- **해결**: Project Settings → Enable World Partition

**이슈**: Actors가 로드되지 않음
- **해결**: Data Layer 상태 확인 (`wp.Runtime.ToggleDrawDataLayers`)

**이슈**: 성능 저하
- **해결**: Cell 크기 조정, Loading Range 최적화

### 로그 카테고리

```cpp
// WorldPartitionLog.h
DECLARE_LOG_CATEGORY_EXTERN(LogWorldPartition, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogWorldPartitionRuntimeHash, Log, All);
```

**로그 활성화**:
```
# DefaultEngine.ini or Console
[Core.Log]
LogWorldPartition=VeryVerbose
LogWorldPartitionRuntimeHash=Verbose
```

## 추가 학습 자료

### 관련 논문 및 기술 문서
- Spatial Hashing Techniques in Game Engines
- Dynamic Level of Detail Systems
- Memory Management in Open World Games

### Game Engine Architecture
- "Game Engine Architecture" (Jason Gregory) - Chapter on World Management
- "Real-Time Rendering" - LOD and Streaming techniques

## 업데이트 이력

- **UE 5.0**: World Partition 도입
- **UE 5.1**: Data Layers 안정화, HLOD 개선
- **UE 5.2**: External Data Layers 추가
- **UE 5.3**: Streaming 성능 최적화
- **UE 5.7**: 현재 버전 (안정화, 버그 수정)

## 기여 및 피드백

이 문서에 대한 피드백이나 추가 자료가 있다면:
- GitHub Issues: [프로젝트 Issue 페이지]
- 직접 수정 후 Pull Request

---

**마지막 업데이트**: 2025-12-16
**문서 버전**: 1.0
