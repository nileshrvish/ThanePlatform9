# Stage 9A (editor, read-only): sign / indicator / NPC materials and their parameters, post-process volumes, lights' units.
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
seen = set()


def describe_mi(m, tag):
    if m is None or m.get_path_name() in seen:
        return
    seen.add(m.get_path_name())
    s = "%s %s" % (tag, m.get_path_name())
    if isinstance(m, unreal.MaterialInstance):
        parent = m.get_editor_property("parent")
        s += " parent=%s" % (parent.get_name() if parent else None)
        for p in m.get_editor_property("scalar_parameter_values"):
            s += " %s=%.3f" % (p.parameter_info.name, p.parameter_value)
        for p in m.get_editor_property("vector_parameter_values"):
            c = p.parameter_value
            s += " %s=(%.2f,%.2f,%.2f)" % (p.parameter_info.name, c.r, c.g, c.b)
        for p in m.get_editor_property("texture_parameter_values"):
            s += " %s=%s" % (p.parameter_info.name, p.parameter_value.get_name() if p.parameter_value else None)
        base = m.get_base_material()
    else:
        base = m
    if base and base.get_path_name() not in seen:
        seen.add(base.get_path_name())
        try:
            sc = mel.get_scalar_parameter_names(base); vc = mel.get_vector_parameter_names(base); tc = mel.get_texture_parameter_names(base)
        except Exception as e:
            sc = vc = tc = str(e)
        unreal.log("P9PY: BASE %s shading=%s blend=%s twosided=%s scalars=%s vectors=%s textures=%s" % (base.get_path_name(), base.get_editor_property("shading_model"),
                   base.get_editor_property("blend_mode"), base.get_editor_property("two_sided"), list(sc), list(vc), list(tc)))
    unreal.log("P9PY: " + s)


for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if any(k in l for k in ("Sign", "Indicator", "NameBoard_Thane")) and isinstance(a, unreal.StaticMeshActor):
        c = a.static_mesh_component
        mats = [c.get_material(i) for i in range(c.get_num_materials())]
        unreal.log("P9PY: SIGN %s mesh=%s loc=%s mats=%s" % (l, c.static_mesh.get_name() if c.static_mesh else None, a.get_actor_location(), [m.get_name() if m else None for m in mats]))
        for m in mats:
            describe_mi(m, "  MAT")
    if isinstance(a, unreal.P9FriendNPC) or isinstance(a, unreal.P9SilhouetteStalker):
        for c in a.get_components_by_class(unreal.SkeletalMeshComponent):
            mats = [c.get_material(i) for i in range(c.get_num_materials())]
            unreal.log("P9PY: NPC %s mesh=%s mats=%s cast=%s" % (l, c.get_skeletal_mesh_asset().get_name() if c.get_skeletal_mesh_asset() else None, [m.get_name() if m else None for m in mats], c.cast_shadow))
            for m in mats:
                describe_mi(m, "  MAT")
    if isinstance(a, unreal.PostProcessVolume):
        s = a.settings
        unreal.log("P9PY: PPV %s unbound=%s exp_method=%s(ov %s) bias=%.2f(ov %s) minEV=%.2f(ov %s) maxEV=%.2f(ov %s) bloom=%.2f(ov %s) lumen? gi=%s refl=%s" % (
            l, a.unbound, s.auto_exposure_method, s.override_auto_exposure_method, s.auto_exposure_bias, s.override_auto_exposure_bias,
            s.auto_exposure_min_brightness, s.override_auto_exposure_min_brightness, s.auto_exposure_max_brightness, s.override_auto_exposure_max_brightness,
            s.bloom_intensity, s.override_bloom_intensity, s.dynamic_global_illumination_method, s.reflection_method))
    if isinstance(a, (unreal.SkyLight, unreal.DirectionalLight, unreal.ExponentialHeightFog)):
        c = a.get_component_by_class(unreal.LightComponentBase) if not isinstance(a, unreal.ExponentialHeightFog) else None
        unreal.log("P9PY: ENV %s %s intensity=%s color=%s" % (l, a.get_class().get_name(), c.intensity if c else "-", c.light_color if c else "-"))
