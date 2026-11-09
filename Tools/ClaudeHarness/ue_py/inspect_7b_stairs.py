# Stage 7B (read-only): station stair geometry. Lists stair-like actors and samples the floor (Visibility trace, like the NPCs)
# along each flight's centre line every 1 cm, then reports the step rise/tread/nosing positions.
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
out = []
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if "stair" in l.lower() or "step" in l.lower() or "landing" in l.lower():
        o, e = a.get_actor_bounds(False)
        sm = ""
        if isinstance(a, unreal.StaticMeshActor):
            m = a.static_mesh_component.static_mesh
            sm = m.get_name() if m else ""
            if m:
                bs = m.get_editor_property("body_setup")
                sm += " ctf=%s" % (str(bs.get_editor_property("collision_trace_flag")).split(".")[-1] if bs else "?")
        out.append("%s @%s ext%s %s" % (l, [round(v) for v in (o.x, o.y, o.z)], [round(v) for v in (e.x, e.y, e.z)], sm))
unreal.log("P9PY: stair actors (%d):\n" % len(out) + "\n".join(out))

def profile(name, x0, x1, y, ztop):
    zs = []
    n = int(abs(x1 - x0))
    s = 1 if x1 > x0 else -1
    for i in range(n + 1):
        x = x0 + s * i
        h = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, y, ztop), unreal.Vector(x, y, ztop - 1200),
                                                   unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
        zs.append(round(h.to_tuple()[4].z, 1) if h and h.to_tuple()[0] else None)
    # step edges
    edges = []
    for i in range(1, len(zs)):
        if zs[i] is not None and zs[i - 1] is not None and abs(zs[i] - zs[i - 1]) > 3:
            edges.append((x0 + s * i, zs[i - 1], zs[i]))
    unreal.log("P9PY: %s y=%d: z(%d)=%s z(%d)=%s, %d edges: %s" % (name, y, x0, zs[0], x1, zs[-1], len(edges), edges[:60]))

profile("PF1", -1500, -100, -1525, 900)
profile("PF910", 200, 1500, 7412, 1200)
