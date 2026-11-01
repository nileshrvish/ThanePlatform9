# Stage 5C read-only: every friend's route (points, stops, facing) - to place Specs' bench stop on the seat side.
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        r = a.get_editor_property("route")
        pts = []
        for i, p in enumerate(r):
            l = p.get_editor_property("location")
            pts.append("%d:(%.0f,%.0f,%.0f) %s%s" % (i, l.x, l.y, l.z, str(p.get_editor_property("stop")).split(".")[-1].split(":")[0],
                       (" yaw%.0f" % p.get_editor_property("face_yaw")) if p.get_editor_property("use_face_yaw") else ""))
        l = a.get_actor_location()
        unreal.log("P9PY: %s start=%d actor=(%.0f,%.0f,%.0f) yaw=%.0f | %s" % (a.get_actor_label(), a.get_editor_property("start_index"), l.x, l.y, l.z, a.get_actor_rotation().yaw, " ; ".join(pts)))
