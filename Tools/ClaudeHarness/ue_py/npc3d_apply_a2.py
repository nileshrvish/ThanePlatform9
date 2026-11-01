# Stage 5C (A2) map edit, editor world, NOT saved here:
#  1. the 7 placed NPCs use the A2 clips (/Game/P9/Characters/Station3D/<Name>/A2/SK_P9_NPC3D_<Name>_A2_<Clip>)
#  2. Specs (F3) sits on the SEAT side of the PF1 steel bench (seats face north/+Y, backrest on the south edge at y~-1300):
#     sit stop (1800,-1250) facing 90, approach point (1690,-1200) in front of the seats; the placed actor follows its start point.
# RESTORE = True (npc3d_restore_a2.py) puts back the first NPC3D clips and Specs' old stop.
import unreal
el = unreal.EditorAssetLibrary
RESTORE = globals().get("RESTORE", False)
ROOT = "/Game/P9/Characters/Station3D"
SLOTS = {"F1": "dhanraj", "F2": "Nilesh", "F3": "shivshankar", "F4": "Rohit", "F5": "rupesh", "CW": "Modi"}
FRIEND_PROPS = {"idle_anim": "Idle", "walk_anim": "Walk", "chat_anim": "Chat", "look_around_anim": "LookAround", "turn_anim": "TurnInPlace",
                "sit_down_anim": "SitDown", "sit_idle_anim": "SitIdle", "stand_up_anim": "StandUp", "gesture_anim": "Gesture", "pour_anim": "Pour"}
SIL_PROPS = {"idle_anim": "Idle", "walk_anim": "Walk", "menace_anim": "Menace", "talk_anim": "TalkAngry"}
SPECS_NEW = {0: ((1800.0, -1250.0, 0.0), 90.0), 1: ((1690.0, -1200.0, 0.0), None)}
SPECS_OLD = {0: ((1760.0, -1318.0, 0.0), -90.0), 1: ((1640.0, -1350.0, 0.0), None)}

def anim(n, clip):
    p = ("%s/%s/SK_P9_NPC3D_%s_Anim_%s" % (ROOT, n, n, clip)) if RESTORE else ("%s/%s/A2/SK_P9_NPC3D_%s_A2_%s" % (ROOT, n, n, clip))
    return el.load_asset(p) if el.does_asset_exist(p) else None

def apply(actor, props, n):
    actor.modify(); got = []
    for prop, clip in props.items():
        before = actor.get_editor_property(prop)
        a = anim(n, clip)
        if before is None and a is not None: continue          # the role does not use this slot
        if before is not None and a is None: raise RuntimeError("P9PY: missing %s clip %s" % (n, clip))
        actor.set_editor_property(prop, a)
        if a: got.append(clip)
    return "%s [%s]" % (actor.get_actor_label(), ",".join(got))

out = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        key = str(a.get_editor_property("friend_key"))
        if key not in SLOTS: continue
        out.append(apply(a, FRIEND_PROPS, SLOTS[key]))
        if key == "F3":
            route = list(a.get_editor_property("route"))
            for i, (loc, yaw) in (SPECS_OLD if RESTORE else SPECS_NEW).items():
                p = route[i]
                p.set_editor_property("location", unreal.Vector(*loc))
                if yaw is not None:
                    p.set_editor_property("use_face_yaw", True); p.set_editor_property("face_yaw", yaw)
                route[i] = p
            a.set_editor_property("route", route)
            s = route[a.get_editor_property("start_index")].get_editor_property("location")
            a.set_actor_location(unreal.Vector(s.x, s.y, s.z + 90.0), False, False)
            a.set_actor_rotation(unreal.Rotator(0.0, 0.0, (SPECS_OLD if RESTORE else SPECS_NEW)[0][1]), False)
            out.append("Specs sit stop -> %s" % str((SPECS_OLD if RESTORE else SPECS_NEW)[0]))
    elif isinstance(a, unreal.P9SilhouetteStalker):
        out.append(apply(a, SIL_PROPS, "kanchana"))
unreal.log("P9PY: npc3d_apply_a2%s: %s" % (" (RESTORE)" if RESTORE else "", " | ".join(out)))
