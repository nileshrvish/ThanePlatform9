# Stage 9C probe (editor world, read-only): what is under the feet at the footstep test spots, the PF1 bench geometry, the level's material use
import unreal
from collections import Counter
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log = lambda s: unreal.log("P9PY: " + s)
pts = {"PF1": (1100, -1050), "PF1stairs": (-800, -1525), "FOBdeck": (0, 3000), "Concourse": (-250, 3150), "Booking": (-600, 3300), "FOBnorth": (0, 6400),
       "PF9": (1700, 7100), "PF910stairs": (850, 7410), "PF910landing": (1450, 7410), "PF9east": (2800, 7100), "PF9west": (-2800, 7440), "FOBstairsPF23": (0, 1500)}
for k, (x, y) in pts.items():
    hits = unreal.SystemLibrary.line_trace_multi(w, unreal.Vector(x, y, 1200), unreal.Vector(x, y, -200), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
    s = []
    for h in hits[:4]:
        t = h.to_tuple()
        comp = t[10]
        act = t[9]; pm = t[8]
        loc = t[4]
        mat = ""
        try:
            fi = t[11] if len(t) > 11 else -1
            if comp and hasattr(comp, "get_material"):
                mats = [comp.get_material(i) for i in range(comp.get_num_materials())]
                mat = ",".join(sorted(set(m.get_name() for m in mats if m)))[:120]
        except Exception as e:
            mat = str(e)[:40]
        s.append("%s/%s z=%.0f pm=%s [%s]" % (act.get_actor_label() if act else "?", comp.get_name() if comp else "?", loc.z, pm.get_name() if pm else None, mat))
    log("%s: %s" % (k, " | ".join(s)))
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    p = a.get_actor_location()
    if ("Bench" in l or "bench" in l) and abs(p.y + 1280) < 200 and abs(p.x - 1800) < 600:
        o, e = a.get_actor_bounds(False)
        log("bench %s at %s yaw %.0f bounds %s ext %s" % (l, p, a.get_actor_rotation().yaw, o, e))
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            sm = c.static_mesh
            b = c.get_editor_property("relative_location")
            log("   comp %s mesh %s rel %s mats %s" % (c.get_name(), sm.get_name() if sm else None, b, [m.get_name() for m in c.get_materials() if m]))
cnt = Counter()
for a in eas.get_all_level_actors():
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        for m in c.get_materials():
            if m: cnt[m.get_path_name().split(".")[0]] += 1
for m, n in cnt.most_common(70):
    log("mat %4d %s" % (n, m))
log("distinct materials %d" % len(cnt))
