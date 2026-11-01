# Stage 5C read-only: the bench Specs sits on, and the trains (label, location, auto cycle, stops, seats).
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    l = a.get_actor_location()
    if abs(l.x - 1800) < 250 and abs(l.y + 1300) < 250 and l.z < 200:
        o, e = a.get_actor_bounds(False, True)
        meshes = [c.static_mesh.get_name() for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
        unreal.log("P9PY: near-bench %s cls=%s loc=(%.0f,%.0f,%.0f) yaw=%.1f bounds o=(%.0f,%.0f,%.0f) e=(%.0f,%.0f,%.0f) meshes=%s" % (
            a.get_actor_label(), a.get_class().get_name(), l.x, l.y, l.z, a.get_actor_rotation().yaw, o.x, o.y, o.z, e.x, e.y, e.z, meshes[:3]))
    if isinstance(a, unreal.P9Train):
        unreal.log("P9PY: train %s (%s) loc=(%.0f,%.0f,%.0f) auto=%s stops=%s start=%d side=%d" % (a.get_actor_label(), a.get_name(), l.x, l.y, l.z,
            a.get_editor_property("auto_cycle"), [round(s) for s in a.get_editor_property("stop_distances")], a.get_editor_property("start_stop_index"), a.get_editor_property("platform_side_world_y")))
