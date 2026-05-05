# Engine 모듈 실무 노트

UE 모듈 `Engine` (`Engine/Source/Runtime/Engine/`) 사용 시 노하우. 가장 큰 모듈이자 거의 모든 게임 모듈이 의존하는 핵심.

`Engine` 모듈에는 AActor, UActorComponent, UWorld, GameMode, GameInstance 등 게임 프레임워크의 베이스가 들어있다.

Build.cs 일반 룰은 [`../BuildAndModule.md`](../BuildAndModule.md) 참고.

## 주제별 문서

- [ActorComponent.md](ActorComponent.md) — UActorComponent / USceneComponent / UPrimitiveComponent 선택
- [ComponentTick.md](ComponentTick.md) — 컴포넌트 Tick 활성화/비활성화의 2단계 구조

## References

- 모듈 위치: `Engine/Source/Runtime/Engine/`
- 출처: #19 NavDebugVisualizer 단계 2
