# Stage 5F: set up the Mannequin-rigged NPC meshes (MQ) and give them their clips, all made in Unreal:
#  - Unreal mocap idle + walk: retargeted MannyX -> MQ (same skeleton layout, both face +X)
#  - clips Unreal does not ship (sit, chat, look, turn, gestures, pour, menace): moved from the old 23-bone rig
#    (sit from the A4 set with the hands resting on the thighs; the rest A3) onto the MQ skeleton
# (MQ2 = fixed skeleton scale; MQ v1 had a 0.0001-scale skeleton and is unused.) New assets only, in Station3D/<n>/MQ2/ and Station3D/Retarget/. NAMES may be set by a wrapper.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Characters/Station3D"; RT = ROOT + "/Retarget"
NAMES = globals().get("NAMES", ["dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"])
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
mannyx = el.load_asset("/Game/P9/Characters/MannyX/SKM_MannyX"); rig_mx = el.load_asset(RT + "/IK_MannyX")
MOCAP = {"Idle": "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Idle", "Walk": "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Walk"}
FRIEND_OLD = {"SitDown": "A4", "SitIdle": "A4", "StandUp": "A4", "Chat": "A4", "LookAround": "A4", "TurnInPlace": "A4"}
ROLE_OLD = {"Modi": {"Chat": "A3", "LookAround": "A3", "TurnInPlace": "A3", "Gesture": "A3", "Pour": "A3"},
            "kanchana": {"TurnInPlace": "A3", "Menace": "A3", "TalkAngry": "A3"}}
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lods = el.load_asset(ROOT + "/LODS_P9_NPC3D")

def ik_rig(name, mesh):
    p = "%s/%s" % (RT, name)
    if el.does_asset_exist(p): return el.load_asset(p)
    rig = at.create_asset(name, RT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig); c.set_skeletal_mesh(mesh)
    c.apply_auto_generated_retarget_definition(); c.apply_auto_fbik()
    el.save_loaded_asset(rig, False)
    return rig
def retargeter(name, src_rig, src_mesh, dst_rig, dst_mesh):
    p = "%s/%s" % (RT, name)
    if el.does_asset_exist(p): return el.load_asset(p)
    r = at.create_asset(name, RT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(r)
    rc.set_ik_rig(S, src_rig); rc.set_ik_rig(T, dst_rig); rc.set_preview_mesh(S, src_mesh); rc.set_preview_mesh(T, dst_mesh)
    if rc.get_num_retarget_ops() == 0: rc.add_default_ops()
    try: rc.assign_ik_rig_to_all_ops(T, dst_rig)
    except Exception: pass
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    el.save_loaded_asset(r, False)
    return r
def run(rtg, src_mesh, dst_mesh, src_path, dst_path):
    if el.does_asset_exist(dst_path): return "exists"
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([el.find_asset_data(src_path)], src_mesh, dst_mesh, rtg, "", "", "", "_tmpRT", False)
    for r in res or []:
        m = r.get_asset()
        if isinstance(m, unreal.AnimSequence):
            el.rename_asset(m.get_path_name().split(".")[0], dst_path); el.save_asset(dst_path, False)
            return "%.2fs" % m.get_play_length()
    return "FAILED"

report = []
for n in NAMES:
    mq = el.load_asset("%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2" % (ROOT, n, n))
    old = el.load_asset("%s/%s/SK_P9_NPC3D_%s" % (ROOT, n, n))
    mi = el.load_asset("%s/Materials/MI_P9_NPC3D_%s" % (ROOT, n))
    mq.modify()
    slot = mq.materials[0].material_slot_name
    mq.set_editor_property("materials", [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=slot)])
    mq.set_editor_property("lod_settings", lods); ss.regenerate_lod(mq, 3, True, False)
    el.save_loaded_asset(mq, False)
    rig_mq = ik_rig("IK_NPC3D_MQ2_" + n, mq)
    rig_old = el.load_asset("%s/IK_NPC3D_%s" % (RT, n))
    rtg_mx = retargeter("RTG_MannyX_to_MQ2_" + n, rig_mx, mannyx, rig_mq, mq)
    rtg_old = retargeter("RTG_Old_to_MQ2_" + n, rig_old, old, rig_mq, mq)
    done = []
    for clip, src in MOCAP.items():
        done.append("%s %s" % (clip, run(rtg_mx, mannyx, mq, src, "%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2_%s" % (ROOT, n, n, clip))))
    for clip, tag in ROLE_OLD.get(n, FRIEND_OLD).items():
        src = "%s/%s/%s/SK_P9_NPC3D_%s_%s_%s" % (ROOT, n, tag, n, tag, clip)
        done.append("%s %s" % (clip, run(rtg_old, old, mq, src, "%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2_%s" % (ROOT, n, n, clip))))
    report.append("%s lods=%d chains=%d [%s]" % (n, ss.get_lod_count(mq), len(unreal.IKRigController.get_controller(rig_mq).get_retarget_chains()), ", ".join(done)))
for r in report: unreal.log("P9PY: mq " + r)
