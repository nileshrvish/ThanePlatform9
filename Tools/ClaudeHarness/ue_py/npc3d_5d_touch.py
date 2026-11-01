# Stage 5D (editor; NOT saved): mark the friend/chaiwala actors modified so the map is re-saved without the obsolete
# empty HeldRight/HeldLeft default components (props are now created at runtime only). Reports what each NPC has.
import unreal
n = 0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        a.modify(); n += 1
        smc = [c.get_name() for c in a.get_components_by_class(unreal.StaticMeshComponent)]
        unreal.log("P9PY: %s static mesh comps=%s held_r=%s" % (a.get_actor_label(), smc, a.get_editor_property("held_mesh_right")))
unreal.log("P9PY: touched %d NPCs" % n)
