import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
cw = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "CW")
t = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9Train) if a.get_name() == "P9Train_4")
l = t.get_actor_transform().inverse_transform_location(cw.get_actor_location())
b = cw.get_editor_property("body")
unreal.log("P9PY: cwlocal %.1f %.1f t=%.2f clip=%s" % (l.x, l.y, unreal.GameplayStatics.get_time_seconds(world), b.get_anim_instance().get_class().get_name() if b.get_anim_instance() else "-"))
