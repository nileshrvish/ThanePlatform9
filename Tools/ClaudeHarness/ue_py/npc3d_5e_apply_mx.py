# Stage 5E (editor; NOT saved): idle + walk from the retargeted Mannequin mocap (MX) for all 7 NPCs; every other clip
# (sit, chat, gestures, pour, menace, turn) stays the A3 set. RESTORE=True puts the A3 idle/walk back.
import unreal
el = unreal.EditorAssetLibrary
RESTORE = globals().get("RESTORE", False)
ROOT = "/Game/P9/Characters/Station3D"
SLOTS = {"F1": "dhanraj", "F2": "Nilesh", "F3": "shivshankar", "F4": "Rohit", "F5": "rupesh", "CW": "Modi"}
def clip(n, c):
    p = ("%s/%s/A3/SK_P9_NPC3D_%s_A3_%s" if RESTORE else "%s/%s/MX/SK_P9_NPC3D_%s_MX_%s") % (ROOT, n, n, c)
    a = el.load_asset(p)
    if not a: raise RuntimeError("P9PY: missing " + p)
    return a
out = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    n = None
    if isinstance(a, unreal.P9FriendNPC): n = SLOTS.get(str(a.get_editor_property("friend_key")))
    elif isinstance(a, unreal.P9SilhouetteStalker): n = "kanchana"
    if not n: continue
    a.modify()
    a.set_editor_property("idle_anim", clip(n, "Idle")); a.set_editor_property("walk_anim", clip(n, "Walk"))
    out.append(a.get_actor_label())
unreal.log("P9PY: 5e %s idle/walk on %d NPCs: %s" % ("A3" if RESTORE else "MX mocap", len(out), ", ".join(out)))
