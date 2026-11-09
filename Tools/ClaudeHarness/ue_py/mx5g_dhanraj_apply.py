# Stage 5G (editor; NOT saved; Mixamo clips = MX2 set with correct lengths): F1 (Dhanraj) uses the user's Mixamo rig and clips; drunk legs with breakup mumbles; stairs clip.
# Clip speeds are the Mixamo clips' own speeds x 0.9331 (import scale 1.77/1.897).
import unreal
el = unreal.EditorAssetLibrary; A = "/Game/P9/Characters/Station3D/dhanraj/MX5G/Anims/"
def c(n): 
    x = el.load_asset(A + n)
    if not x: raise RuntimeError("P9PY: missing " + n)
    return x
f1 = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a, unreal.P9FriendNPC) and str(a.get_editor_property("friend_key")) == "F1")
f1.modify()
mesh = el.load_asset("/Game/P9/Characters/Station3D/dhanraj/MX5G/SK_P9_NPC3D_dhanraj_MX")
props = {"friend_mesh": mesh,
         "walk_anim": c("SK_P9_NPC3D_dhanraj_MX2_Walk"), "drunk_walk_anim": c("SK_P9_NPC3D_dhanraj_MX2_DrunkWalk"),
         "stairs_up_anim": c("SK_P9_NPC3D_dhanraj_MX2_StairsUp"), "sit_idle_anim": c("SK_P9_NPC3D_dhanraj_MX2_SitTalk"),
         "idle_anim": c("SK_P9_NPC3D_dhanraj_MX5G_Idle"), "sit_down_anim": c("SK_P9_NPC3D_dhanraj_MX5G_SitDown"),
         "stand_up_anim": c("SK_P9_NPC3D_dhanraj_MX5G_StandUp"), "chat_anim": c("SK_P9_NPC3D_dhanraj_MX5G_Chat"),
         "look_around_anim": c("SK_P9_NPC3D_dhanraj_MX5G_LookAround"), "turn_anim": c("SK_P9_NPC3D_dhanraj_MX5G_TurnInPlace"),
         "walk_speed": 125.0, "walk_clip_speed": round(162.7 * 0.9331, 1), "drunk_walk_speed": round(105.8 * 0.9331, 1),
         "drunk_chance": 0.5, "stairs_up_speed": 60.0, "stairs_up_clip_speed": round(51.1 * 0.9331, 1),
         "drunk_lines": ["/Game/P9/Audio/NPC/SW_P9_NPC_F1_Drunk_%d" % i for i in range(1, 6)]}
for k, v in props.items():
    if k == "drunk_lines":
        snd = [el.load_asset(p) for p in v]
        if not all(snd): raise RuntimeError("P9PY: missing drunk line asset")
        f1.set_editor_property(k, snd)
    else:
        f1.set_editor_property(k, v)
f1.get_editor_property("body").set_skeletal_mesh_asset(mesh)
unreal.log("P9PY: F1 Dhanraj -> Mixamo: walk %.1f (clip %.1f) drunk %.1f chance %.2f stairs %.1f (clip %.1f) lines %d" % (
    f1.get_editor_property("walk_speed"), f1.get_editor_property("walk_clip_speed"), f1.get_editor_property("drunk_walk_speed"),
    f1.get_editor_property("drunk_chance"), f1.get_editor_property("stairs_up_speed"), f1.get_editor_property("stairs_up_clip_speed"), len([x for x in f1.get_editor_property("drunk_lines") if x])))
unreal.log("P9PY: clip lengths walk %.2f drunk %.2f stairs %.2f sit %.2f; first line %s" % tuple([f1.get_editor_property(n).get_play_length() for n in ("walk_anim", "drunk_walk_anim", "stairs_up_anim", "sit_idle_anim")] + [f1.get_editor_property("drunk_lines")[0]]))
