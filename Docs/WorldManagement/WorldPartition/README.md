# World Partition 시스템 분석

> **작성일**: 2025-12-16
> **엔진 버전**: Unreal Engine 5.7
> **분석 기준**: 엔진 소스 코드

## 개요

World Partition은 UE5에서 도입된 **대규모 오픈 월드를 위한 자동 스트리밍 시스템**입니다. 기존의 Level Streaming 방식을 대체하며, 공간 기반의 자동화된 셀 분할과 스트리밍을 제공합니다.

## 목차

1. [01-Overview.md](01-Overview.md) - 전체 개념 소개
   - World Partition이란?
   - Level Streaming과의 차이점
   - 주요 특징 및 장점

2. [02-Architecture.md](02-Architecture.md) - 아키텍처 및 설계
   - 전체 시스템 구조
   - 핵심 컴포넌트
   - 설계 철학

3. [03-CoreClasses.md](03-CoreClasses.md) - 핵심 클래스 분석
   - UWorldPartition
   - UWorldPartitionRuntimeHash
   - Actor Descriptor 시스템
   - Streaming Cell

4. [04-StreamingSystem.md](04-StreamingSystem.md) - 스트리밍 시스템
   - Runtime Spatial Hashing
   - Streaming Source
   - Cell 로딩/언로딩 메커니즘
   - 성능 최적화

5. [05-ActorDescriptor.md](05-ActorDescriptor.md) - Actor Descriptor 시스템
   - FWorldPartitionActorDesc 구조
   - 메타데이터 관리
   - 직렬화 시스템

6. [06-HLOD.md](06-HLOD.md) - HLOD 통합
   - HLOD 시스템 개요
   - World Partition과의 통합
   - 자동 생성 및 관리

7. [07-PracticalGuide.md](07-PracticalGuide.md) - 실전 활용 가이드
   - World Partition 설정
   - 런타임 스트리밍 제어
   - 디버깅 방법

8. [References.md](References.md) - 참고 자료

## 주요 개념 빠른 참조

### UWorldPartition (WorldPartition.h:141)
메인 시스템 클래스로, 월드의 액터들을 관리하고 스트리밍을 제어합니다.

```cpp
UCLASS(AutoExpandCategories=(WorldPartition), MinimalAPI)
class UWorldPartition final : public UObject,
                               public FActorDescContainerInstanceCollection,
                               public IWorldPartitionCookPackageGenerator
```

### Runtime Spatial Hash
공간을 그리드로 분할하여 각 셀에 액터를 배치하고, 플레이어 위치 기반으로 동적 스트리밍을 수행합니다.

### Actor Descriptor
액터의 메타데이터를 경량 구조로 저장하여, 실제 액터를 로드하지 않고도 정보를 조회할 수 있습니다.

## 소스 코드 위치

### Runtime 코드
- **Public**: `Engine/Source/Runtime/Engine/Public/WorldPartition/`
- **Private**: `Engine/Source/Runtime/Engine/Private/WorldPartition/`

### Editor 코드
- **UnrealEd**: `Engine/Source/Editor/UnrealEd/Public/WorldPartition/`
- **WorldPartitionEditor**: `Engine/Source/Editor/WorldPartitionEditor/`

### HLOD 플러그인
- `Engine/Plugins/Editor/WorldPartitionHLODUtilities/`

## 관련 시스템

- [Data Layers](../DataLayers/) - World Partition과 함께 사용되는 레이어 시스템
- Level Streaming - 기존 레벨 스트리밍 시스템 (World Partition이 대체)
- HLOD (Hierarchical Level of Detail)

## 학습 순서

1. **기초**: 01-Overview.md로 전체 개념 이해
2. **구조**: 02-Architecture.md로 시스템 설계 파악
3. **구현**: 03-CoreClasses.md에서 핵심 클래스 분석
4. **심화**: 04-StreamingSystem.md, 05-ActorDescriptor.md로 세부 메커니즘 이해
5. **실전**: 07-PracticalGuide.md로 실제 활용 방법 학습

## 주요 특징

✅ **자동 셀 분할**: 수동으로 레벨을 나눌 필요 없음
✅ **공간 기반 스트리밍**: 플레이어 위치에 따라 자동으로 로드/언로드
✅ **대규모 월드 지원**: 무제한 크기의 오픈 월드 제작 가능
✅ **HLOD 자동 생성**: 먼 거리의 오브젝트를 자동으로 최적화
✅ **Data Layers 통합**: 레이어별 스트리밍 제어
✅ **원네트워크 지원**: 서버/클라이언트 스트리밍 최적화

## 시작하기

World Partition을 처음 접한다면 [01-Overview.md](01-Overview.md)부터 시작하세요.
