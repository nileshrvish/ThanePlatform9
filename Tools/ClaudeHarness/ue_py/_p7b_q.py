import unreal
w = unreal.UnrealEditorSubsystem().get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9FriendNPC):
    unreal.log("P9PY: %s fitted=%s rise=%.3f tread=%.1f up=%s" % (a.get_editor_property("friend_key"), a.get_editor_property("stair_clips_fitted"), a.get_editor_property("stair_rise"), a.get_editor_property("stair_tread"), a.get_editor_property("stairs_up_anim").get_name()))
