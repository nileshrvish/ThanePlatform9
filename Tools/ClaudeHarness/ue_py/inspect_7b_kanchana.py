# Stage 7B (read-only): Kanchana's stalker instance settings, her MX7A assets and retarget assets; friend capsule sizes from
# the reference pose (editor world bone transforms are the reference pose).
import unreal
el = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.P9SilhouetteStalker):
        out = []
        for p in ("stalker_mesh", "idle_anim", "walk_anim", "menace_anim", "talk_anim", "walk_clip_speed", "follow_distance"):
            v = a.get_editor_property(p)
            out.append("%s=%s" % (p, v.get_path_name() if hasattr(v, "get_path_name") else v))
        unreal.log("P9PY: stalker %s: %s" % (a.get_actor_label(), "; ".join(out)))
for p in el.list_assets("/Game/P9/Characters/Station3D/kanchana/MX7A", recursive=True, include_folder=False):
    unreal.log("P9PY: asset " + p)
for p in el.list_assets("/Game/P9/Characters/Station3D/Retarget", recursive=False, include_folder=False):
    if "MX7A_kanchana" in p or "MX5G" in p or "dhanraj" in p.lower(): unreal.log("P9PY: rtg " + p)
for p in el.list_assets("/Game/P9/Characters/Station3D/kanchana/MQ2", recursive=True, include_folder=False):
    unreal.log("P9PY: mq2 " + p)
for p in el.list_assets("/Game/P9/Characters/Station3D/dhanraj/MX5G", recursive=True, include_folder=False):
    unreal.log("P9PY: dh " + p)
# body sizes from the reference pose
for a in eas.get_all_level_actors():
    if isinstance(a, (unreal.P9FriendNPC, unreal.P9SilhouetteStalker)):
        b = a.get_editor_property("body")
        m = b.get_skeletal_mesh_asset()
        names = [str(n) for n in b.get_all_socket_names()]
        def W(n):
            for c in (n, "mixamorig:" + n):
                if c in names: return b.get_socket_transform(c, unreal.RelativeTransformSpace.RTS_WORLD).translation
            return None
        la, ra, hd, lu, ru, sp = W("LeftArm"), W("RightArm"), W("Head"), W("LeftUpLeg"), W("RightUpLeg"), W("Spine2")
        sh = (la - ra).length() if la and ra else -1
        hp = (lu - ru).length() if lu and ru else -1
        o, e = a.get_actor_bounds(False)
        unreal.log("P9PY: body %s mesh %s shoulders %.1f hips %.1f head z %.1f (actor z %.1f) bounds ext %s" % (a.get_actor_label(), m.get_name() if m else None, sh, hp,
                   hd.z if hd else -1, a.get_actor_location().z, [round(v) for v in (e.x, e.y, e.z)]))
