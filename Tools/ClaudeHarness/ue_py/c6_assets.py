# Stage 6C assets (new assets in /Game/P9/Anomalies6C only; the map is not touched): clock master materials + instances,
# slot assignment on the imported meshes, a box collision on the body, LOD report. Never overwrites an existing material asset.
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
ROOT = "/Game/P9/Anomalies6C"
MATS = ROOT + "/Materials"
WHITE = unreal.load_asset("/Engine/EngineResources/WhiteSquareTexture")
saved = []

def master_opaque():
    p = MATS + "/M_P9_AnalogClock"
    if EAL.does_asset_exist(p):
        return unreal.load_asset(p)
    m = AT.create_asset("M_P9_AnalogClock", MATS, unreal.Material, unreal.MaterialFactoryNew())
    tex = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -700, -200)
    tex.set_editor_property("parameter_name", "BaseTexture"); tex.set_editor_property("texture", WHITE)
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -700, 50)
    tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, -100)
    MEL.connect_material_expressions(tex, "RGB", mul, "A"); MEL.connect_material_expressions(tint, "", mul, "B")
    met = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -350, 100)
    met.set_editor_property("parameter_name", "Metallic"); met.set_editor_property("default_value", 0.0)
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -350, 220)
    rough.set_editor_property("parameter_name", "Roughness"); rough.set_editor_property("default_value", 0.5)
    MEL.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m); saved.append(p)
    return m

def master_glass():
    p = MATS + "/M_P9_AnalogClockGlass"
    if EAL.does_asset_exist(p):
        return unreal.load_asset(p)
    m = AT.create_asset("M_P9_AnalogClockGlass", MATS, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    tint = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -400, -100)
    tint.set_editor_property("parameter_name", "Tint"); tint.set_editor_property("default_value", unreal.LinearColor(0.85, 0.9, 0.95, 1))
    op = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 100)
    op.set_editor_property("parameter_name", "Opacity"); op.set_editor_property("default_value", 0.10)
    rough = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -400, 200)
    rough.set_editor_property("parameter_name", "Roughness"); rough.set_editor_property("default_value", 0.05)
    MEL.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(op, "", unreal.MaterialProperty.MP_OPACITY)
    MEL.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(m); saved.append(p)
    return m

opaque, glass = master_opaque(), master_glass()
dial_tex = unreal.load_asset(ROOT + "/Textures/T_P9_AnalogClock_Dial")
# name -> (parent, tint, metallic, roughness, texture, opacity)
SPEC = {
    "MI_P9_AnalogClock_Housing": (opaque, (0.78, 0.79, 0.80), 1.0, 0.30, None, None),     # brushed aluminium drum and bezels
    "MI_P9_AnalogClock_Dial": (opaque, (1.0, 1.0, 1.0), 0.0, 0.55, dial_tex, None),
    "MI_P9_AnalogClock_Mount": (opaque, (0.50, 0.51, 0.51), 1.0, 0.50, None, None),       # galvanised pipe
    "MI_P9_AnalogClock_Hand": (opaque, (0.012, 0.012, 0.013), 0.0, 0.45, None, None),
    "MI_P9_AnalogClock_SecondHand": (opaque, (0.55, 0.025, 0.02), 0.0, 0.40, None, None),
    "MI_P9_AnalogClock_Glass": (glass, (0.85, 0.9, 0.95), None, 0.05, None, 0.10),
}
mis = {}
for name, (parent, tint, metal, rough, tex, opac) in SPEC.items():
    p = MATS + "/" + name
    if EAL.does_asset_exist(p):
        mis[name] = unreal.load_asset(p); unreal.log("P9PY: exists (left alone) " + p); continue
    mi = AT.create_asset(name, MATS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(*tint, 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    if metal is not None: MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    if tex: MEL.set_material_instance_texture_parameter_value(mi, "BaseTexture", tex)
    if opac is not None: MEL.set_material_instance_scalar_parameter_value(mi, "Opacity", opac)
    MEL.update_material_instance(mi); mis[name] = mi; saved.append(p)

sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for mesh_name in ["SM_P9_AnalogClock_Body", "SM_P9_AnalogClock_Glass", "SM_P9_AnalogClock_HourHand", "SM_P9_AnalogClock_MinuteHand", "SM_P9_AnalogClock_SecondHand"]:
    p = ROOT + "/" + mesh_name
    sm = unreal.load_asset(p)
    changed = False
    for i, slot in enumerate(sm.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        mi = next((m for k, m in mis.items() if slot_name.startswith(k)), None)
        if mi and sm.get_material(i) != mi:
            sm.set_material(i, mi); changed = True
    if mesh_name.endswith("Body") and sme.get_simple_collision_count(sm) == 0:
        sme.add_simple_collisions(sm, unreal.ScriptCollisionShapeType.BOX); changed = True
    lods = sme.get_lod_count(sm)
    info = ", ".join("LOD%d %d tris" % (l, sme.get_number_verts(sm, l) and sm.get_num_triangles(l)) for l in range(lods))
    bb = sm.get_bounding_box()
    size = bb.max - bb.min
    unreal.log("P9PY: %s %s | size %.1f x %.1f x %.1f cm | slots %s | collision %d | screen sizes %s" % (
        mesh_name, info, size.x, size.y, size.z,
        [(str(s.get_editor_property("material_slot_name")), sm.get_material(i).get_name() if sm.get_material(i) else None) for i, s in enumerate(sm.get_editor_property("static_materials"))],
        sme.get_simple_collision_count(sm), list(sme.get_lod_screen_sizes(sm)) if hasattr(sme, "get_lod_screen_sizes") else "auto"))
    if changed:
        saved.append(p)
for p in sorted(set(saved)):
    EAL.save_asset(p, only_if_is_dirty=False)
unreal.log("P9PY: c6_assets saved %d assets: %s" % (len(set(saved)), sorted(set(saved))))
