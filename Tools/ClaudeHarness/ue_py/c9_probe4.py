# Stage 9C probe 4 (editor, read-only): PF1 steel bench seat profile (complex traces) + walkable collision actors by label prefix
import unreal, re
from collections import defaultdict
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log = lambda s: unreal.log("P9PY: " + s)
bench = next(a for a in eas.get_all_level_actors() if a.get_actor_label() == "S3D_SteelBench_PF1_1800")
c = bench.get_component_by_class(unreal.StaticMeshComponent)
log("bench collision %s profile %s" % (c.get_collision_enabled(), c.get_collision_profile_name()))
for x in (1800, 1740):
    row = []
    for y in range(-1320, -1225, 5):
        h = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 150), unreal.Vector(x, y, 1), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        if h is None: row.append("%d:-" % y); continue
        t = h.to_tuple()
        row.append("%d:%s%.0f" % (y, (t[9].get_actor_label()[:3] if t[9] else "?"), t[4].z) if t[0] else "%d:-" % y)
    log("x=%d %s" % (x, " ".join(row)))
groups = defaultdict(list)
for a in eas.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor): continue
    sc = a.static_mesh_component
    if not sc or not sc.static_mesh: continue
    ce = str(sc.get_collision_enabled())
    if "NO_COLLISION" in ce: continue
    o, e, _r = unreal.SystemLibrary.get_component_bounds(sc)
    top = o.z + e.z
    if e.x * e.y < 2000: continue
    key = re.sub(r"[_]?\d+$", "", a.get_actor_label())
    groups[key].append(top)
for k in sorted(groups):
    v = groups[k]
    log("walk %-38s n=%3d top %.0f..%.0f" % (k, len(v), min(v), max(v)))
