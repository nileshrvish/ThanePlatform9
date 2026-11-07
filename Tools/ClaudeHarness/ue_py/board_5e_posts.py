# Stage 5E (editor; NOT saved): the four Thane name boards (SM_P9_FOBSign, a hanging sign) stood in mid-air 3.25 m above
# the platforms. Give each board two steel posts from the platform to the top of its hanger rods (Indian Railways style).
# New actors only (S5E_NameBoardPost_*), in the boards' folder; the boards themselves are not moved.
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.UnrealEditorSubsystem().get_editor_world()
cyl = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cylinder")       # 100 cm tall, 100 cm diameter, pivot at centre
# steel: reuse the material of a canopy column on PF1 if there is one, else the chaiwala steel
mat = None
for a in sub.get_all_level_actors():
    lab = a.get_actor_label()
    if ("Column" in lab or "Pillar" in lab) and "PF1" in lab:
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        if c and c.get_num_materials(): mat = c.get_material(0); unreal.log("P9PY: post material from %s: %s" % (lab, mat.get_name())); break
if not mat: mat = unreal.EditorAssetLibrary.load_asset("/Game/P9/Characters/Materials/MI_P9_ChaiwalaSteel")
existing = {a.get_actor_label() for a in sub.get_all_level_actors()}
made = []
for b in [a for a in sub.get_all_level_actors() if a.get_actor_label().startswith("S4B_NameBoard_Thane_")]:
    o, e = b.get_actor_bounds(False, True)
    top = o.z + e.z
    for side, sx in (("L", -1), ("R", 1)):
        name = "S5E_NameBoardPost_%s_%s" % (b.get_actor_label().replace("S4B_NameBoard_Thane_", ""), side)
        if name in existing: raise RuntimeError("P9PY: exists " + name)
        x = o.x + sx * (e.x + 4.0)                      # just outside the board edge
        hit = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(x, o.y, o.z - e.z - 1), unreal.Vector(x, o.y, -500), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [b], unreal.DrawDebugTrace.NONE, True)
        floor = hit.to_tuple()[4].z if hit else 0.0
        h = top + 2.0 - floor
        p = sub.spawn_actor_from_object(cyl, unreal.Vector(x, o.y, floor + h / 2.0), unreal.Rotator(0, 0, 0))
        p.set_actor_label(name); p.set_folder_path(b.get_folder_path())
        p.set_actor_scale3d(unreal.Vector(0.07, 0.07, h / 100.0))      # 7 cm steel post
        c = p.static_mesh_component; c.set_material(0, mat); c.set_mobility(unreal.ComponentMobility.STATIC)
        c.set_collision_profile_name("BlockAll")
        made.append("%s h=%.0f floor=%.0f" % (name, h, floor))
unreal.log("P9PY: board posts %d: %s" % (len(made), " | ".join(made)))
