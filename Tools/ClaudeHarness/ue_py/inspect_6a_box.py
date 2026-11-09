# Stage 6A read-only: every actor whose bounds intersect a box (editor world). Box from BOX below. Modifies nothing.
import unreal

BOXES = {"pf45": ((150, 2060, 560), (700, 2440, 900)), "pf67": ((150, 3880, 560), (700, 4260, 900)), "pf8": ((150, 5740, 560), (700, 6120, 900))}

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for key, (lo, hi) in BOXES.items():
    for a in sub.get_all_level_actors():
        o, e = a.get_actor_bounds(False, True)
        mn = o - e
        mx = o + e
        if mx.x < lo[0] or mn.x > hi[0] or mx.y < lo[1] or mn.y > hi[1] or mx.z < lo[2] or mn.z > hi[2]:
            continue
        if e.x > 3000 or e.y > 3000:
            continue
        parts = []
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            m = c.static_mesh
            n = c.get_instance_count() if isinstance(c, unreal.InstancedStaticMeshComponent) else 1
            parts.append("%s:%s x%d %s coll=%s" % (c.get_name(), m.get_name() if m else None, n,
                         [x.get_name() if x else None for x in c.get_materials()], str(c.get_collision_enabled()).split(".")[-1].split(":")[0]))
        unreal.log("P9PY: [%s] %s (%s) hid=%s loc=%s yaw=%.0f min=%s max=%s %s" % (key, a.get_actor_label(), a.get_class().get_name(), a.get_editor_property("hidden"),
                   a.get_actor_location(), a.get_actor_rotation().yaw, [round(v) for v in (mn.x, mn.y, mn.z)], [round(v) for v in (mx.x, mx.y, mx.z)], " ".join(parts)))
