import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "F1")
b = npc.get_editor_property("body")
o, e = npc.get_actor_bounds(False, True)
unreal.log("P9PY: F1 hidden=%s loc=%s bounds o=%s e=%s visible=%s mesh=%s" % (npc.get_editor_property("hidden"), npc.get_actor_location(), o, e, b.is_visible(), b.get_skeletal_mesh_asset().get_name() if b.get_skeletal_mesh_asset() else None))
for s in ("root", "pelvis", "head", "foot_l", "hand_r"):
    unreal.log("P9PY:  %s world %s" % (s, b.get_socket_location(s)))
m = unreal.load_asset("/Game/P9/Characters/Station3D/dhanraj/MQ/SK_P9_NPC3D_dhanraj_MQ")
bb = m.get_bounds(); unreal.log("P9PY: MQ asset bounds origin %s ext %s" % (bb.origin, bb.box_extent))
