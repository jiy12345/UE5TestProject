# MVVM 플러그인 소스 코드 분석

> **작성일**: 2025-11-30
> **엔진 버전**: UE 5.7
> **관련 이슈**: [#3](https://github.com/jiy12345/UE5TestProject/issues/3)

## 개요

Unreal Engine 5.7 `ModelViewViewModel` 플러그인의 내부 동작 원리를 소스 코드 기반으로 분석합니다.

## 학습 목표

- ViewModel과 View(Widget) 사이의 바인딩 메커니즘 이해
- Field Notify 시스템의 동작 원리 파악
- 바인딩 실행 모드(Immediate/Delayed/Tick)별 차이 이해
- 실제 ViewModel 작성 및 바인딩 설정 방법 습득

## 목차

| 파일 | 내용 |
|------|------|
| [01-plugin-structure.md](01-plugin-structure.md) | 모듈 구성, 핵심 클래스 및 파일 구조 |
| [02-field-notify-mechanism.md](02-field-notify-mechanism.md) | Field Notify 메커니즘, 델리게이트 등록 및 브로드캐스트 |
| [03-binding-execution.md](03-binding-execution.md) | Binding 실행 흐름, ExecutionMode별 분석 |
| [04-lifecycle.md](04-lifecycle.md) | Widget 생성~파괴 전체 라이프사이클 |
| [05-practical-guide.md](05-practical-guide.md) | ViewModel 작성, Binding 설정, 디버깅 팁 |

## 핵심 요약

- **Field Notify 발생 시점**: `UE_MVVM_SET_PROPERTY_VALUE` 매크로 호출로 값이 변경될 때 `BroadcastFieldValueChanged()` 자동 호출
- **Binding 등록**: Widget 생성 시 `InitializeBindings()`에서 각 Field에 콜백 델리게이트 등록
- **Binding 실행 모드**: Immediate(즉시) / Delayed(프레임 끝) / Tick(매 프레임)
- **성능**: 컴파일 타임 경로 인덱싱으로 런타임 O(1) 접근

## 주요 소스 경로

```
Engine/Engine/Plugins/Runtime/ModelViewViewModel/Source/
```

## 선수 지식

- UObject 시스템, UPROPERTY/UFUNCTION
- UMG Widget 기초 (UUserWidget, UWidget)
