# Stage 9A: a soft spill light inside each platform / footbridge sign box (the internally lit sign glows on its frame, the
# ceiling and the column under it). Unshadowed, 6 cd, 260 cm, warm red-white. Idempotent (found by label). Does NOT save the map.
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EAL = unreal.EditorAssetLibrary
for n in ("dhanraj", "Modi"):
    EAL.save_asset("/Game/P9/Characters/Station3D/Materials/MI_P9_NPC3D_%s" % n, False)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
signs = [l for l in actors if (l.startswith("S3B1_PlatformSign_") or l.startswith("S3A6_PlatformSign_") or l.startswith("S3B1_FOBSign_PF")
         or l.startswith("S3A7_FOBSign_PF"))]
made = 0
for l in sorted(signs):
    s = actors[l]
    o, e = s.get_actor_bounds(False)
    lab = "S9A_SignGlow_" + l.split("_", 1)[1]
    g = actors.get(lab)
    if g is None:
        g = eas.spawn_actor_from_class(unreal.PointLight, o, unreal.Rotator(0, 0, 0))
        g.set_actor_label(lab)
        g.set_folder_path("P9/Lighting/SignGlow9A")
        made += 1
    g.set_actor_location(o, False, False)
    c = g.get_component_by_class(unreal.PointLightComponent)
    c.set_mobility(unreal.ComponentMobility.MOVABLE)
    c.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    c.set_intensity(6.0)
    c.set_attenuation_radius(260.0)
    c.set_light_color(unreal.LinearColor(1.0, 0.55, 0.48, 1.0))
    c.set_cast_shadows(False)
    c.set_editor_property("source_radius", 40.0)
    c.set_editor_property("specular_scale", 0.3)
    unreal.log("P9PY: %s at (%.0f %.0f %.0f)" % (lab, o.x, o.y, o.z))
unreal.log("P9PY: sign glow lights: %d signs, %d new" % (len(signs), made))
