# Stage 5G: Dhanraj on the user's Mixamo rig (MX5G). Mixamo clips: Walk, DrunkWalk, StairsUp, SitTalk (seated idle).
# Clips Mixamo did not supply are retargeted in Unreal: Idle (Unreal mocap, MannyX), SitDown/StandUp/Chat/LookAround/
# TurnInPlace (from his Mannequin rig, MQ2/MQ2S). New assets only. Map is NOT changed here.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Characters/Station3D"; RT = ROOT + "/Retarget"; D = ROOT + "/dhanraj/MX5G"
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
mesh = el.load_asset(D + "/SK_P9_NPC3D_dhanraj_MX")
mesh.modify()
slot = mesh.materials[0].material_slot_name
mesh.set_editor_property("materials", [unreal.SkeletalMaterial(material_interface=el.load_asset(ROOT + "/Materials/MI_P9_NPC3D_dhanraj"), material_slot_name=slot)])
mesh.set_editor_property("lod_settings", el.load_asset(ROOT + "/LODS_P9_NPC3D"))
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem); ss.regenerate_lod(mesh, 3, True, False)
el.save_loaded_asset(mesh, False)
b = mesh.get_bounds()
unreal.log("P9PY: MX5G mesh bounds ext %s origin %s lods %d" % (b.box_extent, b.origin, ss.get_lod_count(mesh)))
def ik_rig(name, m):
    p = "%s/%s" % (RT, name)
    if el.does_asset_exist(p): return el.load_asset(p)
    rig = at.create_asset(name, RT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig); c.set_skeletal_mesh(m)
    c.apply_auto_generated_retarget_definition(); c.apply_auto_fbik()
    el.save_loaded_asset(rig, False)
    unreal.log("P9PY: rig %s chains %s" % (name, [str(x) for x in c.get_retarget_chains()][:12]))
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
def run(rtg, sm, dm, src, dst):
    if el.does_asset_exist(dst): return "exists"
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([el.find_asset_data(src)], sm, dm, rtg, "", "", "", "_tmpRT", False)
    for r in res or []:
        m = r.get_asset()
        if isinstance(m, unreal.AnimSequence):
            el.rename_asset(m.get_path_name().split(".")[0], dst); el.save_asset(dst, False); return "%.2fs" % m.get_play_length()
    return "FAILED"
rig = ik_rig("IK_NPC3D_MX5G_dhanraj", mesh)
mannyx = el.load_asset("/Game/P9/Characters/MannyX/SKM_MannyX"); mq2 = el.load_asset(ROOT + "/dhanraj/MQ2/SK_P9_NPC3D_dhanraj_MQ2")
r_mx = retargeter("RTG_MannyX_to_MX5G_dhanraj", el.load_asset(RT + "/IK_MannyX"), mannyx, rig, mesh)
r_mq = retargeter("RTG_MQ2_to_MX5G_dhanraj", el.load_asset(RT + "/IK_NPC3D_MQ2_dhanraj"), mq2, rig, mesh)
out = ["Idle " + run(r_mx, mannyx, mesh, "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Idle", D + "/Anims/SK_P9_NPC3D_dhanraj_MX5G_Idle")]
for clip, src in (("SitDown", "MQ2S/SK_P9_NPC3D_dhanraj_MQ2S_SitDown"), ("StandUp", "MQ2S/SK_P9_NPC3D_dhanraj_MQ2S_StandUp"),
                  ("Chat", "MQ2/SK_P9_NPC3D_dhanraj_MQ2_Chat"), ("LookAround", "MQ2/SK_P9_NPC3D_dhanraj_MQ2_LookAround"), ("TurnInPlace", "MQ2/SK_P9_NPC3D_dhanraj_MQ2_TurnInPlace")):
    out.append("%s %s" % (clip, run(r_mq, mq2, mesh, "%s/dhanraj/%s" % (ROOT, src), D + "/Anims/SK_P9_NPC3D_dhanraj_MX5G_" + clip)))
unreal.log("P9PY: MX5G retargets: " + ", ".join(out))
