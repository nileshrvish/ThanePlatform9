# Stage 5E: retargeted mocap holds the arms as close as the slim Mannequin's; on the wider scans they sink into the
# torso. Build v2 retargeters with the upper arms tilted outward in the TARGET retarget pose (added on top of the
# auto-alignment) and retarget idle + walk into <Name>/MX2/. SPLAY: degrees per character; AXIS sign set by SIGN.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
RT = "/Game/P9/Characters/Station3D/Retarget"; ROOT = "/Game/P9/Characters/Station3D"
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
SPLAY = globals().get("SPLAY", {"Modi": 12.0})
AXIS = globals().get("AXIS", "X"); SIGN = globals().get("SIGN", 1.0); TAG = globals().get("TAG", "MX2")
src_mesh = el.load_asset("/Game/P9/Characters/MannyX/SKM_MannyX"); src_rig = el.load_asset(RT + "/IK_MannyX")
CLIPS = {"Idle": "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Idle", "Walk": "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Walk"}
def axis_quat(axis, deg):
    r = {"X": unreal.Rotator(roll=deg, pitch=0.0, yaw=0.0), "Y": unreal.Rotator(roll=0.0, pitch=deg, yaw=0.0), "Z": unreal.Rotator(roll=0.0, pitch=0.0, yaw=deg)}[axis]
    return r.quaternion()
out = []
for n, deg in SPLAY.items():
    mesh = el.load_asset("%s/%s/SK_P9_NPC3D_%s" % (ROOT, n, n)); rig = el.load_asset("%s/IK_NPC3D_%s" % (RT, n))
    name = "RTG_MannyX_to_%s_%s" % (n, TAG)
    # reuse a same-named test retargeter (an interrupted run leaves one behind); it is fully reconfigured below
    rtg = el.load_asset(RT + "/" + name) if el.does_asset_exist(RT + "/" + name) else at.create_asset(name, RT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, src_rig); rc.set_ik_rig(T, rig); rc.set_preview_mesh(S, src_mesh); rc.set_preview_mesh(T, mesh)
    if rc.get_num_retarget_ops() == 0: rc.add_default_ops()
    try: rc.assign_ik_rig_to_all_ops(T, rig)
    except Exception: pass
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    for side, s in (("l", 1.0), ("r", -1.0)):
        b = "upperarm_" + side
        cur = rc.get_rotation_offset_for_retarget_pose_bone(b, T)
        add = axis_quat(AXIS, SIGN * s * deg)
        rc.set_rotation_offset_for_retarget_pose_bone(b, add * cur, T)
    el.save_loaded_asset(rtg, False)
    for clip, src in CLIPS.items():
        dst = "%s/%s/%s/SK_P9_NPC3D_%s_%s_%s" % (ROOT, n, TAG, n, TAG, clip)
        if el.does_asset_exist(dst): raise RuntimeError("exists " + dst)
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([el.find_asset_data(src)], src_mesh, mesh, rtg, "", "", "", "_RT_%s_%s" % (n, TAG), False)
        for r in res or []:
            m = r.get_asset()
            if isinstance(m, unreal.AnimSequence):
                el.rename_asset(m.get_path_name().split(".")[0], dst); el.save_asset(dst, False); out.append(dst.split("/")[-1])
unreal.log("P9PY: armsplay %s axis %s sign %+.0f: %s" % (TAG, AXIS, SIGN, ", ".join(out)))
