# Stage 5D (editor; map NOT saved here):
#  - props material MI_P9_ChaiProps (child of M_P9_NPC3D) on the ketli and glass; texture settings; assets saved
#  - the 7 placed NPCs use the A3 clips; the chaiwala also gets his new Walk clip
#  - read-only report: stall / vendor layout and the chaiwala's hand sockets (to place props and the return path)
import unreal
el = unreal.EditorAssetLibrary; mel = unreal.MaterialEditingLibrary; at = unreal.AssetToolsHelpers.get_asset_tools()
PROPS = "/Game/P9/Props/ChaiProps"; ROOT = "/Game/P9/Characters/Station3D"
saved = []
def tex(k): return el.load_asset("%s/T_P9_ChaiProps_%s" % (PROPS, k))
n = tex("Normal"); n.modify(); n.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP); n.set_editor_property("srgb", False)
r = tex("Roughness"); r.modify(); r.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS); r.set_editor_property("srgb", False)
for t in (n, r, tex("BaseColor")):
    t.set_editor_property("filter", unreal.TextureFilter.TF_NEAREST)      # flat colour cells, no bleeding
    saved.append(t)
mip = PROPS + "/MI_P9_ChaiProps"
mi = el.load_asset(mip) if el.does_asset_exist(mip) else at.create_asset("MI_P9_ChaiProps", PROPS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
mel.set_material_instance_parent(mi, el.load_asset(ROOT + "/Materials/M_P9_NPC3D"))
for k in ("BaseColor", "Normal", "Roughness"): mel.set_material_instance_texture_parameter_value(mi, k, tex(k))
mel.update_material_instance(mi); saved.append(mi)
for m in ("SM_P9_Ketli", "SM_P9_ChaiGlass"):
    sm = el.load_asset(PROPS + "/" + m); sm.modify(); sm.set_material(0, mi); saved.append(sm)
for a in saved: el.save_loaded_asset(a, False)

SLOTS = {"F1": "dhanraj", "F2": "Nilesh", "F3": "shivshankar", "F4": "Rohit", "F5": "rupesh", "CW": "Modi"}
FP = {"idle_anim": "Idle", "walk_anim": "Walk", "chat_anim": "Chat", "look_around_anim": "LookAround", "turn_anim": "TurnInPlace",
      "sit_down_anim": "SitDown", "sit_idle_anim": "SitIdle", "stand_up_anim": "StandUp", "gesture_anim": "Gesture", "pour_anim": "Pour"}
SP = {"idle_anim": "Idle", "walk_anim": "Walk", "menace_anim": "Menace", "talk_anim": "TalkAngry"}
def a3(nm, clip):
    p = "%s/%s/A3/SK_P9_NPC3D_%s_A3_%s" % (ROOT, nm, nm, clip)
    return el.load_asset(p) if el.does_asset_exist(p) else None
out = []
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    props = None
    if isinstance(a, unreal.P9FriendNPC) and str(a.get_editor_property("friend_key")) in SLOTS:
        nm = SLOTS[str(a.get_editor_property("friend_key"))]; props = FP
    elif isinstance(a, unreal.P9SilhouetteStalker):
        nm = "kanchana"; props = SP
    if not props: continue
    a.modify(); got = []
    for prop, clip in props.items():
        before = a.get_editor_property(prop); new = a3(nm, clip)
        if before is None and new is None: continue
        if before is not None and new is None: raise RuntimeError("P9PY: missing A3 %s %s" % (nm, clip))
        a.set_editor_property(prop, new); got.append(clip)
    out.append("%s[%s]" % (a.get_actor_label(), ",".join(got)))
    a.rerun_construction_scripts() if hasattr(a, "rerun_construction_scripts") else None
unreal.log("P9PY: 5d A3 clips: " + " | ".join(out))
unreal.log("P9PY: 5d props material saved (%d assets)" % len(saved))

# ---- read-only layout report
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    lab = a.get_actor_label()
    if lab in ("S5B_ChaiStall_Vishwaguru", "S5B_Chaiwala", "S3C_Kiosk_Concourse", "S2_Prop_ConcStall"):
        o, e = a.get_actor_bounds(False, True); l = a.get_actor_location()
        unreal.log("P9PY: layout %s loc=(%.0f,%.0f,%.0f) yaw=%.0f bounds o=(%.0f,%.0f,%.0f) e=(%.0f,%.0f,%.0f) hidden=%s" % (lab, l.x, l.y, l.z, a.get_actor_rotation().yaw, o.x, o.y, o.z, e.x, e.y, e.z, a.is_hidden_ed()))
