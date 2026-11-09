# Stage 8A (PIE, read-only): one sample of the PF9/10 lamps' intensities (spot + fill) and the FOB end light.
import unreal
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
vals = []
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Light):
    l = a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name()
    if any(k in l for k in ("Lamp_PF9_10", "Fill_PF9_10")):
        c = a.get_component_by_class(unreal.LightComponent)
        vals.append("%s=%.1f%s" % (l.replace("S2C_Lamp_PF9_10_", "L").replace("S3C_Fill_PF9_10_", "F"), c.intensity, "" if c.is_visible() else "(off)"))
unreal.log("P9PY: lamps " + " ".join(sorted(vals)))
