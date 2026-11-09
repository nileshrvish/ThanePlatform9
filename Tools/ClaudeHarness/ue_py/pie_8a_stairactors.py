# Stage 8A (PIE, read-only): every actor/primitive in the PF9/10 stair region (x 180..1420, y 7230..7590, z -5..620) of the PIE world,
# with mesh, materials, collision and bounds - the stair geometry lives partly inside the S2 island level instance.
import unreal
world = None
for w in unreal.EditorLevelLibrary.get_pie_worlds(False) if hasattr(unreal.EditorLevelLibrary, "get_pie_worlds") else []:
    world = w
if world is None:
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
acts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
rows = []
for a in acts:
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        try:
            o, e, r = unreal.SystemLibrary.get_component_bounds(c)
        except Exception:
            continue
        lo, hi = o - e, o + e
        if hi.x < 180 or lo.x > 1420 or hi.y < 7230 or lo.y > 7590 or hi.z < -5 or lo.z > 620 or e.x > 2000:
            continue
        mesh = c.static_mesh.get_name() if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh else c.get_class().get_name()
        mats = []
        if isinstance(c, unreal.MeshComponent):
            mats = [m.get_name() if m else None for m in c.get_materials()]
        rows.append("%s | %s | %s | %s | coll=%s vis=%s hig=%s | lo(%.0f,%.0f,%.1f) hi(%.0f,%.0f,%.1f)" % (
            a.get_name(), a.get_actor_label() if hasattr(a, "get_actor_label") else "", mesh, mats, c.get_collision_profile_name(), c.is_visible(),
            a.get_editor_property("hidden"), lo.x, lo.y, lo.z, hi.x, hi.y, hi.z))
for r in sorted(rows):
    unreal.log("P9PY: " + r)
