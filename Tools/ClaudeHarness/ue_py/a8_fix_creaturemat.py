# Stage 8A: M_P9_Creature8A did not compile - its Masks samplers had sRGB engine textures as defaults. Defaults become linear
# mask textures (the Lantern Man's own roughness / emissive), then recompile and save (this material asset only).
import unreal
MEL = unreal.MaterialEditingLibrary
m = unreal.load_asset("/Game/P9/Creatures8A/Materials/M_P9_Creature8A")
fixes = {"RoughnessTex": "/Game/P9/Creatures8A/LanternMan/T_P9_LanternMan_Roughness", "EmissiveMaskTex": "/Game/P9/Creatures8A/LanternMan/T_P9_LanternMan_Emissive"}
n = 0
for e in m.get_editor_property("expression_collection").get_editor_property("expressions"):
    if isinstance(e, unreal.MaterialExpressionTextureSampleParameter2D):
        name = str(e.get_editor_property("parameter_name"))
        if name in fixes:
            e.set_editor_property("texture", unreal.load_asset(fixes[name]))
            n += 1
        unreal.log("P9PY: %s sampler %s texture %s" % (name, e.get_editor_property("sampler_type"), e.get_editor_property("texture").get_name()))
MEL.recompile_material(m)
s = MEL.get_statistics(m)
unreal.log("P9PY: fixed %d defaults; stats vs %s ps %s samplers %s" % (n, s.num_vertex_shader_instructions, s.num_pixel_shader_instructions, s.num_samplers))
unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False)
for p in ("/Game/P9/Creatures8A/Materials/MI_P9_LanternMan", "/Game/P9/Creatures8A/Materials/MI_P9_Greeting"):
    mi = unreal.load_asset(p); MEL.update_material_instance(mi); unreal.EditorAssetLibrary.save_loaded_asset(mi, only_if_is_dirty=False)
