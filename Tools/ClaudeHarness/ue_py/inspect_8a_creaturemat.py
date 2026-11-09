# Stage 8A (read-only): what the creature materials actually contain.
import unreal
MEL = unreal.MaterialEditingLibrary
for p in ("/Game/P9/Creatures8A/Materials/MI_P9_LanternMan", "/Game/P9/Creatures8A/Materials/MI_P9_Greeting"):
    mi = unreal.load_asset(p)
    tex = [(t.parameter_info.name, t.parameter_value.get_name() if t.parameter_value else None) for t in mi.get_editor_property("texture_parameter_values")]
    sc = [(s.parameter_info.name, s.parameter_value) for s in mi.get_editor_property("scalar_parameter_values")]
    unreal.log("P9PY: %s parent %s tex %s scalars %s" % (mi.get_name(), mi.get_editor_property("parent").get_name(), tex, sc))
m = unreal.load_asset("/Game/P9/Creatures8A/Materials/M_P9_Creature8A")
unreal.log("P9PY: master tex params %s" % [str(n) for n in MEL.get_texture_parameter_names(m)])
for e in MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR), :
    unreal.log("P9PY: base colour input node %s" % (e.get_name() if e else None))
for p in ("/Game/P9/Creatures8A/Greeting/SM_P9_Greeting_rest", "/Game/P9/Creatures8A/LanternMan/SM_P9_LanternMan"):
    sm = unreal.load_asset(p)
    unreal.log("P9PY: %s slots %s" % (sm.get_name(), [(str(s.material_slot_name), s.material_interface.get_name() if s.material_interface else None) for s in sm.get_editor_property("static_materials")]))
t = unreal.load_asset("/Game/P9/Creatures8A/Greeting/T_P9_Greeting_BaseColor")
unreal.log("P9PY: basecolor %dx%d srgb %s" % (t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("srgb")))
