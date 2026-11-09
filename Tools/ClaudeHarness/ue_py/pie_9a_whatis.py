import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.StaticMeshActor):
    if a.get_name() in ("StaticMeshActor_1162", "StaticMeshActor_1164") or a.get_name().startswith("StaticMeshActor_11"):
        c = a.static_mesh_component
        o, e = a.get_actor_bounds(False)
        unreal.log("P9PY: %s label=%s mesh=%s loc=%s ctr=(%.0f %.0f %.0f) ext=(%.0f %.0f %.0f) hidden=%s coll=%s owner=%s tags=%s" % (a.get_name(), a.get_actor_label() if hasattr(a, "get_actor_label") else "",
                   c.static_mesh.get_name() if c.static_mesh else None, a.get_actor_location(), o.x, o.y, o.z, e.x, e.y, e.z, a.get_editor_property("hidden"), c.get_collision_enabled(), a.get_owner(), a.tags))
