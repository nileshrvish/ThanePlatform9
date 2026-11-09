# Stage 7A (editor; map NOT saved): F2-F5 and the chaiwala use their Mixamo rigs and clips (MX7A), like F1 Dhanraj in 5G
# (mx5g_dhanraj_apply.py). Only meshes, clips and clip speeds change; routes, roles, dialogue and audio are left alone.
# Clip speeds = the Mixamo clips' own speeds x the import scale (stairs: along the slope). WalkSpeed moves towards the clip's
# natural pace by at most 15 % of the old value (routines keep their timing). Kanchana (stalker) is not touched (protected).
import unreal
el = unreal.EditorAssetLibrary
R = "/Game/P9/Characters/Station3D/"
# key: name, walk clip speed, stairs clip speed (cm/s, Unreal)
NPC = {"F2": ("Nilesh", 140.1, 43.9), "F3": ("shivshankar", 142.8, 44.8), "F4": ("Rohit", 92.5, 43.3), "F5": ("rupesh", 103.4, 48.4),
       "CW": ("Modi", 88.2, 41.3)}
def clip(n, c):
    x = el.load_asset("%s%s/MX7A/Anims/SK_P9_NPC3D_%s_MX7A_%s" % (R, n, n, c))
    if not x: raise RuntimeError("P9PY: missing %s %s" % (n, c))
    return x
acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in acts:
    if not isinstance(a, unreal.P9FriendNPC): continue
    key = str(a.get_editor_property("friend_key"))
    if key not in NPC: continue
    n, walk_clip, stairs_clip = NPC[key]
    mesh = el.load_asset("%s%s/MX7A/SK_P9_NPC3D_%s_MX" % (R, n, n))
    a.modify()
    old = a.get_editor_property("walk_speed")
    props = {"friend_mesh": mesh, "idle_anim": clip(n, "Idle"), "chat_anim": clip(n, "Chat") if n != "Modi" else clip(n, "Chat"),
             "look_around_anim": clip(n, "LookAround"), "turn_anim": clip(n, "TurnInPlace"), "walk_anim": clip(n, "Walk"),
             "stairs_up_anim": clip(n, "StairsUp"), "walk_clip_speed": walk_clip, "stairs_up_clip_speed": stairs_clip,
             "walk_speed": round(min(max(walk_clip, old * 0.85), old * 1.15), 1)}
    if n == "Modi":
        props.update({"gesture_anim": clip(n, "Gesture"), "pour_anim": clip(n, "Pour"),
                      "held_socket_right": "mixamorig:RightHand", "held_socket_left": "mixamorig:LeftHand"})
    else:
        props.update({"sit_down_anim": clip(n, "SitDown"), "sit_idle_anim": clip(n, "SitIdle"), "stand_up_anim": clip(n, "StandUp")})
    for k, v in props.items():
        a.set_editor_property(k, v)
    a.get_editor_property("body").set_skeletal_mesh_asset(mesh)
    unreal.log("P9PY: %s (%s) -> MX7A: walk %.1f (was %.1f, clip %.1f, rate %.2f) stairs %.1f (clip %.1f) clips %s" % (
        a.get_actor_label(), n, a.get_editor_property("walk_speed"), old, walk_clip, a.get_editor_property("walk_speed") / walk_clip,
        a.get_editor_property("stairs_up_speed"), stairs_clip, sorted(k for k in props if k.endswith("_anim"))))
