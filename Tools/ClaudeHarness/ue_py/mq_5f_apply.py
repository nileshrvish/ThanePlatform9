# Stage 5F (editor; NOT saved): the 7 placed NPCs use the Mannequin-rigged meshes (MQ2) and their MQ2 clips
# (Unreal mocap idle/walk + the moved sit/talk/gesture clips). Director's static epilogue proxy untouched (invisible).
import unreal
el = unreal.EditorAssetLibrary; ROOT = "/Game/P9/Characters/Station3D"
SLOTS = {"F1": "dhanraj", "F2": "Nilesh", "F3": "shivshankar", "F4": "Rohit", "F5": "rupesh", "CW": "Modi"}
FP = {"idle_anim": "Idle", "walk_anim": "Walk", "chat_anim": "Chat", "look_around_anim": "LookAround", "turn_anim": "TurnInPlace",
      "sit_down_anim": "SitDown", "sit_idle_anim": "SitIdle", "stand_up_anim": "StandUp", "gesture_anim": "Gesture", "pour_anim": "Pour"}
SP = {"idle_anim": "Idle", "walk_anim": "Walk", "menace_anim": "Menace", "talk_anim": "TalkAngry"}
def a(n, c):
    # sit clips built on the Mannequin rig itself (MQ2S: hands palm-down on the thighs); everything else MQ2
    if c in ("SitDown", "SitIdle", "StandUp"):
        p = "%s/%s/MQ2S/SK_P9_NPC3D_%s_MQ2S_%s" % (ROOT, n, n, c)
        if el.does_asset_exist(p): return el.load_asset(p)
    p = "%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2_%s" % (ROOT, n, n, c)
    return el.load_asset(p) if el.does_asset_exist(p) else None
out = []
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, unreal.P9FriendNPC) and str(actor.get_editor_property("friend_key")) in SLOTS:
        n = SLOTS[str(actor.get_editor_property("friend_key"))]; props = FP; meshprop = "friend_mesh"
    elif isinstance(actor, unreal.P9SilhouetteStalker):
        n = "kanchana"; props = SP; meshprop = "stalker_mesh"
    else: continue
    mesh = el.load_asset("%s/%s/MQ2/SK_P9_NPC3D_%s_MQ2" % (ROOT, n, n))
    actor.modify(); actor.set_editor_property(meshprop, mesh); got = []
    for prop, clip in props.items():
        before = actor.get_editor_property(prop); new = a(n, clip)
        if before is None and new is None: continue
        if before is not None and new is None: raise RuntimeError("P9PY: missing MQ2 %s %s" % (n, clip))
        actor.set_editor_property(prop, new); got.append(clip)
    actor.get_editor_property("body").set_skeletal_mesh_asset(mesh)
    out.append("%s->%s[%s]" % (actor.get_actor_label(), mesh.get_name(), ",".join(got)))
unreal.log("P9PY: mq2 applied: " + " | ".join(out))
