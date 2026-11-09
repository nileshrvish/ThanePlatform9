# Stage 8A (PIE, read-only): where Kanchana stands, and whether her body (r 25, half height 82) overlaps anything solid there
# (world geometry, trains, NPC bodies, the player); nearest blocking surface distance by 8 horizontal traces.
import unreal, math
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
k = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9SilhouetteStalker)[0]
loc = k.get_actor_location()
hidden = k.get_editor_property("hidden") if hasattr(k, "get_editor_property") else None
centre = loc + unreal.Vector(0, 0, 84)
hits = unreal.SystemLibrary.capsule_overlap_actors(world, centre, 25.0, 82.0,
    [unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2, unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY3], None, [k])
hits = (hits[1] if isinstance(hits, tuple) else hits) or []
names = [h.get_actor_label() if hasattr(h, "get_actor_label") else h.get_name() for h in hits]
near = []
for i in range(8):
    a = math.radians(i * 45)
    d = unreal.Vector(math.cos(a), math.sin(a), 0)
    r = unreal.SystemLibrary.line_trace_single(world, centre, centre + d * 300.0, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [k], unreal.DrawDebugTrace.NONE, True)
    t = r.to_tuple() if r else None
    near.append(round(t[3], 0) if t and t[0] else 300)
fr = unreal.SystemLibrary.line_trace_single(world, centre, centre - unreal.Vector(0, 0, 300), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [k], unreal.DrawDebugTrace.NONE, True)
floor = fr.to_tuple() if fr else (False,)
unreal.log("P9PY: Kanchana at %s yaw %.0f hidden=%s; overlapping %s; clear distance by 45-degree ray (cm) %s; floor z %s" % (
    [round(v) for v in (loc.x, loc.y, loc.z)], k.get_actor_rotation().yaw, hidden, names or "nothing", near, round(floor[4].z, 1) if floor[0] else None))
