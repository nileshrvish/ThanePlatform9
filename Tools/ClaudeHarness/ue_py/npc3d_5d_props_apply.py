# Stage 5D (editor; map NOT saved): apply the hand-space prop offsets measured in PIE (pie_5d_handpose.py) to the chaiwala.
import unreal, json
J = json.load(open(r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\handpose_5d.json"))
cw = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == "S5B_Chaiwala")
cw.modify()
for side, prop in (("r", "held_offset_right"), ("l", "held_offset_left")):
    v = J[side]
    t = unreal.Transform(unreal.Vector(*v["loc"]), unreal.Rotator(roll=v["rot"][0], pitch=v["rot"][1], yaw=v["rot"][2]), unreal.Vector(1, 1, 1))
    cw.set_editor_property(prop, t)
unreal.log("P9PY: prop offsets applied: %s" % {k: (J[k]["loc"], J[k]["rot"]) for k in J})
