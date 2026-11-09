# Stage 8A read-only: material slots / parameters of the train interior and shell, the stair treads, the FOB shell and the
# station tube light; the PF9/10 light actors' settings. Modifies nothing.
import unreal

def mat_info(m):
    if not m:
        return "None"
    s = m.get_path_name()
    try:
        base = m
        chain = []
        while isinstance(base, unreal.MaterialInstance):
            chain.append(base.get_name())
            base = base.get_editor_property("parent")
        s += " base=%s" % (base.get_name() if base else None)
        if isinstance(m, unreal.MaterialInstance):
            sp = [(p.parameter_info.name, round(p.parameter_value, 3)) for p in m.get_editor_property("scalar_parameter_values")]
            vp = [(p.parameter_info.name, [round(p.parameter_value.r, 3), round(p.parameter_value.g, 3), round(p.parameter_value.b, 3)]) for p in m.get_editor_property("vector_parameter_values")]
            tp = [(p.parameter_info.name, p.parameter_value.get_name() if p.parameter_value else None) for p in m.get_editor_property("texture_parameter_values")]
            s += " S=%s V=%s T=%s" % (sp, vp, tp)
        if base:
            for kind, fn in (("scal", unreal.MaterialEditingLibrary.get_scalar_parameter_names),
                             ("vec", unreal.MaterialEditingLibrary.get_vector_parameter_names),
                             ("tex", unreal.MaterialEditingLibrary.get_texture_parameter_names)):
                try:
                    s += " %s:%s" % (kind, [str(n) for n in fn(base)])
                except Exception as e:
                    pass
    except Exception as e:
        s += " err %s" % e
    return s

for path in ["/Game/P9/Environment/Trains/SM_P9_TrainInterior_Cab", "/Game/P9/Environment/Trains/SM_P9_LocalTrain_CabShell"]:
    sm = unreal.load_asset(path)
    if not sm:
        unreal.log("P9PY: missing %s" % path)
        continue
    b = sm.get_bounding_box()
    unreal.log("P9PY: %s bounds %s..%s" % (path, b.min, b.max))
    for i, s in enumerate(sm.get_editor_property("static_materials")):
        unreal.log("P9PY:   slot %d %s -> %s" % (i, s.material_slot_name, mat_info(s.material_interface)))

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
want = ("S3B2_StairFinish_PF9_10", "S3C_FOBShell_B", "S3B3_TubeLight_PF9_10_W1", "S3B2_FOBFloor_DeckB", "S2_FOB_Roof_B",
        "S3C_StairWall_PF9_10_N", "S2_FOB_Deck_B", "S2_FOB_Parapet_W_B", "S3B3_TubeLight_FOB_7410")
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if l in want:
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.static_mesh
            unreal.log("P9PY: %s mesh %s visible=%s coll=%s" % (l, m.get_path_name() if m else None, c.is_visible(), c.get_collision_profile_name()))
            for i, mm in enumerate(c.get_materials()):
                unreal.log("P9PY:   [%d] %s" % (i, mat_info(mm)))
    if l.startswith("S2C_Lamp_PF9") or l.startswith("S3C_Fill_PF9") or l.startswith("S2C_FOB_") or l.startswith("S2C_Stair_PF9"):
        for c in a.get_components_by_class(unreal.LightComponent):
            extra = ""
            if isinstance(c, unreal.SpotLightComponent):
                extra = " inner=%.0f outer=%.0f" % (c.inner_cone_angle, c.outer_cone_angle)
            unreal.log("P9PY: LIGHT %s %s i=%.1f units=%s r=%.0f color=%s temp=%s(%.0f) shadows=%s mob=%s rot=%s%s" % (
                l, c.get_class().get_name(), c.intensity, c.get_editor_property("intensity_units"), c.attenuation_radius,
                c.light_color, c.get_editor_property("use_temperature"), c.get_editor_property("temperature"), c.cast_shadows,
                c.mobility, a.get_actor_rotation(), extra))
