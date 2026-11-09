import unreal
w = unreal.UnrealEditorSubsystem().get_game_world()
for y in (-1450.0, -1420.0, -1600.0):
    zs = []
    for x in range(-900, -650, 10):
        h = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, 420), unreal.Vector(x, y, -50), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True).to_tuple()
        zs.append((x, round(h[4].z) if h[0] else None, h[9].get_name() if h[0] and h[9] else ""))
    unreal.log("P9PY: lane y%.0f landing: %s" % (y, [(x, z) for x, z, n in zs]))
    unreal.log("P9PY: lane y%.0f actors: %s" % (y, sorted(set(n for x, z, n in zs))))
