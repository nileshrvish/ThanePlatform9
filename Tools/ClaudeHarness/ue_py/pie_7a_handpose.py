# Stage 7A (PIE, read-only): copy of pie_5d_handpose.py for the chaiwala's Mixamo rig (Mixamo hand/forearm bones; Unreal strips the "mixamorig:" prefix on import).
# With his real idle pose evaluated, computes the hand-space offsets that keep the ketli hanging upright by its handle (spout
# forward) and the glass upright in the left palm. Writes JSON for mx7a_props_apply.py; changes nothing.
import unreal, json
OUT = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad\handpose_7a.json"
world = unreal.UnrealEditorSubsystem().get_game_world()
cw = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "CW")
body = cw.get_editor_property("body")
yaw = cw.get_actor_rotation().yaw
res = {}
for side, palm, up, inward in (("r", 7.0, 0.0, 1.0), ("l", 9.5, 0.0, -1.5)):
    S = "Right" if side == "r" else "Left"
    H = body.get_socket_transform("%sHand" % S, unreal.RelativeTransformSpace.RTS_WORLD)
    L = body.get_socket_transform("%sForeArm" % S, unreal.RelativeTransformSpace.RTS_WORLD)
    d = H.translation - L.translation; d = d / max(1e-3, d.length())
    c = cw.get_actor_location(); iv = unreal.Vector(c.x - H.translation.x, c.y - H.translation.y, 0.0); iv = iv / max(1e-3, iv.length())
    loc = H.translation + d * palm + unreal.Vector(0, 0, up) + iv * inward
    rel = unreal.Transform(loc, unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1, 1, 1)).make_relative(H)
    r = rel.rotation.rotator()
    res[side] = {"loc": [rel.translation.x, rel.translation.y, rel.translation.z], "rot": [r.roll, r.pitch, r.yaw], "hand_z": H.translation.z, "dir": [d.x, d.y, d.z]}
json.dump(res, open(OUT, "w"), indent=1)
unreal.log("P9PY: handpose %s activity clip idle=%s" % (json.dumps(res), cw.get_editor_property("idle_anim").get_name()))
