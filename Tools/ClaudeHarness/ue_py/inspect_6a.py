# Stage 6A read-only inspection: actors relevant to the new anomaly set. Writes JSON; modifies nothing.
import json
import re
import unreal

OUT = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad\inspect_6a.json"
KEY = re.compile(r"(?i)clock|indicator|sign|gate|scaffold|barric|booth|ticket|counter|window|track|rail|stair|step|poster|adstand|fob|deck|parapet|roof|concourse|announcer|director|spawn|silhou|landing|tarp|fence|wall|grill|bar")

def v3(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out = []
for a in sub.get_all_level_actors():
    label = a.get_actor_label()
    cls = a.get_class().get_name()
    if not KEY.search(label) and not KEY.search(cls):
        continue
    origin, ext = a.get_actor_bounds(False, True)
    d = {"label": label, "cls": cls, "folder": str(a.get_folder_path()), "loc": v3(a.get_actor_location()),
         "yaw": round(a.get_actor_rotation().yaw, 1), "scale": v3(a.get_actor_scale3d()),
         "bmin": v3(origin - ext), "bmax": v3(origin + ext), "comps": []}
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh = c.static_mesh
        mats = []
        try:
            mats = [m.get_path_name() if m else None for m in c.get_materials()]
        except Exception:
            pass
        d["comps"].append({"name": c.get_name(), "mesh": mesh.get_path_name() if mesh else None, "mats": mats,
                           "isc": isinstance(c, unreal.InstancedStaticMeshComponent),
                           "n": c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1,
                           "coll": str(c.get_collision_profile_name())})
    if isinstance(a, unreal.P9AnomalySpawnPoint):
        d["point"] = {"id": str(a.get_editor_property("point_id")), "type": str(a.get_editor_property("location_type")),
                      "target": a.get_editor_property("target_actor").get_actor_label() if a.get_editor_property("target_actor") else None}
    out.append(d)
with open(OUT, "w") as f:
    json.dump(out, f, indent=1)
unreal.log("P9PY: inspect_6a wrote %d actors" % len(out))
