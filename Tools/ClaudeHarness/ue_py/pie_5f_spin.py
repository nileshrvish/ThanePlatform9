import unreal
CLIP = globals().get("CLIP", "Walk"); N = globals().get("N", "dhanraj")
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "F1")
b = npc.get_editor_property("body")
out = []
an = unreal.load_asset("/Game/P9/Characters/Station3D/%s/MQ2/SK_P9_NPC3D_%s_MQ2_%s" % (N, N, CLIP)); L = an.get_play_length()
for i in range(8):
    t = L * i / 8.0
    p = unreal.AnimationLibrary.get_bone_pose_for_time(an, "pelvis", t, False) if hasattr(unreal.AnimationLibrary, "get_bone_pose_for_time") else None
    r = unreal.AnimationLibrary.get_bone_pose_for_time(an, "root", t, False) if p else None
    out.append("t%.2f pelvis rot %s loc %s | root rot %s" % (t, p.rotation.rotator() if p else "?", p.translation if p else "?", r.rotation.rotator() if r else "?"))
for o in out: unreal.log("P9PY: " + o)
