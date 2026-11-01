# Stage 5B: save ONLY the station map (refuses if any other package is dirty).
import unreal
maps = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
content = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()]
world = unreal.UnrealEditorSubsystem().get_editor_world()
if content or maps != ["/Game/P9/Maps/L_Station_Redesign"] or not world.get_path_name().startswith("/Game/P9/Maps/L_Station_Redesign"):
    unreal.log_warning("P9PY: save_map_5b refused - maps %s content %s world %s" % (maps, content, world.get_path_name()))
else:
    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/P9/Maps/L_Station_Redesign")
    unreal.log("P9PY: save_map_5b saved=%s" % ok)
