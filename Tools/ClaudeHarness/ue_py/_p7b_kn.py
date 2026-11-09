import unreal
w = unreal.UnrealEditorSubsystem().get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9SilhouetteStalker):
    b = a.get_editor_property("body"); ai = b.get_anim_instance()
    unreal.log("P9PY: KN mesh %s rel %s mode %s anim %s loc %s" % (b.get_skeletal_mesh_asset().get_name(), b.get_editor_property("relative_location"), b.get_animation_mode(), ai.get_class().get_name() if ai else None, a.get_actor_location()))
