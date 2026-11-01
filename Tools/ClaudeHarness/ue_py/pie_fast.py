import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
unreal.GameplayStatics.set_global_time_dilation(w, 1.0)
unreal.log("P9PY: time dilation %s" % unreal.GameplayStatics.get_global_time_dilation(w))
