# Stage 5C read-only: compare the 5A v2 NPC meshes with the imported NPC3D meshes; list instance asset properties.
import unreal
el = unreal.EditorAssetLibrary
def sk_info(path):
    m = el.load_asset(path)
    if not m: return "missing " + path
    ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    n = ss.get_lod_count(m)
    lods = range(n); lod_s = ["L%d tris=%s" % (i, ss.get_num_verts(m, i)) for i in lods]
    mats = [(str(s.material_slot_name), s.material_interface.get_path_name() if s.material_interface else None) for s in m.materials]
    b = m.get_bounds()
    return "%s lods=%d [%s] mats=%s bounds_ext=(%.0f,%.0f,%.0f) origin=(%.0f,%.0f,%.0f) lodsettings=%s" % (
        path.split("/")[-1], n, "; ".join(lod_s).replace("tris","verts"), mats, b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z,
        m.get_editor_property("lod_settings"))
for p in ["/Game/P9/Characters/Station/F1_Curly/SK_P9_F1_Curly", "/Game/P9/Characters/Station/Silhouette/SK_P9_Silhouette",
          "/Game/P9/Characters/Station3D/dhanraj/SK_P9_NPC3D_dhanraj", "/Game/P9/Characters/Station3D/kanchana/SK_P9_NPC3D_kanchana"]:
    unreal.log("P9PY: " + sk_info(p))
mat = el.load_asset("/Game/P9/Characters/Friends/Materials/M_P9_FriendSkin")
unreal.log("P9PY: FriendSkin skel=%s two_sided=%s blend=%s" % (mat.get_editor_property("used_with_skeletal_mesh"), mat.get_editor_property("two_sided"), mat.get_editor_property("blend_mode")))
for t in ["Normal", "Roughness", "BaseColor"]:
    tx = el.load_asset("/Game/P9/Characters/Station3D/Textures/T_P9_NPC3D_dhanraj_" + t)
    unreal.log("P9PY: tex %s comp=%s srgb=%s flipG=%s size=%dx%d" % (t, tx.get_editor_property("compression_settings"), tx.get_editor_property("srgb"),
               tx.get_editor_property("flip_green_channel"), tx.blueprint_get_size_x(), tx.blueprint_get_size_y()))
sm = el.load_asset("/Game/P9/Characters/Station/Static/SM_P9_Chaiwala_VG"); sm2 = el.load_asset("/Game/P9/Characters/Station3D/Static/SM_P9_NPC3D_Modi")
for s in (sm, sm2):
    b = s.get_bounds(); unreal.log("P9PY: static %s ext=(%.0f,%.0f,%.0f) origin=(%.0f,%.0f,%.0f) lods=%d mat0=%s" % (s.get_name(), b.box_extent.x, b.box_extent.y, b.box_extent.z, b.origin.x, b.origin.y, b.origin.z, s.get_num_lods(), s.get_material(0).get_path_name() if s.get_material(0) else None))
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.P9FriendNPC):
        fm = a.get_editor_property("friend_mesh")
        anims = {k: (a.get_editor_property(k).get_name() if a.get_editor_property(k) else None) for k in ("idle_anim", "walk_anim", "chat_anim", "look_around_anim", "turn_anim", "sit_down_anim", "sit_idle_anim", "stand_up_anim", "gesture_anim", "pour_anim")}
        body = a.get_editor_property("body"); ov = body.get_editor_property("override_materials")
        unreal.log("P9PY: NPC %s key=%s mesh=%s anims=%s overrides=%s" % (a.get_actor_label(), a.get_editor_property("friend_key"), fm.get_name() if fm else None, anims, [o.get_name() if o else None for o in ov]))
    elif isinstance(a, unreal.P9SilhouetteStalker):
        anims = {k: (a.get_editor_property(k).get_name() if a.get_editor_property(k) else None) for k in ("stalker_mesh", "idle_anim", "walk_anim", "menace_anim", "talk_anim")}
        unreal.log("P9PY: SIL %s %s overrides=%s" % (a.get_actor_label(), anims, [o.get_name() if o else None for o in a.get_editor_property("body").get_editor_property("override_materials")]))
    elif isinstance(a, unreal.P9CampaignDirector):
        cm = a.get_editor_property("chaiwala_mesh")
        unreal.log("P9PY: DIRECTOR %s chaiwala_mesh=%s" % (a.get_actor_label(), cm.get_path_name() if cm else None))
