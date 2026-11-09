# Stage 5E test: the Mannequin faces +Y, the scanned NPCs face +X. Build two test retargeters for dhanraj with a 90-degree
# retarget-pose yaw on the pelvis (A: source -90, B: target +90), retarget the walk with each. New test assets only.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
RT = "/Game/P9/Characters/Station3D/Retarget"; n = "dhanraj"
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
src_mesh = el.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
mesh = el.load_asset("/Game/P9/Characters/Station3D/%s/SK_P9_NPC3D_%s" % (n, n))
src_rig = el.load_asset(RT + "/IK_Manny"); rig = el.load_asset(RT + "/IK_NPC3D_" + n)
walk = el.find_asset_data("/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd")
for tag, side, yaw in (("tA", S, -90.0), ("tB", T, 90.0)):
    name = "RTG_Manny_to_%s_%s" % (n, tag)
    if el.does_asset_exist(RT + "/" + name): raise RuntimeError("exists " + name)
    rtg = at.create_asset(name, RT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, src_rig); rc.set_ik_rig(T, rig); rc.set_preview_mesh(S, src_mesh); rc.set_preview_mesh(T, mesh)
    if rc.get_num_retarget_ops() == 0: rc.add_default_ops()
    try: rc.assign_ik_rig_to_all_ops(T, rig)
    except Exception: pass
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    q = unreal.Rotator(0.0, 0.0, yaw).quaternion()
    rc.set_rotation_offset_for_retarget_pose_bone("pelvis", q, side)
    if side == S: rc.auto_align_all_bones(T)
    el.save_loaded_asset(rtg, False)
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([walk], src_mesh, mesh, rtg, "", "", "", "_RT_%s_%s" % (n, tag), False)
    for r in res or []:
        m = r.get_asset()
        if isinstance(m, unreal.AnimSequence):
            dst = "/Game/P9/Characters/Station3D/%s/MC/SK_P9_NPC3D_%s_MC_Walk_%s" % (n, n, tag)
            el.rename_asset(m.get_path_name().split(".")[0], dst); el.save_asset(dst, False)
            unreal.log("P9PY: facing test %s -> %s" % (tag, dst))
