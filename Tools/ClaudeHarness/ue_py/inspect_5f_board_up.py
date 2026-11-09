# Stage 5F read-only: the ceiling/canopy above each Thane board (trace up from the board top at both rod positions).
import unreal
world = unreal.UnrealEditorSubsystem().get_editor_world()
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
boards = [a for a in sub.get_all_level_actors() if a.get_actor_label().startswith("S4B_NameBoard_Thane_")]
ign = boards + [a for a in sub.get_all_level_actors() if a.get_actor_label().startswith("S5E_NameBoardPost_")]
for b in boards:
    o, e = b.get_actor_bounds(False, True)
    for dx in (-60, 0, 60):
        for dy in (0, -150, 150):
            s = unreal.Vector(o.x + dx, o.y + dy, o.z + e.z + 1)
            hit = unreal.SystemLibrary.line_trace_single(world, s, s + unreal.Vector(0, 0, 1500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ign, unreal.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            if dy == 0 or t:
                unreal.log("P9PY: %s dx%+d dy%+d top %.0f -> %s" % (b.get_actor_label(), dx, dy, o.z + e.z, ("hit z=%.0f %s" % (t[4].z, t[9].get_actor_label())) if t else "nothing above (open sky)"))
