# Stage 9A (PIE, read-only): why are Specs / Chaiwala held? Floor traces (as P9NPCLoco::FloorAt: from feet+60 down) and chest-height
# sweeps (as MoveHitsWorld) along the line from the NPC to its remaining target.
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
cases = {"S5B_Friend_F3_Specs": unreal.Vector(1800, -1250, 0), "S5B_Chaiwala": unreal.Vector(512, 3650, 600)}
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9FriendNPC):
    lab = a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name()
    key = next((k for k in cases if k in lab or lab in k), None)
    if not key:
        continue
    p = a.get_actor_location(); feet = p.z - 90.0; t = cases[key]
    unreal.log("P9PY: %s at (%.0f %.0f %.0f) -> (%.0f %.0f)" % (lab, p.x, p.y, feet, t.x, t.y))
    for i in range(0, 11):
        f = i / 10.0
        x, y = p.x + (t.x - p.x) * f, p.y + (t.y - p.y) * f
        hits = unreal.SystemLibrary.line_trace_multi(w, unreal.Vector(x, y, feet + 60), unreal.Vector(x, y, feet - 250), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [a], unreal.DrawDebugTrace.NONE, True)
        hs = []
        for h in hits or []:
            t2 = h.to_tuple()
            hs.append("%s@%.0f" % (t2[9].get_name() if t2[9] else "?", t2[4].z))
        unreal.log("P9PY:   f=%.1f (%.0f %.0f) first-hits %s" % (f, x, y, hs[:3]))
    # chest-height sweep from the NPC towards the target
    o = unreal.Vector(p.x, p.y, feet + 100); e = unreal.Vector(t.x, t.y, feet + 100)
    hit = unreal.SystemLibrary.sphere_trace_single(w, o, e, 22.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [a], unreal.DrawDebugTrace.NONE, True)
    if hit:
        t3 = hit.to_tuple()
        unreal.log("P9PY:   chest sweep blocked by %s at (%.0f %.0f)" % (t3[9].get_name() if t3[9] else "?", t3[4].x, t3[4].y))
    else:
        unreal.log("P9PY:   chest sweep clear")
