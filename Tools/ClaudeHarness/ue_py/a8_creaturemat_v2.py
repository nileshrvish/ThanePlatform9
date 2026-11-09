# Stage 8A: M_P9_Creature8A_v2 - the creature master material with sampler defaults that match their types (the first version did
# not compile: Masks samplers with sRGB defaults). MI_P9_LanternMan / MI_P9_Greeting are re-parented to it. Saves these 3 assets.
import unreal
EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
MF = "/Game/P9/Creatures8A/Materials"
LM = "/Game/P9/Creatures8A/LanternMan"
path = MF + "/M_P9_Creature8A_v2"
if EAL.does_asset_exist(path):
    raise RuntimeError("exists " + path)
m = AT.create_asset("M_P9_Creature8A_v2", MF, unreal.Material, unreal.MaterialFactoryNew())
def tex_param(name, x, y, default, sampler):
    e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("sampler_type", sampler)
    e.set_editor_property("texture", unreal.load_asset(default))
    return e
bc = tex_param("BaseColorTex", -800, -200, LM + "/T_P9_LanternMan_BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
nm = tex_param("NormalTex", -800, 100, LM + "/T_P9_LanternMan_Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
rg = tex_param("RoughnessTex", -800, 400, LM + "/T_P9_LanternMan_Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
em = tex_param("EmissiveMaskTex", -800, 700, LM + "/T_P9_LanternMan_Emissive", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
rs = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 500); rs.set_editor_property("parameter_name", "RoughnessScale"); rs.set_editor_property("default_value", 1.0)
rmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 420)
MEL.connect_material_expressions(rg, "R", rmul, "A"); MEL.connect_material_expressions(rs, "", rmul, "B")
ec = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 800); ec.set_editor_property("parameter_name", "EmissiveColor")
ec.set_editor_property("default_value", unreal.LinearColor(1.0, 0.55, 0.22, 1.0))
es = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 950); es.set_editor_property("parameter_name", "EmissiveStrength"); es.set_editor_property("default_value", 0.0)
m1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 720)
MEL.connect_material_expressions(em, "R", m1, "A"); MEL.connect_material_expressions(ec, "", m1, "B")
m2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 800)
MEL.connect_material_expressions(m1, "", m2, "A"); MEL.connect_material_expressions(es, "", m2, "B")
MEL.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
MEL.connect_material_property(rmul, "", unreal.MaterialProperty.MP_ROUGHNESS)
MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
MEL.recompile_material(m)
s = MEL.get_statistics(m)
unreal.log("P9PY: v2 stats vs %s ps %s samplers %s" % (s.num_vertex_shader_instructions, s.num_pixel_shader_instructions, s.num_samplers))
EAL.save_loaded_asset(m, only_if_is_dirty=False)
for p in (MF + "/MI_P9_LanternMan", MF + "/MI_P9_Greeting"):
    mi = unreal.load_asset(p)
    MEL.set_material_instance_parent(mi, m)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, only_if_is_dirty=False)
    unreal.log("P9PY: %s parent now %s" % (mi.get_name(), mi.get_editor_property("parent").get_name()))
