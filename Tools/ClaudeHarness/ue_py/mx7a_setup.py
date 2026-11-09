# Stage 7A: the Stage 5G Dhanraj workflow (mx5g_dhanraj_setup.py) for the six remaining characters (MX7A). New assets only;
# Dhanraj's MX5G assets, the shared LODS_P9_NPC3D and the map are not touched.
#  - mesh: existing MI_P9_NPC3D_<n> (one slot), LOD settings LODS_P9_NPC3D_MX7A (copy of LODS_P9_NPC3D + finger bones removed
#    from LOD1/LOD2), 3 LODs regenerated
#  - IK rig + retargeters (MannyX -> MX7A, MQ2 -> MX7A); clips Mixamo did not supply are retargeted:
#    Idle (Unreal mocap via MannyX), SitDown/StandUp (MQ2S), Chat/LookAround/TurnInPlace (MQ2), Gesture/Pour (chaiwala, MQ2)
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Characters/Station3D"; RT = ROOT + "/Retarget"
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
NAMES = globals().get("NAMES", ["Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"])
FINGERS = ["mixamorig:%sHand%s1" % (s, f) for s in ("Left", "Right") for f in ("Thumb", "Index", "Middle", "Ring", "Pinky")]

# ---- LOD settings: a copy of the shared asset (Dhanraj keeps the original), fingers dropped from LOD1+
LODS = ROOT + "/LODS_P9_NPC3D_MX7A"
def bone_ref(name):
    r = unreal.BoneFilter(); r.set_editor_property("bone_name", name); r.set_editor_property("exclude_self", False); return r
lods = el.load_asset(LODS) if el.does_asset_exist(LODS) else el.duplicate_asset(ROOT + "/LODS_P9_NPC3D", LODS)
groups = list(lods.get_editor_property("lod_groups"))
if any(i >= 1 and not g.get_editor_property("bone_list") for i, g in enumerate(groups)):
    for i, g in enumerate(groups):
        if i >= 1:
            g.set_editor_property("bone_filter_action_option", unreal.BoneFilterActionOption.REMOVE)
            g.set_editor_property("bone_list", [bone_ref(b) for b in FINGERS])
    lods.set_editor_property("lod_groups", groups)
    el.save_loaded_asset(lods, False)
unreal.log("P9PY: LOD settings %s: %s" % (LODS, [(round(g.get_editor_property("screen_size").default, 3), len(g.get_editor_property("bone_list"))) for g in lods.get_editor_property("lod_groups")]))

def ik_rig(name, m):
    p = "%s/%s" % (RT, name)
    if el.does_asset_exist(p): return el.load_asset(p)
    rig = at.create_asset(name, RT, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    c = unreal.IKRigController.get_controller(rig); c.set_skeletal_mesh(m)
    c.apply_auto_generated_retarget_definition(); c.apply_auto_fbik()
    el.save_loaded_asset(rig, False)
    unreal.log("P9PY: rig %s chains %s" % (name, [str(x) for x in c.get_retarget_chains()][:14]))
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
    if not el.does_asset_exist(src): return "NO SOURCE"
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([el.find_asset_data(src)], sm, dm, rtg, "", "", "", "_tmpRT", False)
    for r in res or []:
        m = r.get_asset()
        if isinstance(m, unreal.AnimSequence):
            el.rename_asset(m.get_path_name().split(".")[0], dst); el.save_asset(dst, False); return "%.2fs" % m.get_play_length()
    return "FAILED"

mannyx = el.load_asset("/Game/P9/Characters/MannyX/SKM_MannyX")
for n in NAMES:
    D = "%s/%s/MX7A" % (ROOT, n)
    mesh = el.load_asset("%s/SK_P9_NPC3D_%s_MX" % (D, n))
    mi = el.load_asset("%s/Materials/MI_P9_NPC3D_%s" % (ROOT, n))
    if mesh.get_editor_property("lod_settings") != lods:
        mesh.modify()
        slot = mesh.materials[0].material_slot_name
        mesh.set_editor_property("materials", [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=slot)])
        mesh.set_editor_property("lod_settings", lods)
        ss.regenerate_lod(mesh, 3, True, False)
        el.save_loaded_asset(mesh, False)
    sk = mesh.get_editor_property("skeleton")
    b = mesh.get_bounds()
    verts = [ss.get_num_verts(mesh, l) for l in range(ss.get_lod_count(mesh))]
    unreal.log("P9PY: %s mesh height %.1f cm, LODs %d verts %s, slots %d (%s)" % (n, b.box_extent.z * 2, ss.get_lod_count(mesh), verts, len(mesh.materials),
               mesh.materials[0].material_interface.get_name()))
    rig = ik_rig("IK_NPC3D_MX7A_" + n, mesh)
    r_mx = retargeter("RTG_MannyX_to_MX7A_" + n, el.load_asset(RT + "/IK_MannyX"), mannyx, rig, mesh)
    out = []
    if n != "kanchana":                                    # Kanchana: Mixamo idle supplied; her stalker is not changed (protected)
        out.append("Idle " + run(r_mx, mannyx, mesh, "/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Idle", "%s/Anims/SK_P9_NPC3D_%s_MX7A_Idle" % (D, n)))
        mq2 = el.load_asset("%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2" % (ROOT, n, n))
        r_mq = retargeter("RTG_MQ2_to_MX7A_" + n, el.load_asset("%s/IK_NPC3D_MQ2_%s" % (RT, n)), mq2, rig, mesh)
        clips = [("LookAround", "MQ2"), ("TurnInPlace", "MQ2")]
        if n != "Modi":
            clips += [("SitDown", "MQ2S"), ("StandUp", "MQ2S"), ("Chat", "MQ2")]
        else:
            clips += [("Gesture", "MQ2"), ("Pour", "MQ2")]
        for clip, tag in clips:
            out.append("%s %s" % (clip, run(r_mq, mq2, mesh, "%s/%s/%s/SK_P9_NPC3D_%s_%s_%s" % (ROOT, n, tag, n, tag, clip), "%s/Anims/SK_P9_NPC3D_%s_MX7A_%s" % (D, n, clip))))
    unreal.log("P9PY: %s skeleton %d bones; retargets: %s" % (n, len(sk.get_editor_property("bone_tree")) if hasattr(sk, "get_editor_property") else -1, ", ".join(out)))
