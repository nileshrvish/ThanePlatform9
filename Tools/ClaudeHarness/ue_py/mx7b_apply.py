# Stage 7B (editor; map NOT saved): instance properties only.
#  - all six station NPCs (F1 Dhanraj included): fitted stair clips MX7B StairsUp/StairsDown, bStairClipsFitted, StairsDownSpeed,
#    body capsule sizes (radius from the shoulder width, half height = height / 2). Meshes, walk/idle/sit clips, routes, roles,
#    dialogue and props are not touched.
#  - Kanchana (S5B_Silhouette): her Stage 7A Mixamo mesh and clips (Idle, Walk from MX7A; Menace, TalkAngry, StairsUp/Down
#    from MX7B), WalkAnimSpeed = her Mixamo walk's own speed (WalkClipSpeed - her pace - unchanged), body size.
import unreal
el = unreal.EditorAssetLibrary
R = "/Game/P9/Characters/Station3D/"
def A(n, tag, clip):
    p = "%s%s/%s/Anims/SK_P9_NPC3D_%s_%s_%s" % (R, n, tag, n, tag, clip)
    x = el.load_asset(p)
    if not x: raise RuntimeError("P9PY: missing " + p)
    return x
# key: name, body radius, half height
NPC = {"F1": ("dhanraj", 27.0, 88.5), "F2": ("Nilesh", 28.0, 86.0), "F3": ("shivshankar", 27.0, 84.5), "F4": ("Rohit", 25.0, 87.0),
       "F5": ("rupesh", 28.0, 90.0), "CW": ("Modi", 28.0, 85.0)}
out = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        key = str(a.get_editor_property("friend_key"))
        if key not in NPC: continue
        n, rad, hh = NPC[key]
        a.modify()
        for k, v in {"stairs_up_anim": A(n, "MX7B", "StairsUp"), "stairs_down_anim": A(n, "MX7B", "StairsDown"), "stair_clips_fitted": True,
                     "stairs_down_speed": 66.7, "body_radius": rad, "body_half_height": hh, "avoid_others": True,
                     "idle_anim": A(n, "MX7B", "Idle")}.items():          # the user's new Mixamo idle (Dhanraj included, user choice)
            a.set_editor_property(k, v)
        # capsule size/collision are applied by ApplyCollision (OnConstruction / BeginPlay)
        cap = a.get_editor_property("capsule")
        out.append("%s(%s) idle=%s up=%s down=%s up%.0f/down%.0f cm/s capsule r%.0f hh%.0f %s" % (a.get_actor_label(), n, a.get_editor_property("idle_anim").get_name(), a.get_editor_property("stairs_up_anim").get_name(),
                   a.get_editor_property("stairs_down_anim").get_name(), a.get_editor_property("stairs_up_speed"), a.get_editor_property("stairs_down_speed"),
                   a.get_editor_property("body_radius"), a.get_editor_property("body_half_height"), "props"))
    elif isinstance(a, unreal.P9SilhouetteStalker):
        n = "kanchana"
        a.modify()
        for k, v in {"stalker_mesh": el.load_asset(R + "kanchana/MX7A/SK_P9_NPC3D_kanchana_MX"), "idle_anim": A(n, "MX7A", "Idle"),
                     "walk_anim": A(n, "MX7A", "Walk"), "menace_anim": A(n, "MX7B", "Menace"), "talk_anim": A(n, "MX7B", "TalkAngry"),
                     "stairs_up_anim": A(n, "MX7B", "StairsUp"), "stairs_down_anim": A(n, "MX7B", "StairsDown"),
                     "walk_anim_speed": 129.5, "body_radius": 25.0, "body_half_height": 82.0}.items():
            a.set_editor_property(k, v)
        b = a.get_editor_property("body")
        b.set_skeletal_mesh_asset(a.get_editor_property("stalker_mesh"))
        out.append("%s: mesh %s idle %s walk %s menace %s talk %s stairs %s/%s walkAnimSpeed %.1f pace(WalkClipSpeed) %.1f body rel %s rot %s" % (
            a.get_actor_label(), a.get_editor_property("stalker_mesh").get_name(), a.get_editor_property("idle_anim").get_name(), a.get_editor_property("walk_anim").get_name(),
            a.get_editor_property("menace_anim").get_name(), a.get_editor_property("talk_anim").get_name(), a.get_editor_property("stairs_up_anim").get_name(),
            a.get_editor_property("stairs_down_anim").get_name(), a.get_editor_property("walk_anim_speed"), a.get_editor_property("walk_clip_speed"),
            b.get_editor_property("relative_location"), b.get_editor_property("relative_rotation")))
unreal.log("P9PY: 7B apply:\n" + "\n".join(out))
