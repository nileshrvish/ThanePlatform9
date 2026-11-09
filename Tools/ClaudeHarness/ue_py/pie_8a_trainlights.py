# Stage 8A (PIE, read-only): the Vashi local's interior lights and each interior material slot (is the tube glow driven?).
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
for t in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9Train):
    unreal.log('P9PY: train %s at %s' % (t.get_name(), t.get_actor_location()))
    for c in t.get_components_by_class(unreal.PointLightComponent):
        unreal.log("P9PY: %s %s i=%.1f" % (t.get_name(), c.get_name(), c.intensity))
    for c in t.get_components_by_class(unreal.StaticMeshComponent):
        if "Interior" not in c.get_name():
            continue
        for i, m in enumerate(c.get_materials()):
            v = None
            if isinstance(m, unreal.MaterialInstance):
                try:
                    v = m.get_scalar_parameter_value("Intensity")
                except Exception:
                    v = None
            unreal.log("P9PY: %s slot %d %s (%s) Intensity=%s" % (c.get_name(), i, m.get_name() if m else None, m.get_class().get_name() if m else "", v))
