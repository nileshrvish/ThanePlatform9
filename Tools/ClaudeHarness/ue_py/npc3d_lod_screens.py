# Stage 5C: set LOD screen sizes 1.0 / 0.35 / 0.12 on LODS_P9_NPC3D and re-apply it to the 7 NPC3D meshes. Saves those assets.
import unreal
el = unreal.EditorAssetLibrary
ROOT = "/Game/P9/Characters/Station3D"
NAMES = ["dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"]
lods = el.load_asset(ROOT + "/LODS_P9_NPC3D"); lods.modify()
groups = list(lods.get_editor_property("lod_groups"))
for g, ss in zip(groups, (1.0, 0.35, 0.12)):
    try:
        pf = unreal.PerPlatformFloat(default=ss)
    except Exception as e:
        unreal.log("P9PY: ctor kwargs failed %s" % e); pf = unreal.PerPlatformFloat(ss)
    g.set_editor_property("screen_size", pf)
lods.set_editor_property("lod_groups", groups)
got = [g.get_editor_property("screen_size").default for g in lods.get_editor_property("lod_groups")]
unreal.log("P9PY: LOD screen sizes now %s" % got)
if abs(got[1] - 0.35) > 1e-3:
    raise RuntimeError("P9PY: could not set screen sizes")
el.save_loaded_asset(lods, False)
ss_sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
out = []
for n in NAMES:
    sk = el.load_asset("%s/%s/SK_P9_NPC3D_%s" % (ROOT, n, n)); sk.modify()
    ok = ss_sub.regenerate_lod(sk, 3, True, False)
    el.save_loaded_asset(sk, False)
    out.append("%s ok=%s verts=%s" % (n, ok, [ss_sub.get_num_verts(sk, i) for i in range(ss_sub.get_lod_count(sk))]))
unreal.log("P9PY: lod screens " + " | ".join(out))
