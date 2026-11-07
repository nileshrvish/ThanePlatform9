import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
f1 = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "F1")
v = f1.get_editor_property("voice")
lines = f1.get_editor_property("drunk_lines")
unreal.log("P9PY: F1 drunk_chance %.2f lines %d first %s voice playing %s audible %.0f" % (f1.get_editor_property("drunk_chance"), len(lines), lines[0] if lines else None, v.is_playing(), f1.get_editor_property("audible_distance")))
for n in ("SK_P9_NPC3D_dhanraj_MX_DrunkWalk", "SK_P9_NPC3D_dhanraj_MX_Walk", "SK_P9_NPC3D_dhanraj_MX_StairsUp", "SK_P9_NPC3D_dhanraj_MX_SitTalk"):
    a = unreal.load_asset("/Game/P9/Characters/Station3D/dhanraj/MX5G/Anims/" + n)
    unreal.log("P9PY: %s len %.2fs frames %s rate %s" % (n, a.get_play_length(), a.get_editor_property("number_of_sampled_frames") if hasattr(a, "get_editor_property") else "?", a.get_editor_property("rate_scale")))
cam = unreal.GameplayStatics.get_player_camera_manager(world, 0)
unreal.log("P9PY: cam %s npc %s" % (cam.get_camera_location(), f1.get_actor_location()))
