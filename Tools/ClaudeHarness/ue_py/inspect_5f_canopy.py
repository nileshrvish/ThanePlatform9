import unreal
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    l = a.get_actor_label()
    if "Canopy" in l and ("PF1" in l or "MOD_" in l or "PF9" in l):
        o, e = a.get_actor_bounds(False, True)
        unreal.log("P9PY: %s x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f hidden=%s" % (l, o.x - e.x, o.x + e.x, o.y - e.y, o.y + e.y, o.z - e.z, o.z + e.z, a.is_hidden_ed()))
