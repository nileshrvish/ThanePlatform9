# Stage 9C (read-only): the expressions of M_P9_TexturedLit and what feeds each material input
import unreal
MEL = unreal.MaterialEditingLibrary
m = unreal.load_asset("/Game/P9/Environment/Shared/M_P9_TexturedLit")
log = lambda s: unreal.log("P9PY: " + s)
log("shading %s blend %s num expr %d" % (m.get_editor_property("shading_model"), m.get_editor_property("blend_mode"), MEL.get_num_material_expressions(m)))
for prop in ("MP_BASE_COLOR", "MP_ROUGHNESS", "MP_METALLIC", "MP_SPECULAR", "MP_NORMAL", "MP_EMISSIVE_COLOR", "MP_AMBIENT_OCCLUSION"):
    n = MEL.get_material_property_input_node(m, getattr(unreal.MaterialProperty, prop))
    log("%s <- %s" % (prop, (n.get_class().get_name() + ":" + n.get_name()) if n else None))
    if n:
        for i in MEL.get_inputs_for_material_expression(m, n):
            log("     in %s" % ((i.get_class().get_name() + ":" + i.get_name() + (" " + str(i.get_editor_property("parameter_name")) if hasattr(i, "parameter_name") else "")) if i else None))
            if i:
                for j in MEL.get_inputs_for_material_expression(m, i):
                    log("         in %s" % ((j.get_class().get_name() + ":" + j.get_name()) if j else None))
