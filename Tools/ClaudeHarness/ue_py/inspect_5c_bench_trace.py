# Stage 5C read-only: find which side of the PF1 steel bench (1800,-1275) has the backrest, by tracing across it
# at seat height (45) and backrest height (75) from both sides. Also the PF2/3 stairs (any actor labelled Stair + PF2).
import unreal
world = unreal.UnrealEditorSubsystem().get_editor_world()
def tr(z, y0, y1):
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(1800, y0, z), unreal.Vector(1800, y1, z),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit: return None
    t = hit.to_tuple()
    return "%.1f(%s)" % (t[4].y, t[9].get_actor_label() if t[9] else "?")   # impact point y, hit actor
for z in (20, 45, 60, 75, 85):
    unreal.log("P9PY: bench z=%d from north->south hit y=%s | from south->north hit y=%s" % (z, tr(z, -1150, -1400), tr(z, -1400, -1150)))
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
st = [a.get_actor_label() for a in sub.get_all_level_actors() if "Stair" in a.get_actor_label() and ("PF2" in a.get_actor_label() or "PF3" in a.get_actor_label())]
unreal.log("P9PY: PF2/3 stairs actors: %s" % st[:10])
