# Stage 5D (PIE, read-only): with the chaiwala's real idle pose evaluated, compute the hand-space offsets that keep the
# ketli hanging upright by its handle (spout forward) and the glass upright in the left palm. Writes JSON for the
# editor-side script (npc3d_5d_props_apply.py); changes nothing.
import unreal, json
OUT = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\handpose_5d.json"
world = unreal.UnrealEditorSubsystem().get_game_world()
cw = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "CW")
body = cw.get_editor_property("body")
yaw = cw.get_actor_rotation().yaw
res = {}
for side, palm, up, inward in (("r", 7.0, 0.0, 1.0), ("l", 9.5, 0.0, -1.5)):
    H = body.get_socket_transform("hand_" + side, unreal.RelativeTransformSpace.RTS_WORLD)
    L = body.get_socket_transform("lowerarm_" + side, unreal.RelativeTransformSpace.RTS_WORLD)
    d = H.translation - L.translation; d = d / max(1e-3, d.length())
    c = cw.get_actor_location(); iv = unreal.Vector(c.x - H.translation.x, c.y - H.translation.y, 0.0); iv = iv / max(1e-3, iv.length())
    loc = H.translation + d * palm + unreal.Vector(0, 0, up) + iv * inward
    rel = unreal.Transform(loc, unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(1, 1, 1)).make_relative(H)
    r = rel.rotation.rotator()
    res[side] = {"loc": [rel.translation.x, rel.translation.y, rel.translation.z], "rot": [r.roll, r.pitch, r.yaw], "hand_z": H.translation.z, "dir": [d.x, d.y, d.z]}
anim = cw.get_editor_property("idle_anim")
json.dump(res, open(OUT, "w"), indent=1)
unreal.log("P9PY: handpose %s" % json.dumps(res))
