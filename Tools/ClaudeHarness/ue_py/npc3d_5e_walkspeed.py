# Stage 5E (editor; NOT saved): the mocap walk (MX) covers ~127 cm/s on dhanraj (hip 0.890 m); the retargeter scales the
# stride by leg length, so each NPC's WalkSpeed is set to its clip's speed (feet stay planted). Silhouette: WalkClipSpeed.
import unreal
HIP = {"dhanraj": 0.890, "Nilesh": 0.864, "shivshankar": 0.849, "Rohit": 0.875, "rupesh": 0.906, "Modi": 0.854, "kanchana": 0.823}
SLOTS = {"F1": "dhanraj", "F2": "Nilesh", "F3": "shivshankar", "F4": "Rohit", "F5": "rupesh", "CW": "Modi"}
out = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC) and str(a.get_editor_property("friend_key")) in SLOTS:
        n = SLOTS[str(a.get_editor_property("friend_key"))]; v = round(127.0 * HIP[n] / 0.890, 1)
        a.modify(); a.set_editor_property("walk_speed", v); out.append("%s %.1f" % (n, v))
    elif isinstance(a, unreal.P9SilhouetteStalker):
        v = round(127.0 * HIP["kanchana"] / 0.890, 1); a.modify(); a.set_editor_property("walk_clip_speed", v); out.append("kanchana(clip) %.1f" % v)
unreal.log("P9PY: walk speeds: " + ", ".join(out))
