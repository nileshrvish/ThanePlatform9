# Stage 5B A/B helper (editor world, does NOT save): put the 41 in-scope lights back to their ORIGINAL recorded values
# (intensity, radius, cones, colour, temperature flag, location). Re-run light_5b_apply.py to return to the retune.
import json
import unreal

ORIG = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\inspect_5b_ORIGINAL.json"
PREFIXES = ("S2C_Lamp_", "S3A5_Lamp_", "S3C_Fill_", "S2C_FOB_", "S2C_Stair_", "S2C_Conc_")

def parse_color(s):
    # "<Struct 'Color' ... {b: 215, g: 230, r: 205, a: 255}>"
    body = s[s.rfind("{") + 1:s.rfind("}")]
    d = {k.strip(): int(v) for k, v in (p.split(":") for p in body.split(","))}
    return unreal.Color(d["r"], d["g"], d["b"], d["a"])

orig = {(l["actor"], l["comp"]): l for l in json.load(open(ORIG, encoding="utf-8"))["lights"]}
n = 0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    label = a.get_actor_label()
    if not label.startswith(PREFIXES):
        continue
    for lc in a.get_components_by_class(unreal.LightComponent):
        o = orig.get((label, lc.get_name()))
        if not o:
            continue
        a.modify(); lc.modify()
        lc.set_editor_property("intensity", float(o["intensity"]))
        lc.set_editor_property("attenuation_radius", float(o["radius"]))
        lc.set_editor_property("light_color", parse_color(o["color"]))
        lc.set_editor_property("use_temperature", bool(o["use_temp"]))
        lc.set_editor_property("temperature", float(o["temp"]))
        if o["inner"] is not None and isinstance(lc, unreal.SpotLightComponent):
            lc.set_editor_property("inner_cone_angle", float(o["inner"]))
            lc.set_editor_property("outer_cone_angle", float(o["outer"]))
        loc = o["loc"]
        if (a.get_actor_location() - unreal.Vector(*loc)).length() > 0.5:
            a.set_actor_location(unreal.Vector(*loc), False, False)
        n += 1
unreal.log("P9PY: light_5b_restore_original restored %d lights (not saved)" % n)
