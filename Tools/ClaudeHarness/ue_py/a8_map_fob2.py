# Stage 8A (editor; not saved): LOOP_FOB also lists the deck B parapet collision blocks (hidden, collision kept, while engaged).
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
acts = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
p = next(a for a in acts.values() if isinstance(a, unreal.P9AnomalySpawnPoint) and str(a.get_editor_property("point_id")) == "LOOP_FOB")
ex = list(p.get_editor_property("extra_actors"))
for l in ("S2_FOB_Parapet_W_B", "S2_FOB_Parapet_E_4", "S2_FOB_Parapet_E_5"):
    if acts[l] not in ex:
        ex.append(acts[l])
p.set_editor_property("extra_actors", ex)
unreal.log("P9PY: LOOP_FOB extras: %s" % [a.get_actor_label() for a in ex])
