# Stage 5E: retarget Unreal's motion-captured Mannequin idle + walk onto the scanned NPCs with the IK Retargeter.
# New assets only: /Game/P9/Characters/Station3D/Retarget/ (IK rigs, retargeters) and <Name>/MC/ (retargeted clips).
# Set NAMES (globals) to limit the characters; default: all seven.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
RT = "/Game/P9/Characters/Station3D/Retarget"
ROOT = "/Game/P9/Characters/Station3D"
NAMES = globals().get("NAMES", ["dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"])
SRC_MESH = el.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
CLIPS = {"Idle": "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle", "Walk": "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"}
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET

def ik_rig(name, mesh):
    p = "%s/%s" % (RT, name)
    if el.does_asset_exist(p): return el.load_asset(p)
    rig = at.create_asset(name, RT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig)
    c.set_skeletal_mesh(mesh)
    c.apply_auto_generated_retarget_definition()
    c.apply_auto_fbik()
    unreal.log("P9PY: rig %s root=%s chains=%s" % (name, c.get_retarget_root(), [str(ch.get_editor_property("chain_name")) if hasattr(ch, "get_editor_property") else str(ch) for ch in c.get_retarget_chains()]))
    el.save_loaded_asset(rig, False)
    return rig

src_rig = ik_rig("IK_Manny", SRC_MESH)
out = []
for n in NAMES:
    mesh = el.load_asset("%s/%s/SK_P9_NPC3D_%s" % (ROOT, n, n))
    rig = ik_rig("IK_NPC3D_" + n, mesh)
    rp = "%s/RTG_Manny_to_%s" % (RT, n)
    if el.does_asset_exist(rp):
        rtg = el.load_asset(rp)
    else:
        rtg = at.create_asset("RTG_Manny_to_" + n, RT, unreal.IKRetargeter, unreal.IKRetargetFactory())
        rc = unreal.IKRetargeterController.get_controller(rtg)
        rc.set_ik_rig(S, src_rig); rc.set_ik_rig(T, rig)
        rc.set_preview_mesh(S, SRC_MESH); rc.set_preview_mesh(T, mesh)
        if rc.get_num_retarget_ops() == 0: rc.add_default_ops()
        try: rc.assign_ik_rig_to_all_ops(T, rig)
        except Exception as e: unreal.log("P9PY: assign_ik_rig_to_all_ops: %s" % e)
        rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
        rc.auto_align_all_bones(T)            # target retarget pose matched to the Mannequin's limb directions
        el.save_loaded_asset(rtg, False)
        unreal.log("P9PY: retargeter %s ops=%d" % (rtg.get_name(), rc.get_num_retarget_ops()))
    dest = "%s/%s/MC" % (ROOT, n)
    for clip, src in CLIPS.items():
        target_path = "%s/SK_P9_NPC3D_%s_MC_%s" % (dest, n, clip)
        if el.does_asset_exist(target_path): out.append("%s exists" % target_path); continue
        ad = unreal.EditorAssetLibrary.find_asset_data(src)
        res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([ad], SRC_MESH, mesh, rtg, "", "", "", "_RT_" + n, False)
        made = [r.get_asset() for r in res] if res else []
        for m in made:
            if isinstance(m, unreal.AnimSequence):
                ok = el.rename_asset(m.get_path_name().split(".")[0], target_path)
                out.append("%s %s (%.2fs)" % (target_path.split("/")[-1], "ok" if ok else "RENAME FAILED " + m.get_path_name(), m.get_play_length()))
                el.save_asset(target_path, False)
unreal.log("P9PY: retarget done: " + " | ".join(out))
