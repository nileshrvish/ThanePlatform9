# Stage 5D (editor; map NOT saved here): the chaiwala holds a ketli (right hand) and a chai glass (left hand), and gets
# his epilogue return path (Dadar local door -> Platform 1 -> PF1 stairs -> footbridge -> behind his counter).
# Offsets are computed from the hand bones in his idle pose so the ketli hangs upright by its handle (spout forward)
# and the glass stands upright in the palm.
import unreal
el = unreal.EditorAssetLibrary
RETURN = [(-1450, -1150, 0), (-1350, -1525, 0), (-50, -1525, 600), (0, -1250, 600), (0, 2380, 600), (0, 3000, 600),
          (300, 3350, 600), (520, 3460, 600), (512, 3650, 600)]
cw = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == "S5B_Chaiwala")
body = cw.get_editor_property("body")
body.set_animation(cw.get_editor_property("idle_anim")); body.set_position(0.0, False)
body.set_position(0.01, False)
yaw = cw.get_actor_rotation().yaw
def held(side, palm_cm, up_cm, inward_cm, desired_yaw):
    H = body.get_socket_transform("hand_" + side, unreal.RelativeTransformSpace.RTS_WORLD)
    L = body.get_socket_transform("lowerarm_" + side, unreal.RelativeTransformSpace.RTS_WORLD)
    d = H.translation - L.translation; d = d / max(1e-3, d.length())
    inward = unreal.Vector(0, 0, 0) - (H.translation - cw.get_actor_location()) * unreal.Vector(1, 1, 0)
    inward = inward / max(1e-3, inward.length())
    loc = H.translation + d * palm_cm + unreal.Vector(0, 0, up_cm) + inward * inward_cm
    desired = unreal.Transform(loc, unreal.Rotator(0.0, 0.0, desired_yaw), unreal.Vector(1, 1, 1))
    rel = desired.make_relative(H)
    unreal.log("P9PY: hand_%s world=%s dir=%s -> rel loc=%s rot=%s" % (side, H.translation, d, rel.translation, rel.rotation.rotator()))
    return rel
cw.modify()
cw.set_editor_property("held_mesh_right", el.load_asset("/Game/P9/Props/ChaiProps/SM_P9_Ketli"))
cw.set_editor_property("held_mesh_left", el.load_asset("/Game/P9/Props/ChaiProps/SM_P9_ChaiGlass"))
cw.set_editor_property("held_offset_right", held("r", 7.0, 0.0, 1.0, yaw))      # grip in the fingers, ketli hangs below
cw.set_editor_property("held_offset_left", held("l", 6.0, 2.0, 2.5, yaw))       # glass in the palm
cw.set_editor_property("epilogue_return_path", [unreal.Vector(*p) for p in RETURN])
cw.rerun_construction_scripts() if hasattr(cw, "rerun_construction_scripts") else None
unreal.log("P9PY: 5d chaiwala props + return path (%d points) set" % len(RETURN))
