# Stage 7B: Kanchana's MX7A rig gets the two stalker clips Mixamo did not supply - Menace and TalkAngry - retargeted from her
# MQ2 clips (RTG_MQ2_to_MX7A_kanchana, same recipe as mx7a_setup.py). New assets only; nothing existing is changed.
import unreal
el = unreal.EditorAssetLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Characters/Station3D"; RT = ROOT + "/Retarget"
S, T = unreal.RetargetSourceOrTarget.SOURCE, unreal.RetargetSourceOrTarget.TARGET
n = "kanchana"
mesh = el.load_asset("%s/%s/MX7A/SK_P9_NPC3D_%s_MX" % (ROOT, n, n))
mq2 = el.load_asset("%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2" % (ROOT, n, n))
src_rig = el.load_asset("%s/IK_NPC3D_MQ2_%s" % (RT, n)); dst_rig = el.load_asset("%s/IK_NPC3D_MX7A_%s" % (RT, n))
unreal.log("P9PY: rigs %s %s" % (src_rig, dst_rig))
name = "RTG_MQ2_to_MX7A_" + n
if el.does_asset_exist("%s/%s" % (RT, name)):
    rtg = el.load_asset("%s/%s" % (RT, name))
else:
    rtg = at.create_asset(name, RT, unreal.IKRetargeter, unreal.IKRetargetFactory())
    rc = unreal.IKRetargeterController.get_controller(rtg)
    rc.set_ik_rig(S, src_rig); rc.set_ik_rig(T, dst_rig); rc.set_preview_mesh(S, mq2); rc.set_preview_mesh(T, mesh)
    if rc.get_num_retarget_ops() == 0: rc.add_default_ops()
    try: rc.assign_ik_rig_to_all_ops(T, dst_rig)
    except Exception: pass
    rc.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    rc.auto_align_all_bones(T)
    el.save_loaded_asset(rtg, False)
out = []
for clip in ("Menace", "TalkAngry"):
    src = "%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2_%s" % (ROOT, n, n, clip)
    dst = "%s/%s/MX7B/Anims/SK_P9_NPC3D_%s_MX7B_%s" % (ROOT, n, n, clip)
    if el.does_asset_exist(dst): out.append(clip + " exists"); continue
    res = unreal.IKRetargetBatchOperation.duplicate_and_retarget([el.find_asset_data(src)], mq2, mesh, rtg, "", "", "", "_tmpRT", False)
    for r in res or []:
        m = r.get_asset()
        if isinstance(m, unreal.AnimSequence):
            el.rename_asset(m.get_path_name().split(".")[0], dst); el.save_asset(dst, False)
            out.append("%s %.2fs" % (clip, m.get_play_length()))
for p in el.list_assets("%s/%s/MX7B/Anims" % (ROOT, n), recursive=False, include_folder=False):
    a = el.load_asset(p)
    out.append("%s %.2fs skel %s" % (a.get_name(), a.get_play_length(), a.get_editor_property("skeleton").get_name()))
unreal.log("P9PY: kanchana 7B: " + "; ".join(out))
