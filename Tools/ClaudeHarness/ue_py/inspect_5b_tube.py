# Stage 5B read-only: tube fixture bounds (where the glowing tube actually hangs) for footbridge/concourse/platform tubes.
import unreal
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    l = a.get_actor_label()
    if l in ("S3B3_TubeLight_FOB_430", "S3B3_TubeLight_Conc_Stall", "S3B3_TubeLight_PF1_E1"):
        origin, ext = a.get_actor_bounds(False, True)
        sm = a.get_component_by_class(unreal.StaticMeshComponent)
        mats = [m.get_name() if m else None for m in sm.get_materials()]
        unreal.log("P9PY: %s pivot=%s scale=%s bounds z %.1f..%.1f x/y ext %.1f/%.1f mats=%s" % (l, a.get_actor_location(), a.get_actor_scale3d(),
                   origin.z - ext.z, origin.z + ext.z, ext.x, ext.y, mats))
