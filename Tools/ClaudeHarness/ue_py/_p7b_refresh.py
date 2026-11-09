import unreal
w = unreal.UnrealEditorSubsystem().get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9FriendNPC):
    a.get_editor_property("body").set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES, unreal.PropertyAccessChangeNotifyMode.NEVER)
