# Stage 5C asset setup (new assets only; the map is not touched). Refuses if the material assets already exist.
# Textures (normal: flip green for Blender/OpenGL bakes; roughness: linear masks), master material + 7 instances,
# 3 LODs per skeletal mesh via a shared LOD settings asset, static epilogue Modi material + LODs. Saves Station3D only.
import unreal
el = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Characters/Station3D"
NAMES = ["dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"]
MAT_DIR = ROOT + "/Materials"
# resumable: M_P9_NPC3D may already exist from an interrupted run of THIS script in the current session (unsaved);
# it is then reused as-is. Everything else must not exist yet.
for p in [MAT_DIR + "/MI_P9_NPC3D_" + n for n in NAMES]:
    if el.does_asset_exist(p):
        raise RuntimeError("P9PY: ABORT exists " + p)
saved = []
def tex(n, k): return el.load_asset("%s/Textures/T_P9_NPC3D_%s_%s" % (ROOT, n, k))

# ---- textures
for n in NAMES:
    for k in ("BaseColor", "Normal", "Roughness"):
        t = tex(n, k); t.modify()
        t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
        if k == "Normal":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            t.set_editor_property("srgb", False); t.set_editor_property("flip_green_channel", True)
            t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
        elif k == "Roughness":
            t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            t.set_editor_property("srgb", False)
        saved.append(t)

# ---- master material
REUSE = el.does_asset_exist(MAT_DIR + "/M_P9_NPC3D")
mat = el.load_asset(MAT_DIR + "/M_P9_NPC3D") if REUSE else at.create_asset("M_P9_NPC3D", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("used_with_skeletal_mesh", True)
def param(cls_name, name, x, y, t, sampler):
    e = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name); e.set_editor_property("texture", t); e.set_editor_property("sampler_type", sampler)
    return e
if REUSE:
    unreal.log("P9PY: reusing M_P9_NPC3D (%d expressions)" % mel.get_num_material_expressions(mat))
else:
  bc = param(None, "BaseColor", -500, -250, tex("dhanraj", "BaseColor"), unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
  nm = param(None, "Normal", -500, 50, tex("dhanraj", "Normal"), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
  rg = param(None, "Roughness", -500, 350, tex("dhanraj", "Roughness"), unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
  mel.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
  mel.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
  mel.connect_material_property(rg, "R", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat); saved.append(mat)

# ---- shared LOD settings: LOD0 full, LOD1 ~33 %, LOD2 ~10 % triangles
if el.does_asset_exist(ROOT + "/LODS_P9_NPC3D"):
    lods = el.load_asset(ROOT + "/LODS_P9_NPC3D")      # from an interrupted run of this script (unsaved)
else:
    daf = unreal.DataAssetFactory(); daf.set_editor_property("data_asset_class", unreal.SkeletalMeshLODSettings)
    lods = at.create_asset("LODS_P9_NPC3D", ROOT, unreal.SkeletalMeshLODSettings, daf)
groups = []
for ss, pct in ((1.0, 1.0), (0.35, 0.33), (0.12, 0.10)):
    g = unreal.SkeletalMeshLODGroupSettings()
    # screen_size is a read-only PerPlatformFloat in Python: engine default kept, reported below
    rs = g.get_editor_property("reduction_settings"); rs.set_editor_property("num_of_triangles_percentage", pct)
    g.set_editor_property("reduction_settings", rs); groups.append(g)
lods.set_editor_property("lod_groups", groups); saved.append(lods)
unreal.log("P9PY: LOD screen sizes %s" % [g.get_editor_property("screen_size").default for g in lods.get_editor_property("lod_groups")])

ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
report = []
for n in NAMES:
    mi = at.create_asset("MI_P9_NPC3D_" + n, MAT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    for k in ("BaseColor", "Normal", "Roughness"):
        mel.set_material_instance_texture_parameter_value(mi, k, tex(n, k))
    mel.update_material_instance(mi); saved.append(mi)
    sk = el.load_asset("%s/%s/SK_P9_NPC3D_%s" % (ROOT, n, n)); sk.modify()
    slot = sk.materials[0].material_slot_name
    sk.set_editor_property("materials", [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=slot)])
    sk.set_editor_property("lod_settings", lods)
    ok = ss.regenerate_lod(sk, 3, False, False)
    report.append("%s lods=%d ok=%s verts=%s" % (n, ss.get_lod_count(sk), ok, [ss.get_num_verts(sk, i) for i in range(ss.get_lod_count(sk))]))
    saved.append(sk)
    if n == "Modi":
        sm = el.load_asset(ROOT + "/Static/SM_P9_NPC3D_Modi"); sm.modify()
        sm.set_material(0, mi)
        opts = unreal.EditorScriptingMeshReductionOptions()
        opts.set_editor_property("reduction_settings", [unreal.EditorScriptingMeshReductionSettings(1.0, 1.0),
                                                        unreal.EditorScriptingMeshReductionSettings(0.33, 0.35),
                                                        unreal.EditorScriptingMeshReductionSettings(0.10, 0.12)])
        unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_lods(sm, opts)
        report.append("static Modi lods=%d" % sm.get_num_lods()); saved.append(sm)
for a in saved:
    el.save_loaded_asset(a, False)
unreal.log("P9PY: npc3d_setup " + " | ".join(report))
unreal.log("P9PY: npc3d_setup saved %d assets" % len(saved))
