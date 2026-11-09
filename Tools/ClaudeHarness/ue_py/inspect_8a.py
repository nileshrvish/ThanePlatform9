# Stage 8A read-only inspection: actors in the north footbridge (y 5000..8200, z > 500), the PF9/10 stairs and Platform 9/10
# island (y 6400..8200, z < 700), every light in those areas, the anomaly points and Kanchana. Writes JSON; modifies nothing.
import json
import unreal

OUT = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\2881a0a7-5f5b-4397-bc76-9261cc309caa\scratchpad\inspect_8a.json"


def v3(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out = []
for a in sub.get_all_level_actors():
    label = a.get_actor_label()
    cls = a.get_class().get_name()
    origin, ext = a.get_actor_bounds(False, True)
    bmin, bmax = origin - ext, origin + ext
    keep = isinstance(a, (unreal.P9AnomalySpawnPoint, unreal.P9SilhouetteStalker, unreal.P9FriendNPC, unreal.P9CampaignDirector, unreal.P9Train))
    # overlaps the north part of the station?
    if bmax.y > 5000 and bmin.y < 8400 and bmin.x < 3300 and bmax.x > -3300 and ext.x < 4000:
        keep = True
    if not keep:
        continue
    d = {"label": label, "cls": cls, "folder": str(a.get_folder_path()), "loc": v3(a.get_actor_location()),
         "rot": v3(unreal.Vector(a.get_actor_rotation().roll, a.get_actor_rotation().pitch, a.get_actor_rotation().yaw)),
         "scale": v3(a.get_actor_scale3d()), "bmin": v3(bmin), "bmax": v3(bmax), "hidden": a.is_hidden_ed(), "comps": []}
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh = c.static_mesh
        try:
            mats = [m.get_name() if m else None for m in c.get_materials()]
        except Exception:
            mats = []
        isc = isinstance(c, unreal.InstancedStaticMeshComponent)
        d["comps"].append({"name": c.get_name(), "mesh": mesh.get_path_name() if mesh else None, "mats": mats, "isc": isc,
                           "n": c.get_instance_count() if isc else 1, "coll": str(c.get_collision_profile_name()),
                           "mob": str(c.mobility).split(".")[-1]})
    for c in a.get_components_by_class(unreal.LightComponent):
        d["comps"].append({"light": c.get_name(), "cls": c.get_class().get_name(), "i": round(c.intensity, 2),
                           "units": str(c.get_editor_property("intensity_units")).split(".")[-1] if hasattr(c, "intensity_units") else "",
                           "color": [c.light_color.r, c.light_color.g, c.light_color.b],
                           "r": round(c.get_editor_property("attenuation_radius"), 1) if isinstance(c, unreal.LocalLightComponent) else 0,
                           "wloc": v3(c.get_world_location()), "vis": c.is_visible(), "shadows": c.cast_shadows,
                           "mob": str(c.mobility).split(".")[-1]})
    if isinstance(a, unreal.P9AnomalySpawnPoint):
        t = a.get_editor_property("target_actor")
        d["point"] = {"id": str(a.get_editor_property("point_id")), "type": str(a.get_editor_property("location_type")),
                      "target": t.get_actor_label() if t else None,
                      "trig": str(a.get_editor_property("loop_trigger")), "off": v3(a.get_editor_property("loop_offset"))}
    out.append(d)
with open(OUT, "w") as f:
    json.dump(out, f, indent=1)
unreal.log("P9PY: inspect_8a wrote %d actors" % len(out))
