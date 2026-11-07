import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
f1 = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "F1")
f1.set_editor_property("drunk_chance", 1.0)
unreal.log("P9PY: PIE copy F1 drunk_chance=1 (test only)")
