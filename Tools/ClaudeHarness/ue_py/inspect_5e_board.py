# Stage 5E read-only: every actor using the Thane station-name material, its components, bounds and the floor under it.
import unreal
world = unreal.UnrealEditorSubsystem().get_editor_world()
mi = unreal.EditorAssetLibrary.load_asset("/Game/P9/Environment/StationSignage/MI_P9_StationName_Thane")
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    comps = a.get_components_by_class(unreal.StaticMeshComponent)
    uses = [c for c in comps if any(c.get_material(i) == mi for i in range(c.get_num_materials()))]
    if not uses and "Thane" not in a.get_actor_label() and "StationName" not in a.get_actor_label(): continue
    o, e = a.get_actor_bounds(False, True); l = a.get_actor_location()
    unreal.log("P9PY: board %s cls=%s loc=(%.0f,%.0f,%.0f) rot=%s bounds o=(%.0f,%.0f,%.0f) e=(%.0f,%.0f,%.0f) folder=%s" % (a.get_actor_label(), a.get_class().get_name(), l.x, l.y, l.z, a.get_actor_rotation(), o.x, o.y, o.z, e.x, e.y, e.z, a.get_folder_path()))
    for c in comps:
        cl = c.get_world_location()
        unreal.log("P9PY:   comp %s mesh=%s at (%.0f,%.0f,%.0f) scale=%s" % (c.get_name(), c.static_mesh.get_name() if c.static_mesh else None, cl.x, cl.y, cl.z, c.get_world_scale()))
    # what is under it
    hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(o.x, o.y, o.z - e.z - 1), unreal.Vector(o.x, o.y, -500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [a], unreal.DrawDebugTrace.NONE, True)
    if hit:
        t = hit.to_tuple(); unreal.log("P9PY:   floor below at z=%.0f (%s), gap %.0f cm" % (t[4].z, t[9].get_actor_label() if t[9] else "?", (o.z - e.z) - t[4].z))
