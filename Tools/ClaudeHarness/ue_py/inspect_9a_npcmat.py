# Stage 9A (editor, read-only): graph of M_P9_NPC3D / M_P9_SignFace / creature material, texture settings of the NPC textures.
import unreal
mel = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty
for path in ("/Game/P9/Characters/Station3D/Materials/M_P9_NPC3D", "/Game/P9/Environment/StationSignage/M_P9_SignFace",
             "/Game/P9/Environment/Shared/M_P9_LEDPanel"):
    m = unreal.load_asset(path)
    unreal.log("P9PY: MAT %s nodes=%d" % (path, mel.get_num_material_expressions(m)))
    for prop in ("MP_BASE_COLOR", "MP_METALLIC", "MP_SPECULAR", "MP_ROUGHNESS", "MP_NORMAL", "MP_EMISSIVE_COLOR", "MP_AMBIENT_OCCLUSION"):
        n = mel.get_material_property_input_node(m, getattr(P, prop))
        desc = None
        if n:
            desc = n.get_class().get_name()
            for k in ("texture", "constant", "r", "parameter_name", "sampler_type", "const_a", "const_b"):
                try:
                    v = n.get_editor_property(k)
                    desc += " %s=%s" % (k, v.get_name() if hasattr(v, "get_name") else v)
                except Exception:
                    pass
            try:
                ins = mel.get_inputs_for_material_expression(m, n)
                desc += " inputs=%s" % [i.get_class().get_name() + ":" + str(getattr(i, "get_editor_property")("parameter_name") if hasattr(i, "parameter_name") else "") for i in ins if i]
            except Exception as e:
                desc += " (inputs ? %s)" % e
        unreal.log("P9PY:   %s <- %s" % (prop, desc))
for n in ("dhanraj", "Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"):
    for k in ("BaseColor", "Normal", "Roughness"):
        t = unreal.load_asset("/Game/P9/Characters/Station3D/Textures/T_P9_NPC3D_%s_%s" % (n, k))
        if t:
            unreal.log("P9PY: TEX %s srgb=%s comp=%s maxsize=%s lodgroup=%s flipgreen=%s size=%dx%d" % (t.get_name(), t.get_editor_property("srgb"),
                       t.get_editor_property("compression_settings"), t.get_editor_property("max_texture_size"), t.get_editor_property("lod_group"),
                       t.get_editor_property("flip_green_channel"), t.blueprint_get_size_x(), t.blueprint_get_size_y()))
for k in ("T_P9_Sign_PF9_10_Red", "T_P9_FOBSign_PF9_10", "T_P9_TrainIndicator_PF9_10_4B"):
    for d in ("/Game/P9/Environment/StationSignage/",):
        t = unreal.load_asset(d + k)
        if t:
            unreal.log("P9PY: TEX %s srgb=%s comp=%s size=%dx%d src=%s" % (k, t.get_editor_property("srgb"), t.get_editor_property("compression_settings"),
                       t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("asset_import_data").get_first_filename() if t.get_editor_property("asset_import_data") else None))
