# Stage 5B lighting retune (editor world). Same actors and components (LampFault spawn-point references stay valid);
# only light parameters change. Values are always derived from the ORIGINAL values recorded by inspect_5b.py, so the
# script can be re-run with new factors without compounding. Out of scope (untouched): track-crossing lamps, moon,
# sky light, train interior lights. Does not save; the map is saved separately after review.
import json
import unreal

ORIG = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\inspect_5b.json"
TEMPERATURE = 4200.0

# group: (label prefixes, intensity factor vs original, attenuation radius, spot inner, spot outer)
GROUPS = {
    "platform_spot": (("S2C_Lamp_", "S3A5_Lamp_"), 0.5, 1500.0, 45.0, 80.0),
    "platform_fill": (("S3C_Fill_",), 1.0, 700.0, None, None),
    "footbridge":    (("S2C_FOB_",), 0.45, 1400.0, None, None),
    "stairs":        (("S2C_Stair_",), 0.8, 1200.0, None, None),
    "concourse":     (("S2C_Conc_",), 0.4, 1200.0, None, None),
}
# v2: footbridge/concourse point lights sit just under their tube meshes (they hung 1 m lower and up to 2 m off along Y)
ALIGN_TO_TUBE = {"S2C_FOB_": "S3B3_TubeLight_FOB_", "S2C_Conc_": "S3B3_TubeLight_Conc_"}
UNDER_TUBE = 60.0   # the fixture pivot is at the ceiling (z 900); the glowing tube hangs at its bottom (~z 840)

tubes = {a.get_actor_label(): a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
         if a.get_actor_label().startswith("S3B3_TubeLight_")}
moved = []
orig = {(l["actor"], l["comp"]): l for l in json.load(open(ORIG, encoding="utf-8"))["lights"]}
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
counts = {g: 0 for g in GROUPS}
for a in sub.get_all_level_actors():
    label = a.get_actor_label()
    group = next((g for g, spec in GROUPS.items() if label.startswith(spec[0])), None)
    if not group:
        continue
    _, factor, radius, inner, outer = GROUPS[group]
    for lc in a.get_components_by_class(unreal.LightComponent):
        o = orig.get((label, lc.get_name()))
        if not o:
            unreal.log_warning("P9PY: light_5b no original record for %s/%s - skipped" % (label, lc.get_name()))
            continue
        a.modify()
        lc.modify()
        lc.set_editor_property("intensity", float(o["intensity"]) * factor)
        lc.set_editor_property("attenuation_radius", radius)
        lc.set_editor_property("light_color", unreal.Color(255, 255, 255, 255))
        lc.set_editor_property("use_temperature", True)
        lc.set_editor_property("temperature", TEMPERATURE)
        if inner is not None and isinstance(lc, unreal.SpotLightComponent):
            lc.set_editor_property("inner_cone_angle", inner)
            lc.set_editor_property("outer_cone_angle", outer)
        counts[group] += 1
    for lp, tp in ALIGN_TO_TUBE.items():
        if label.startswith(lp):
            tube = tubes.get(tp + label[len(lp):])
            if tube:
                t = tube.get_actor_location()
                new = unreal.Vector(t.x, t.y, t.z - UNDER_TUBE)
                old = a.get_actor_location()
                if (old - new).length() > 0.5:
                    a.set_actor_location(new, False, False)
                    moved.append("%s (%.0f,%.0f,%.0f)->(%.0f,%.0f,%.0f)" % (label, old.x, old.y, old.z, new.x, new.y, new.z))
            else:
                unreal.log_warning("P9PY: light_5b no tube for %s" % label)
unreal.log("P9PY: light_5b moved %d: %s" % (len(moved), "; ".join(moved)))
unreal.log("P9PY: light_5b applied %s (T=%.0fK) %s" % (counts, TEMPERATURE,
           {g: "x%.2f r%.0f" % (s[1], s[2]) + (" cone %.0f/%.0f" % (s[3], s[4]) if s[3] else "") for g, s in GROUPS.items()}))
