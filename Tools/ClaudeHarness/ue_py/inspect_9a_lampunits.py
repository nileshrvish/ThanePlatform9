import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if l in ("S2C_Lamp_PF9_10_W1", "S3C_Fill_PF9_10_W1", "S2C_FOB_7410", "S2C_Stair_PF9_10"):
        c = a.get_component_by_class(unreal.LocalLightComponent)
        unreal.log("P9PY: %s %s intensity=%.2f units=%s radius=%.0f color=%s shadows=%s" % (l, a.get_class().get_name(), c.intensity, c.intensity_units, c.attenuation_radius, c.light_color, c.cast_shadows))
