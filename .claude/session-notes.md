# UE5TestProject 진행 상황 기록

## 날짜: 2025-11-30

### 문제 발생
언리얼 엔진 C++ 프로젝트 빌드 중 다음 에러 발생:
```
TVOSTargetPlatformSettingsModule.cpp(3): Error C1083: 포함 파일을 열 수 없습니다.
'IOSTargetPlatformSettings.h': No such file or directory

VisionOSTargetPlatformSettingsModule.cpp(3): Error C1083: 포함 파일을 열 수 없습니다.
'IOSTargetPlatformSettings.h': No such file or directory
```

### 시도한 해결 방법

#### 1차 시도: 엔진 코드 수정 (성공했으나 되돌림)
- `IOSTargetPlatformSettings.h`를 Private에서 Public 디렉토리로 이동
- TVOS/VisionOS의 `Build.cs`에서 PrivateIncludePaths 제거
- 결과: 빌드 성공
- **문제점**: 다른 Windows 사용자들도 같은 문제를 겪었을 것이므로, 엔진을 수정하는 것이 올바른 해결책이 아님

#### 근본 원인 분석
- 프로젝트 구조:
  - Engine이 git submodule로 관리됨 (https://github.com/EpicGames/UnrealEngine.git, branch: 5.5)
  - 현재 커밋: `ef1397773d16` (Xcode16.3 required compilation fixes)
  
- Git log 확인 결과, TVOS/VisionOS 관련 컴파일 에러 수정 커밋이 여러 개 존재:
  - `d15bf4c9a5e8` - Fix TVOS/VisionOS compilation
  - `5bc6795a91f2` - Fix Compile UnrealGame VisionOS failures
  - `2cd041c25437` - Fix TVOS compilation errors
  
- **결론**: 언리얼 엔진 5.5 브랜치에서 Apple 플랫폼 모듈들이 불안정함

#### 2차 시도: BuildConfiguration.xml 설정 (채택)
**파일 위치**: `C:\Users\jiy12\AppData\Roaming\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`

**설정 내용**: Windows에서 불필요한 Apple 플랫폼(IOS, TVOS, Mac) 빌드 제외

```xml
<?xml version="1.0" encoding="utf-8" ?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
    <BuildConfiguration>
        <!-- Disable building for Apple platforms on Windows -->
    </BuildConfiguration>

    <ProjectFileGenerator>
        <!-- Exclude unnecessary platforms from project files -->
        <DisablePlatformProjectGenerators>
            <Platform>IOS</Platform>
            <Platform>TVOS</Platform>
            <Platform>Mac</Platform>
        </DisablePlatformProjectGenerators>
    </ProjectFileGenerator>

    <IOSToolChain>
        <bGeneratedSYMFile>false</bGeneratedSYMFile>
    </IOSToolChain>

    <WindowsPlatform>
        <!-- Windows-specific settings -->
        <Compiler>VisualStudio2022</Compiler>
    </WindowsPlatform>
</Configuration>
```

#### 3차 시도: Visual Studio에서 프로젝트 제외 (실패)
- Solution Explorer에서 TVOS/VisionOS 프로젝트를 찾으려 했으나 존재하지 않음
- 이런 플랫폼 모듈들은 별도 프로젝트가 아닌 **엔진 전체 빌드의 일부**로 포함됨
- Visual Studio Configuration Manager에서도 개별 제외 불가능

#### 4차 시도: 온라인 조사 및 검증
- 다른 Windows 사용자들의 유사 사례를 찾지 못함
- 관련 포럼 포스트들:
  - [How to Disable IOSTargetPlatform during Compilation](https://forums.unrealengine.com/t/how-to-disable-iostargetplatform-during-compilation/320994) - 명확한 해결책 없음
  - BuildConfiguration.xml 공식 문서 - 플랫폼 제외 설정 방법 문서화되지 않음
- **결론**: UE 5.5의 알려지지 않은 버그일 가능성 높음

#### 5차 시도: 엔진 버전 업데이트 확인
- 현재 커밋: `ef1397773d16` = origin/5.5 (최신)
- 엔진 업데이트로는 해결 불가능

### 현재 상태 (2025-11-30 종료 시점)

**적용된 변경사항**:
- BuildConfiguration.xml 생성 완료 (`C:\Users\jiy12\AppData\Roaming\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml`)

**미완료 사항**:
- BuildConfiguration.xml이 아직 적용되지 않음 (프로젝트 파일 재생성 필요)
- 에러는 여전히 발생 중

### 다음 세션에서 시도할 방법

**옵션 A (최우선 권장)**: 언리얼 엔진 5.7로 업그레이드
1. Engine 서브모듈을 5.7 브랜치로 전환
   ```bash
   cd Engine
   git checkout origin/5.7
   git submodule update --init --recursive
   ```
2. 프로젝트 파일 재생성
3. 빌드 시도
4. 5.7에서 이 문제가 해결되었을 가능성 높음

**옵션 B**: 원래 엔진 수정 다시 적용 (5.5 유지 시)
1. `IOSTargetPlatformSettings.h`를 Public으로 이동
2. TVOS/VisionOS Build.cs에서 PrivateIncludePaths 제거
3. 빌드 성공 확인
4. Epic Games에 버그 리포트 제출

**옵션 C**: 프로젝트 파일 재생성 (BuildConfiguration.xml 적용)
1. **프로젝트 파일 재생성** (Windows에서 수행 필요)
   ```powershell
   # 방법 1: .uproject 우클릭 → "Generate Visual Studio project files"
   
   # 방법 2: 배치 파일 실행
   cd C:\Users\jiy12\Projects\UE5TestProject
   .\Engine\Engine\Build\BatchFiles\GenerateProjectFiles.bat -project="$PWD\UE5TestProject.uproject" -game
   ```

2. **빌드 캐시 정리** (선택사항)
   ```powershell
   Remove-Item -Recurse -Force Intermediate -ErrorAction SilentlyContinue
   Remove-Item -Recurse -Force .vs -ErrorAction SilentlyContinue
   ```

3. **Visual Studio에서 다시 빌드**
   - Build → Clean Solution
   - Build → Rebuild Solution

---

## 날짜: 2025-11-30 (세션 2 - 엔진 업그레이드)

### 진행 상황: UE 5.7 업그레이드 시작

**선택한 방법**: 옵션 A - 언리얼 엔진 5.7로 업그레이드

**진행 중 발생한 이슈**:
- WSL에서 `/mnt/c` 경로를 통한 git 작업이 매우 느림
- `git checkout origin/5.7` 명령이 2분 이상 진행되어 중단

**현재 대기 중인 작업** (Windows에서 수행 필요):

1. **엔진 5.7로 체크아웃** (PowerShell 또는 Git Bash에서):
   ```powershell
   cd C:\Users\jiy12\Projects\UE5TestProject\Engine
   git checkout origin/5.7
   git submodule update --init --recursive
   ```

2. **프로젝트 파일 재생성**:
   ```powershell
   cd C:\Users\jiy12\Projects\UE5TestProject
   # 방법 1: .uproject 우클릭 → "Generate Visual Studio project files"
   # 또는
   # 방법 2:
   .\Engine\Engine\Build\BatchFiles\GenerateProjectFiles.bat -project="$PWD\UE5TestProject.uproject" -game
   ```

3. **Visual Studio에서 빌드**:
   - Build → Clean Solution
   - Build → Rebuild Solution
   - TVOS/VisionOS 에러가 해결되었는지 확인

**예상 결과**:
- UE 5.7에서 Apple 플랫폼 모듈 관련 빌드 이슈가 수정되었을 가능성 높음
- 빌드 성공 시 MVVM 구현 작업 계속 진행 가능

---

### 프로젝트 정보
- 프로젝트명: UE5TestProject
- 설명: UE5 MVVM Pattern Implementation and Source Code Analysis Project
- 타겟 플랫폼: Windows
- 사용 플러그인: ModelViewViewModel
- 엔진 버전: Unreal Engine 5.5 → **5.7로 업그레이드 진행 중**

### 참고 사항
- 커밋 컨벤션: Co-Authored-By 및 Claude Code 링크 포함하지 않음
- 커밋 분리를 선호함 (CLAUDE.md에 명시)
- Git 설정:
  - user.name: Ilyong Jung
  - user.email: jiy12345@users.noreply.github.com

---

## 날짜: 2025-11-30 (세션 3 - 폴더 구조 문제 발견 및 재구성)

### UE 5.7 업그레이드 진행 과정

**완료된 작업**:
1. ✅ Engine을 5.7 브랜치로 체크아웃
2. ✅ Setup.bat 실행 (의존성 다운로드)
3. ✅ MSVC v14.44-17.14 설치 (Visual Studio 2022)
4. ✅ Target.cs 파일 업데이트 (BuildSettingsVersion.V6, Unreal5_7)

### 근본 원인 발견: 폴더 구조 문제

**현재 구조 (비정상)**:
```
C:\Users\jiy12\Projects\UE5TestProject\
    Engine\              ← 언리얼 엔진 소스 전체 (git submodule)
        Engine\          ← 실제 엔진 코드
        Templates\
        GenerateProjectFiles.bat
    Source\
    UE5TestProject.uproject
```

**문제점**:
- 빌드 경로가 `Engine\Engine\Build\...`처럼 Engine이 두 번 나타남
- IOSTargetPlatformSettings.h 등 헤더 파일을 찾지 못함
- 이 구조는 언리얼 공식 권장 구조가 아님

**에러 메시지**:
```
LC_Hook.h: No such file or directory
IOSTargetPlatformSettings.h: No such file or directory (TVOS/VisionOS)
Log file: C:\Users\jiy12\Projects\UE5TestProject\Engine\Engine\Programs\...
```

### 해결책: 폴더 구조 재구성

**목표 구조 (패턴 A - 언리얼 공식 권장)**:
```
C:\UE_Source\            ← 언리얼 엔진 루트
    Engine\
    Templates\
    Samples\
    UE5TestProject\      ← 프로젝트 이동
        UE5TestProject.uproject
        Source\
        Content\
        Config\
```

**실제 적용한 구조 (최종)**:
```
UE5TestProject/              ← Git 저장소 루트
    .git/
    .gitmodules
    .claude/
    Engine/                  ← 서브모듈 (언리얼 엔진 소스)
        Engine/
        Templates/
        Samples/
        GenerateProjectFiles.bat
    Game/                    ← 게임 프로젝트
        Source/
        Content/
        Config/
        Plugins/
        UE5TestProject.uproject
```

**작업 단계 (완료)**:
1. ✅ Game 폴더 생성
2. ✅ 프로젝트 파일들 (Source, Content, Config, Plugins, .uproject)을 Game으로 이동
3. ✅ 빌드 캐시 정리 (.vs, Binaries, Intermediate, Saved, DerivedDataCache 삭제)
4. ✅ 프로젝트 파일 재생성 (`Engine/Build/BatchFiles/GenerateProjectFiles.bat -project="Game/UE5TestProject.uproject"`)
5. ✅ 빌드 테스트 - **성공!**

### 결과

**UE 5.7 업그레이드 성공!** ✅

- MSVC 14.44 요구사항 충족
- 폴더 구조 재구성으로 경로 문제 해결
- 빌드 성공 확인

**해결된 문제**:
- ✅ TVOS/VisionOS 빌드 에러 (폴더 구조 문제로 인한 헤더 파일 탐색 실패)
- ✅ `Engine\Engine\...` 이중 경로 문제
- ✅ MSVC 14.44 요구사항
- ✅ BuildSettingsVersion.V6 호환성

**다음 단계**: MVVM 구현 작업 계속 진행
