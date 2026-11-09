import unreal
w = unreal.UnrealEditorSubsystem().get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9FriendNPC):
    b = a.get_editor_property("body")
    ai = b.get_anim_instance()
    unreal.log("P9PY: %s tick=%s actorTick=%s vis=%s mode=%s anim=%s pause=%s rate=%s" % (a.get_editor_property("friend_key"), b.is_component_tick_enabled(), a.is_actor_tick_enabled(), b.get_editor_property("visibility_based_anim_tick_option"), b.get_animation_mode(), ai.get_class().get_name() if ai else None, b.get_editor_property("pause_anims") if True else 0, b.get_editor_property("global_anim_rate_scale")))
