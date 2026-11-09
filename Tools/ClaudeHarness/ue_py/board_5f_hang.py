# Stage 5F (editor; NOT saved): Thane name boards hang from the canopy ceiling.
#  - removes the 8 Stage 5E floor posts (S5E_NameBoardPost_*), as the user asked
#  - PF1_W and PF9_E stood in the canopy gaps above the stairs (open sky): moved under the neighbouring canopy
#  - every board gets two 3 cm steel hanger rods from its top to the ceiling found by an upward trace
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.UnrealEditorSubsystem().get_editor_world()
cyl = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cylinder")
steel = unreal.EditorAssetLibrary.load_asset("/Game/P9/Environment/Architecture/MI_P9_SteelBlue") or None
actors = sub.get_all_level_actors()
if not steel:
    col = next(a for a in actors if a.get_actor_label() == "S3C_Column_PF1_-2400"); steel = col.get_component_by_class(unreal.StaticMeshComponent).get_material(0)
posts = [a for a in actors if a.get_actor_label().startswith("S5E_NameBoardPost_")]
for p in posts: sub.destroy_actor(p)
MOVE = {"S4B_NameBoard_Thane_PF1_W": -1925.0, "S4B_NameBoard_Thane_PF9_E": 1930.0}
labels = {a.get_actor_label() for a in sub.get_all_level_actors()}
out = ["removed %d posts" % len(posts)]
for b in [a for a in sub.get_all_level_actors() if a.get_actor_label().startswith("S4B_NameBoard_Thane_")]:
    lab = b.get_actor_label(); b.modify()
    if lab in MOVE:
        l = b.get_actor_location(); b.set_actor_location(unreal.Vector(MOVE[lab], l.y, l.z), False, False)
    o, e = b.get_actor_bounds(False, True); top = o.z + e.z
    for side, sx in (("L", -1), ("R", 1)):
        name = "S5F_NameBoardHanger_%s_%s" % (lab.replace("S4B_NameBoard_Thane_", ""), side)
        if name in labels: raise RuntimeError("P9PY: exists " + name)
        x = o.x + sx * (e.x - 12.0)
        s = unreal.Vector(x, o.y, top + 1)
        hit = unreal.SystemLibrary.line_trace_single(world, s, s + unreal.Vector(0, 0, 400), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [b], unreal.DrawDebugTrace.NONE, True)
        if not hit: raise RuntimeError("P9PY: no ceiling above " + lab)
        ceil = hit.to_tuple()[4].z; h = ceil - top + 4.0
        r = sub.spawn_actor_from_object(cyl, unreal.Vector(x, o.y, top - 2.0 + h / 2.0), unreal.Rotator(0, 0, 0))
        r.set_actor_label(name); r.set_folder_path(b.get_folder_path()); r.set_actor_scale3d(unreal.Vector(0.03, 0.03, h / 100.0))
        c = r.static_mesh_component; c.set_material(0, steel); c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION); c.set_cast_shadow(False)
        out.append("%s %.0f cm to z %.0f" % (name, h, ceil))
    out.append("%s at x %.0f" % (lab, b.get_actor_location().x))
unreal.log("P9PY: boards hang: " + " | ".join(out))
