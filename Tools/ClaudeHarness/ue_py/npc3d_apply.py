# Stage 5C map edit (editor world, NOT saved here): point the 7 placed NPCs at the new scanned NPC3D assets.
# Pass RESTORE=True (npc3d_restore.py) to put the 5A v2 assets back. Only asset properties change; routes/roles/flags stay.
import unreal
el = unreal.EditorAssetLibrary
RESTORE = globals().get("RESTORE", False)
NEW = "/Game/P9/Characters/Station3D"
OLD = "/Game/P9/Characters/Station"
# friend_key -> (new name, old folder/name)
SLOTS = {"F1": ("dhanraj", "F1_Curly"), "F2": ("Nilesh", "F2_Stripe"), "F3": ("shivshankar", "F3_Specs"), "F4": ("Rohit", "F4_Selfie"),
         "F5": ("rupesh", "F5_Backpack"), "CW": ("Modi", "Chaiwala_VG")}
SIL = ("kanchana", "Silhouette")
FRIEND_PROPS = {"idle_anim": "Idle", "walk_anim": "Walk", "chat_anim": "Chat", "look_around_anim": "LookAround", "turn_anim": "TurnInPlace",
                "sit_down_anim": "SitDown", "sit_idle_anim": "SitIdle", "stand_up_anim": "StandUp", "gesture_anim": "Gesture", "pour_anim": "Pour"}
SIL_PROPS = {"idle_anim": "Idle", "walk_anim": "Walk", "menace_anim": "Menace", "talk_anim": "TalkAngry"}

def mesh_path(new, old):
    return ("%s/%s/SK_P9_%s" % (OLD, old, old)) if RESTORE else ("%s/%s/SK_P9_NPC3D_%s" % (NEW, new, new))
def anim_path(new, old, clip):
    return ("%s/%s/SK_P9_%s_Anim_%s" % (OLD, old, old, clip)) if RESTORE else ("%s/%s/SK_P9_NPC3D_%s_Anim_%s" % (NEW, new, new, clip))
def load_opt(p):
    return el.load_asset(p) if el.does_asset_exist(p) else None

def apply(actor, mesh_prop, props, new, old):
    m = el.load_asset(mesh_path(new, old))
    if not m: raise RuntimeError("P9PY: missing mesh " + mesh_path(new, old))
    actor.modify()
    actor.set_editor_property(mesh_prop, m)
    got = []
    for prop, clip in props.items():
        before = actor.get_editor_property(prop)
        a = load_opt(anim_path(new, old, clip))
        # only fill slots the role actually uses (a slot that was empty before stays empty)
        if before is None and a is not None and not RESTORE:
            continue
        if a is None and before is not None:
            raise RuntimeError("P9PY: missing clip %s for %s" % (clip, actor.get_actor_label()))
        actor.set_editor_property(prop, a)
        if a: got.append(clip)
    body = actor.get_editor_property("body")
    if body: body.set_skeletal_mesh_asset(m)
    return "%s -> %s [%s]" % (actor.get_actor_label(), m.get_name(), ",".join(got))

out = []
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        key = str(a.get_editor_property("friend_key"))
        if key in SLOTS:
            out.append(apply(a, "friend_mesh", FRIEND_PROPS, *SLOTS[key]))
    elif isinstance(a, unreal.P9SilhouetteStalker):
        out.append(apply(a, "stalker_mesh", SIL_PROPS, *SIL))
    elif isinstance(a, unreal.P9CampaignDirector):
        p = (OLD + "/Static/SM_P9_Chaiwala_VG") if RESTORE else (NEW + "/Static/SM_P9_NPC3D_Modi")
        sm = el.load_asset(p)
        a.modify(); a.set_editor_property("chaiwala_mesh", sm)
        out.append("%s chaiwala_mesh -> %s" % (a.get_actor_label(), sm.get_name()))
unreal.log("P9PY: npc3d_apply%s %d: %s" % (" (RESTORE)" if RESTORE else "", len(out), " | ".join(out)))
