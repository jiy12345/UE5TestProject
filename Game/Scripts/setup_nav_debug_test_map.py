"""
NavDebug 검증용 테스트 맵 자동 생성 스크립트.

사용 방법:
    1. UE5 에디터에서 UE5TestProject 열기
    2. Edit > Plugins에서 "Python Editor Script Plugin" 활성화 (재시작 필요할 수 있음)
    3. Window > Output Log 패널 → 콘솔 입력 모드 "Python" 선택
    4. 다음 명령 실행:
         exec(open(r"<프로젝트 경로>/Game/Scripts/setup_nav_debug_test_map.py").read())
       또는 Tools > Execute Python Script 메뉴에서 파일 선택
    5. 자동으로 /Game/Test/NavDebugTest 맵 생성 + 저장됨

결과 맵:
    - Floor: 2000x2000 평면 (Engine/BasicShapes/Floor)
    - Wall: 장애물 2개 (경로 우회 검증용)
    - NavMeshBoundsVolume: floor 영역 덮음 (NavMesh 빌드 영역)
    - PathStart, PathEnd: 빈 Actor 2개 (NavDebug 컴포넌트가 참조)
    - NavDebugActor: AActor + UNavDebugVisualizerComponent (PathStart/End 할당 + bEnabled)
    - PlayerStart: PIE 시작 카메라

검증:
    PIE 실행 시 NavDebugActor의 컴포넌트가 매 프레임 경로를 시각화한다.
    - 초록 선 + 노란 구체: 정상 경로
    - PathEnd를 NavMesh 밖으로 옮기면: 빨간 선 + "PATH NOT FOUND"
    - Wall 사이로 PathStart/End 위치 조정 시 우회 경로 확인
"""

import unreal


# ---------- 설정 ----------
MAP_PATH = "/Game/Test/NavDebugTest"
NAV_DEBUG_COMPONENT_CLASS = "UNavDebugVisualizerComponent"
GAME_MODE_CLASS_PATH = "/Script/UE5TestProject.UE5TestProjectGameMode"

FLOOR_LOCATION = unreal.Vector(0, 0, 0)
FLOOR_SCALE = unreal.Vector(20, 20, 1)  # 200x200 cm * scale = 2000x2000 cm

WALL_A_LOCATION = unreal.Vector(0, -300, 50)
WALL_B_LOCATION = unreal.Vector(0, 300, 50)
WALL_SCALE = unreal.Vector(0.5, 5, 1)  # 50x500x100 cm

NAV_BOUNDS_LOCATION = unreal.Vector(0, 0, 100)
NAV_BOUNDS_SCALE = unreal.Vector(22, 22, 4)  # NavMesh 빌드 영역 (floor보다 조금 크게)

PATH_START_LOCATION = unreal.Vector(-800, 0, 50)
PATH_END_LOCATION = unreal.Vector(800, 0, 50)

NAV_DEBUG_ACTOR_LOCATION = unreal.Vector(0, 0, 200)

PLAYER_START_LOCATION = unreal.Vector(-1000, -1000, 200)


# ---------- 유틸 ----------
def log(msg):
    unreal.log("[setup_nav_debug_test_map] {}".format(msg))


def get_subsystem(cls):
    return unreal.get_editor_subsystem(cls)


def get_level_editor_subsystem():
    return get_subsystem(unreal.LevelEditorSubsystem)


def get_editor_actor_subsystem():
    return get_subsystem(unreal.EditorActorSubsystem)


def get_editor_asset_subsystem():
    return get_subsystem(unreal.EditorAssetSubsystem)


def spawn_actor(actor_class, location, label=None):
    """Subsystem 우선 사용, fallback으로 deprecated API."""
    actor_subsystem = get_editor_actor_subsystem()
    if actor_subsystem is not None:
        actor = actor_subsystem.spawn_actor_from_class(actor_class, location, unreal.Rotator(0, 0, 0))
    else:
        # Deprecated fallback (UE 5.0~5.2)
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, unreal.Rotator(0, 0, 0))

    if actor is not None and label is not None:
        actor.set_actor_label(label)
    return actor


def spawn_static_mesh(mesh_path, location, scale, label):
    """StaticMeshActor 스폰 + 메시 할당 + 스케일 설정."""
    actor = spawn_actor(unreal.StaticMeshActor, location, label)
    if actor is None:
        log("Failed to spawn StaticMeshActor: {}".format(label))
        return None

    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        log("Failed to load mesh asset: {}".format(mesh_path))
        return actor

    mesh_component = actor.get_editor_property("static_mesh_component")
    mesh_component.set_static_mesh(mesh)
    actor.set_actor_scale3d(scale)
    return actor


def load_class(class_name):
    """UClass를 이름으로 로드. UNavDebugVisualizerComponent 같이 게임 모듈 클래스 찾기."""
    return unreal.load_class(None, "/Script/UE5TestProject.{}".format(class_name))


# ---------- 맵 생성 ----------
def create_new_level():
    """빈 레벨 새로 생성 + 지정 경로로 저장."""
    level_subsystem = get_level_editor_subsystem()
    if level_subsystem is None:
        log("LevelEditorSubsystem 없음 — UE 버전 호환 안 됨")
        return False

    log("Creating new level at {}".format(MAP_PATH))
    success = level_subsystem.new_level(MAP_PATH)
    if not success:
        log("Failed to create new level — 경로 충돌 또는 권한 문제")
        return False
    return True


def setup_floor():
    log("Spawning Floor")
    return spawn_static_mesh(
        "/Engine/BasicShapes/Cube",
        FLOOR_LOCATION,
        FLOOR_SCALE,
        "Floor")


def setup_walls():
    log("Spawning Walls (2 obstacles)")
    spawn_static_mesh("/Engine/BasicShapes/Cube", WALL_A_LOCATION, WALL_SCALE, "Wall_A")
    spawn_static_mesh("/Engine/BasicShapes/Cube", WALL_B_LOCATION, WALL_SCALE, "Wall_B")


def setup_nav_mesh_bounds():
    log("Spawning NavMeshBoundsVolume")
    actor = spawn_actor(unreal.NavMeshBoundsVolume, NAV_BOUNDS_LOCATION, "NavMeshBounds")
    if actor is not None:
        actor.set_actor_scale3d(NAV_BOUNDS_SCALE)
    return actor


def setup_path_actors():
    log("Spawning PathStart and PathEnd (empty Actors)")
    start = spawn_actor(unreal.Actor, PATH_START_LOCATION, "PathStart")
    end = spawn_actor(unreal.Actor, PATH_END_LOCATION, "PathEnd")
    return start, end


def setup_nav_debug_actor(path_start, path_end):
    log("Spawning NavDebugActor + attaching UNavDebugVisualizerComponent")
    actor = spawn_actor(unreal.Actor, NAV_DEBUG_ACTOR_LOCATION, "NavDebugActor")
    if actor is None:
        return None

    comp_class = load_class(NAV_DEBUG_COMPONENT_CLASS)
    if comp_class is None:
        log("Failed to load class: {}. 컴포넌트 부착은 사용자가 에디터에서 수동으로 추가 필요.".format(
            NAV_DEBUG_COMPONENT_CLASS))
        return actor

    # 런타임 컴포넌트 추가 (에디터에서 영구 저장됨)
    comp = actor.add_component_by_class(comp_class, False, unreal.Transform(), False)
    if comp is None:
        log("Failed to add component. 사용자가 에디터에서 수동으로 추가 필요.")
        return actor

    # UPROPERTY 설정
    comp.set_editor_property("PathStart", path_start)
    comp.set_editor_property("PathEnd", path_end)
    comp.set_editor_property("bEnabled", True)
    comp.set_editor_property("PathLineThickness", 5.0)

    return actor


def setup_player_start():
    log("Spawning PlayerStart")
    return spawn_actor(unreal.PlayerStart, PLAYER_START_LOCATION, "PlayerStart")


def setup_world_game_mode_override():
    """World Settings의 GameMode Override를 AUE5TestProjectGameMode로 설정."""
    log("Setting World Settings GameMode Override")
    level_subsystem = get_level_editor_subsystem()
    if level_subsystem is None:
        log("  LevelEditorSubsystem 없음 — World Settings 설정 스킵")
        return

    world = level_subsystem.get_editor_world() if hasattr(level_subsystem, "get_editor_world") \
        else unreal.EditorLevelLibrary.get_editor_world()
    if world is None:
        log("  Editor World 없음 — 스킵")
        return

    world_settings = world.get_world_settings()
    gm_class = unreal.load_class(None, GAME_MODE_CLASS_PATH)
    if gm_class is None:
        log("  AUE5TestProjectGameMode 클래스 로드 실패 — 게임 모듈 빌드 확인 필요")
        return

    world_settings.set_editor_property("default_game_mode", gm_class)
    log("  World Settings GameMode Override = AUE5TestProjectGameMode")


def setup_project_game_default_map():
    """Project Settings의 GameDefaultMap을 우리 맵으로. 게임 빌드 실행 시 자동 로드."""
    log("Setting Project Settings GameDefaultMap")
    try:
        settings = unreal.GameMapsSettings.get_default_object()
        map_soft_path = unreal.SoftObjectPath(MAP_PATH)
        settings.set_editor_property("game_default_map", map_soft_path)
        # 영구 저장 (DefaultEngine.ini에 기록)
        settings.save_config()
        log("  GameDefaultMap = {}".format(MAP_PATH))
    except Exception as e:
        log("  GameDefaultMap 설정 실패 (UE 버전 API 차이 가능): {}".format(e))
        log("  수동 설정: Project Settings > Maps & Modes > Game Default Map = NavDebugTest")


def save_level():
    log("Saving level")
    level_subsystem = get_level_editor_subsystem()
    if level_subsystem is not None:
        return level_subsystem.save_current_level()
    return unreal.EditorLevelLibrary.save_current_level()


# ---------- 메인 ----------
def main():
    log("=" * 60)
    log("NavDebug 테스트 맵 생성 시작")
    log("=" * 60)

    if not create_new_level():
        log("레벨 생성 실패 — 중단")
        return

    setup_floor()
    setup_walls()
    setup_nav_mesh_bounds()
    path_start, path_end = setup_path_actors()
    setup_nav_debug_actor(path_start, path_end)
    setup_player_start()
    setup_world_game_mode_override()
    setup_project_game_default_map()

    saved = save_level()
    if saved:
        log("=" * 60)
        log("완료! 맵: {}".format(MAP_PATH))
        log("NavMesh 빌드는 자동으로 트리거되거나 Build > Build Paths로 수동 빌드.")
        log("PIE 실행하면 NavDebugActor 위치에서 경로 시각화 확인 가능.")
        log("=" * 60)
    else:
        log("저장 실패 — 에디터에서 수동으로 저장 필요")


if __name__ == "__main__":
    main()
else:
    # exec() 또는 Execute Python Script로 실행될 때
    main()
