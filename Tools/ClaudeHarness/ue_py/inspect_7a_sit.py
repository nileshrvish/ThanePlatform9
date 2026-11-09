# Stage 7A (read-only): where the pelvis/hips sits (component space, cm) in the seated clips that already work, per character.
import unreal

AL = unreal.AnimationLibrary
R = "/Game/P9/Characters/Station3D/"
def chain_pos(anim, bone, t):
    """Component-space position of a bone at time t (composes parents via the skeleton's reference hierarchy)."""
    skel = anim.get_editor_property("skeleton")
    names = []
    b = bone
    ref = unreal.SkeletalMeshEditorSubsystem
    while b and str(b) != "None":
        names.append(b)
        b = AL.find_bone_path_to_root(anim, b)[1] if len(AL.find_bone_path_to_root(anim, b)) > 1 else None
    path = list(AL.find_bone_path_to_root(anim, bone))       # bone ... root
    poses = AL.get_bone_poses_for_time(anim, path, t, False)
    xf = unreal.Transform()
    for p in reversed(poses):
        xf = p.multiply(xf)
    return xf.translation, [str(x) for x in path]
for n in ["dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"]:
    a = unreal.load_asset(R + "%s/MQ2S/SK_P9_NPC3D_%s_MQ2S_SitIdle" % (n, n))
    if a:
        p, path = chain_pos(a, "pelvis", 0.5)
        f, _ = chain_pos(a, "foot_l", 0.5)
        unreal.log("P9PY: %-12s MQ2S SitIdle pelvis %s foot_l %s (path %s)" % (n, p, f, path))
a = unreal.load_asset(R + "dhanraj/MX5G/Anims/SK_P9_NPC3D_dhanraj_MX2_SitTalk")
for t in (0.5, 10.0):
    p, path = chain_pos(a, "mixamorig:Hips", t)
    f, _ = chain_pos(a, "mixamorig:LeftFoot", t)
    unreal.log("P9PY: dhanraj MX2_SitTalk t=%.1f hips %s foot %s path %s" % (t, p, f, path))
w = unreal.load_asset(R + "dhanraj/MX5G/Anims/SK_P9_NPC3D_dhanraj_MX2_Walk")
p, path = chain_pos(w, "mixamorig:Hips", 0.0)
unreal.log("P9PY: dhanraj MX2_Walk hips %s" % p)
