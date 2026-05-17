"""
NavDebug 검증용 테스트 맵 자동 생성 스크립트.

캐릭터가 NavMesh 위를 걸어다닐 수 있는 미니멀 환경만 셋업한다.
NavDebugVisualizerComponent 자체의 사용 wrapper는 #19에서 추가 예정.

사용 방법:
    1. UE5 에디터에서 UE5TestProject 열기
    2. Edit > Plugins에서 "Python Editor Script Plugin" 활성화 (재시작 필요할 수 있음)
    3. 실행 (옵션 중 하나):
       - Output Log 콘솔에 `py "C:/.../Game/Scripts/setup_nav_debug_test_map.py"`
       - 또는 메뉴 Tools > Execute Python Script
    4. 자동으로 /Game/Test/NavDebugTest 맵 생성 + 저장됨

결과 맵:
    - Floor: 2000x2000 평면
    - Wall: 장애물 2개
    - NavMeshBoundsVolume: floor 영역 덮음 (NavMesh 빌드 영역)
    - PlayerStart: PIE 시작 위치
    - 조명: DirectionalLight + SkyLight + SkyAtmosphere + ExponentialHeightFog
    - World Settings GameMode Override = AUE5TestProjectGameMode
    - Project GameDefaultMap = 이 맵

PIE 실행 시 AUE5TestProjectCharacter가 PlayerStart에 spawn,
WASD로 NavMesh 위를 걸어다닐 수 있음. 장애물(Wall)에 막히는지 확인.
"""

import unreal


# ---------- 설정 ----------
MAP_PATH = "/Game/Test/NavDebugTest"
GAME_MODE_CLASS_PATH = "/Script/UE5TestProject.UE5TestProjectGameMode"

FLOOR_LOCATION = unreal.Vector(0, 0, 0)
FLOOR_SCALE = unreal.Vector(20, 20, 1)  # 200x200 cm * scale = 2000x2000 cm

WALL_A_LOCATION = unreal.Vector(0, -300, 50)
WALL_B_LOCATION = unreal.Vector(0, 300, 50)
WALL_SCALE = unreal.Vector(0.5, 5, 1)  # 50x500x100 cm

NAV_BOUNDS_LOCATION = unreal.Vector(0, 0, 100)
NAV_BOUNDS_SCALE = unreal.Vector(22, 22, 4)  # NavMesh 빌드 영역 (floor보다 조금 크게)

PLAYER_START_LOCATION = unreal.Vector(-800, 0, 200)


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


def load_class(class_name, module_name="UE5TestProject"):
    """UClass를 다양한 방법으로 로드 시도 — UE Python API가 환경마다 다름."""
    full_path = "/Script/{}.{}".format(module_name, class_name)

    # 1. unreal 네임스페이스 직접 접근 (게임 모듈 클래스도 Python에 자동 등록되는 경우 있음)
    cls = getattr(unreal, class_name, None)
    if cls is not None:
        log("  load_class: getattr(unreal, '{}') 성공".format(class_name))
        return cls

    # 2. load_class — UClass 로드 (가장 정통적이지만 환경 따라 실패)
    try:
        cls = unreal.load_class(None, full_path)
        if cls is not None:
            log("  load_class: unreal.load_class('{}') 성공".format(full_path))
            return cls
    except Exception as e:
        log("  load_class 시도 실패: {}".format(e))

    # 3. load_object — UClass도 UObject이므로 load_object로 가능
    try:
        obj = unreal.load_object(None, full_path)
        if obj is not None:
            log("  load_class: unreal.load_object('{}') 성공".format(full_path))
            return obj
    except Exception as e:
        log("  load_object 시도 실패: {}".format(e))

    # 4. find_class — 짧은 이름으로 native UClass 검색
    try:
        cls = unreal.find_class(class_name)
        if cls is not None:
            log("  load_class: unreal.find_class('{}') 성공".format(class_name))
            return cls
    except Exception as e:
        log("  find_class 시도 실패: {}".format(e))

    log("  load_class 전체 실패: {} (모듈 미로드 또는 reflection 등록 안 됨)".format(full_path))
    return None


# ---------- 맵 생성 ----------
def clear_user_actors():
    """현재 레벨의 사용자 actor 모두 삭제 (WorldSettings, Brush 등 system actor는 제외)."""
    actor_subsystem = get_editor_actor_subsystem()
    if actor_subsystem is None:
        log("  EditorActorSubsystem 없음 — actor 정리 스킵")
        return

    all_actors = actor_subsystem.get_all_level_actors() if hasattr(actor_subsystem, "get_all_level_actors") \
        else unreal.EditorLevelLibrary.get_all_level_actors()

    # 보존할 system actor 클래스 이름
    keep_classes = ("WorldSettings", "Brush", "AbstractNavData", "WorldDataLayers", "LevelInstance",
                    "WorldPartitionVolume", "DefaultPhysicsVolume")

    removed = 0
    for actor in all_actors:
        if actor is None:
            continue
        cls_name = actor.get_class().get_name()
        if cls_name in keep_classes:
            continue
        try:
            if hasattr(actor_subsystem, "destroy_actor"):
                actor_subsystem.destroy_actor(actor)
            else:
                unreal.EditorLevelLibrary.destroy_actor(actor)
            removed += 1
        except Exception:
            pass

    log("  기존 actor {}개 삭제".format(removed))


def create_new_level():
    """레벨 준비: 기존 맵이 있으면 load + actor 정리, 없으면 new_level."""
    level_subsystem = get_level_editor_subsystem()
    if level_subsystem is None:
        log("LevelEditorSubsystem 없음 — UE 버전 호환 안 됨")
        return False

    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        log("기존 맵 발견: load 후 actor 정리")
        loaded = level_subsystem.load_level(MAP_PATH) if hasattr(level_subsystem, "load_level") \
            else unreal.EditorLevelLibrary.load_level(MAP_PATH)
        if not loaded:
            log("  load_level 실패 — 수동 삭제 필요: Content Browser에서 NavDebugTest 우클릭 → Delete")
            return False
        clear_user_actors()
        return True

    log("기존 맵 없음 — Creating new level at {}".format(MAP_PATH))
    success = level_subsystem.new_level(MAP_PATH)
    if not success:
        log("Failed to create new level — 권한 또는 잔여 상태 문제. 에디터 재시작 후 재시도.")
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


def setup_player_start():
    log("Spawning PlayerStart")
    return spawn_actor(unreal.PlayerStart, PLAYER_START_LOCATION, "PlayerStart")


def _set_mobility(actor, mobility):
    """Actor의 root component mobility를 변경. actor.set_mobility 우선, 실패 시 component 직접."""
    try:
        if hasattr(actor, "set_mobility"):
            actor.set_mobility(mobility)
            return
    except Exception:
        pass
    try:
        root = actor.root_component
        if root is not None:
            root.set_mobility(mobility)
    except Exception:
        pass


def setup_lighting():
    """최소 조명 — DirectionalLight 1개 + PostProcessVolume만.
    SkyLight는 SkyAtmosphere/IsSky 메시 없으면 검은 환경광이 되므로 제외."""
    log("Spawning minimal lighting (DirectionalLight only + PostProcessVolume)")

    # DirectionalLight — 유일한 빛 source. Intensity 높여 환경광 부재 보완
    sun = spawn_actor(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), "DirectionalLight")
    if sun is not None:
        _set_mobility(sun, unreal.ComponentMobility.MOVABLE)
        sun.set_actor_rotation(unreal.Rotator(-45.0, 0.0, 0.0), False)
        try:
            light_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
            if light_comp is not None:
                try:
                    light_comp.set_mobility(unreal.ComponentMobility.MOVABLE)
                except Exception:
                    pass
                # 환경광 없으니 직접광 강도 ↑ (그늘진 면은 여전히 어두움, NavDebug엔 충분)
                light_comp.set_editor_property("intensity", 10.0)
        except Exception as e:
            log("  DirectionalLight 속성 설정 실패 (무시): {}".format(e))

    # PostProcessVolume — Auto Exposure를 Manual로 고정해서 PIE에서 균일 노출
    # (디폴트 Auto Exposure가 어두운 씬을 더 어둡게 적응시키는 문제 회피)
    ppv = spawn_actor(unreal.PostProcessVolume, unreal.Vector(0, 0, 0), "GlobalPostProcess")
    if ppv is not None:
        try:
            ppv.set_editor_property("unbound", True)  # 전체 월드 적용
            settings = ppv.get_editor_property("settings")
            # 여러 속성명 fallback (UE 버전별 차이)
            for method_attr, bias_attr in (
                ("auto_exposure_method", "auto_exposure_bias"),
            ):
                try:
                    settings.set_editor_property(method_attr,
                        unreal.AutoExposureMethod.AEM_MANUAL)
                    settings.set_editor_property(bias_attr, 5.0)
                    # override 플래그 (Manual 모드 적용을 위해 ON)
                    for override_attr in (
                        "override_auto_exposure_method",
                        "override_auto_exposure_bias",
                    ):
                        try:
                            settings.set_editor_property(override_attr, True)
                        except Exception:
                            pass
                    break
                except Exception as e:
                    log("  PostProcess Exposure 설정 실패 (무시): {}".format(e))
            ppv.set_editor_property("settings", settings)
        except Exception as e:
            log("  PostProcessVolume 설정 실패 (무시): {}".format(e))


def build_navigation():
    """NavMesh 빌드 트리거. 비동기로 진행되며 빌드 완료까지 시간 걸림."""
    log("Triggering NavMesh build (RebuildNavigation)")
    try:
        world = get_editor_world()
        if world is None:
            log("  Editor World 없음 — 수동 빌드 필요: Build > Build Paths")
            return

        unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
        log("  RebuildNavigation 콘솔 명령 실행 — 빌드는 비동기. 완료 후 한 번 더 Save Level (Ctrl+S) 권장.")
    except Exception as e:
        log("  NavMesh 빌드 트리거 실패 (무시): {} — 수동 빌드: Build > Build Paths".format(e))


def get_editor_world():
    """현재 에디터 월드 — UE 5.x UnrealEditorSubsystem 우선, EditorLevelLibrary는 deprecated fallback."""
    try:
        editor_sub = get_subsystem(unreal.UnrealEditorSubsystem)
        if editor_sub is not None and hasattr(editor_sub, "get_editor_world"):
            return editor_sub.get_editor_world()
    except Exception:
        pass
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        pass
    return None


def setup_world_game_mode_override():
    """World Settings의 GameMode Override를 AUE5TestProjectGameMode로 설정."""
    log("Setting World Settings GameMode Override")
    world = get_editor_world()
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
        # UE 5.x: get_default_object 대신 클래스 메서드 + SaveConfig (UClass 레벨)
        settings = unreal.get_default_object(unreal.GameMapsSettings)
        map_soft_path = unreal.SoftObjectPath(MAP_PATH)
        settings.set_editor_property("game_default_map", map_soft_path)

        # save_config는 UE 5.x에서 메서드명/시그니처가 환경에 따라 다름 — 여러 경로 시도
        saved = False
        for method_name in ("save_config", "modify"):
            if hasattr(settings, method_name):
                try:
                    getattr(settings, method_name)()
                    saved = True
                    break
                except Exception:
                    continue
        if not saved:
            log("  GameDefaultMap 메모리 설정은 성공, ini 저장은 수동 필요:")
            log("    Project Settings > Maps & Modes > Game Default Map = NavDebugTest → 저장")
        else:
            log("  GameDefaultMap = {} (DefaultEngine.ini에 저장됨)".format(MAP_PATH))
    except Exception as e:
        log("  GameDefaultMap 설정 실패 (무시): {}".format(e))
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
    setup_player_start()
    setup_lighting()
    setup_world_game_mode_override()
    setup_project_game_default_map()
    build_navigation()

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
