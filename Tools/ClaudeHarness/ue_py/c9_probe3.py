# Stage 9C probe 3: visible meshes whose bounds contain each test spot near floor height (the finishes over the grey collision)
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = lambda s: unreal.log("P9PY: " + s)
pts = {"PF1": (1100, -1050, 0), "PF1stairsMid": (-800, -1525, 200), "PF1landing": (-300, -1525, 300), "FOBdeck": (0, 3000, 600), "Concourse": (-250, 3150, 600), "Booking": (-600, 3300, 600),
       "FOBnorth": (0, 6400, 600), "FOBsouth": (0, 1000, 600), "PF9": (1700, 7100, 0), "PF910stairsMid": (700, 7410, 400), "PF910landing": (1450, 7410, 300), "PF9west": (-2800, 7440, 0), "FOBstairsPF23": (0, 900, 300)}
acts = []
for a in eas.get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        if not c.static_mesh: continue
        o, e, _r = unreal.SystemLibrary.get_component_bounds(c)
        acts.append((a, c, o, e))
for k, (x, y, z) in pts.items():
    s = []
    for a, c, o, e in acts:
        if abs(x - o.x) <= e.x and abs(y - o.y) <= e.y and (o.z - e.z) - 30 <= z + 20 and (o.z + e.z) >= z - 10 and e.z < 400:
            mats = sorted(set(m.get_name() for m in c.get_materials() if m))
            s.append("%s(%s,col=%s,top=%.0f)[%s]" % (a.get_actor_label(), c.static_mesh.get_name(), str(c.get_collision_enabled()).split(".")[-1][:6], o.z + e.z, ",".join(mats)[:70]))
    log("%s: %s" % (k, " | ".join(s[:8])))
