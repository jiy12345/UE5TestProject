# Editor Python Scripts

UE5 에디터에서 실행하는 자동화 스크립트 모음.

## 사전 준비 — Python Editor Script Plugin 활성화

1. UE5 에디터 열기 (`Game/UE5TestProject.uproject`)
2. `Edit > Plugins`
3. 검색: `Python Editor Script Plugin` → 활성화
4. 에디터 재시작 (변경사항 적용)

활성화 후 `Window > Output Log` 패널에서 입력 모드를 `Python`으로 변경하면 Python 콘솔 사용 가능.

## 스크립트 목록

### `setup_nav_debug_test_map.py`

NavDebug 검증용 테스트 맵 자동 생성 (`/Game/Test/NavDebugTest`).

**실행 방법 (UE 5.x 권장 순)**:

**옵션 A — `py` 콘솔 명령 (가장 쉬움)**:

Output Log 콘솔 입력란에 그대로 입력 (입력 모드 `Cmd` 그대로):

```
py "C:/Users/jiy12/Projects/UE5TestProject/Game/Scripts/setup_nav_debug_test_map.py"
```

(따옴표 포함, 슬래시·역슬래시 둘 다 OK)

**옵션 B — `Tools` 메뉴**:

에디터 메뉴 `Tools > Execute Python Script` → 파일 다이얼로그에서 `Game/Scripts/setup_nav_debug_test_map.py` 선택.

**옵션 C — Python 콘솔 모드로 변경 후 직접 실행**:

Output Log 패널 콘솔 입력란의 **왼쪽 드롭다운**에서 `Cmd` → `Python` 변경 → 그 후 다음 입력:

```python
exec(open(r"C:/Users/jiy12/Projects/UE5TestProject/Game/Scripts/setup_nav_debug_test_map.py").read())
```

(주의: `Cmd` 모드에서 `exec(...)`는 deprecated. 반드시 `Python` 모드로 변경 후 실행)

**옵션 D — 명령행 (헤드리스, 자동화)**:

```bash
"<Engine 경로>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "<프로젝트 경로>/Game/UE5TestProject.uproject" \
  -ExecutePythonScript="<프로젝트 경로>/Game/Scripts/setup_nav_debug_test_map.py"
```

**실행 후**:
- `/Game/Test/NavDebugTest.umap` 자동 생성됨
- 에디터 Content Browser에서 확인 가능
- 첫 로드 후 `Build > Build Paths` 한 번 실행 권장 (NavMesh 빌드)
- PIE 실행 → NavDebugActor의 경로 시각화 확인

## 트러블슈팅

### `unreal.LevelEditorSubsystem 없음`

UE 버전이 5.0 이전이거나 Python 플러그인이 너무 오래된 경우. UE 5.1+ 권장.

### `load_class` 실패 — `UNavDebugVisualizerComponent` 못 찾음

이 컴포넌트는 우리 게임 모듈(`UE5TestProject`) 에 정의됨. 게임 모듈이 컴파일되어 있어야 함:
1. 에디터 종료
2. `UnrealBuildTool`로 빌드 (또는 .sln 빌드)
3. 에디터 재시작 후 스크립트 실행

### NavMesh가 보이지 않음

- `Show > Navigation` 토글
- 또는 PIE 후 `P` 키 (콘솔의 `show navigation`)
- 처음 로드 시 `Build > Build Paths` 수동 실행 필요할 수 있음

### 스크립트가 일부만 실행되고 멈춤

- `Output Log`에서 오류 메시지 확인
- UE Python API 버전 호환성 — UE 5.1+ 권장. 5.0 이전은 `EditorLevelLibrary` deprecated API 사용 (스크립트가 자동 fallback)
