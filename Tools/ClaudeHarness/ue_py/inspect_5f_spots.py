import unreal
world = unreal.UnrealEditorSubsystem().get_editor_world()
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
spots = {"PF1_W": (-1925, -1080), "PF1_E": (1000, -1080), "PF9_W": (-1000, 7080), "PF9_E": (1930, 7080)}
for k, (x, y) in spots.items():
    near = []
    for a in sub.get_all_level_actors():
        o, e = a.get_actor_bounds(False, True)
        if abs(o.x - x) < e.x + 110 and abs(o.y - y) < e.y + 30 and o.z + e.z > 300 and o.z - e.z < 440 and not a.get_actor_label().startswith(("S4B_NameBoard", "S5E_NameBoardPost", "RD_Canopy", "S3C_Canopy")) and e.z < 600:
            near.append(a.get_actor_label())
    for dx in (-70, 70):
        s = unreal.Vector(x + dx, y, 390)
        hit = unreal.SystemLibrary.line_trace_single(world, s, s + unreal.Vector(0, 0, 400), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        unreal.log("P9PY: %s rod dx%+d ceiling %s" % (k, dx, ("%.1f %s" % (t[4].z, t[9].get_actor_label())) if t else "NONE"))
    unreal.log("P9PY: %s at (%d,%d) nearby z300-440: %s" % (k, x, y, near[:8]))
