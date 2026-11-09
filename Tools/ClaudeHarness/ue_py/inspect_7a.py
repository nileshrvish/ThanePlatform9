# Stage 7A (read-only): every station NPC's mesh, clips, speeds; mesh stats (tris per LOD, bones, slots, textures).
import unreal

el = unreal.EditorAssetLibrary
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
acts = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
CLIPS = ["idle_anim", "chat_anim", "look_around_anim", "turn_anim", "walk_anim", "sit_down_anim", "sit_idle_anim", "stand_up_anim",
         "gesture_anim", "pour_anim", "drunk_walk_anim", "stairs_up_anim"]
def nm(o): return o.get_path_name().split("/")[-1].split(".")[0] if o else "-"
def mesh_stats(m):
    if not m: return "-"
    lods = ss.get_lod_count(m)
    tris = []
    for l in range(lods):
        try: tris.append(ss.get_num_verts(m, l))
        except Exception: tris.append("?")
    sk = m.get_editor_property("skeleton")
    return "%s skel %s bones %d verts/LOD %s slots %s" % (m.get_path_name().split(".")[0], nm(sk), sk.get_editor_property("bone_tree").__len__() if sk else -1, tris,
                                               [nm(s.material_interface) for s in m.materials])
for a in acts:
    if isinstance(a, unreal.P9FriendNPC):
        unreal.log("P9PY: %s key %s role %s" % (a.get_actor_label(), a.get_editor_property("friend_key"), a.get_editor_property("role")))
        unreal.log("P9PY:   mesh %s" % mesh_stats(a.get_editor_property("friend_mesh")))
        unreal.log("P9PY:   clips %s" % {c: nm(a.get_editor_property(c)) for c in CLIPS})
        unreal.log("P9PY:   speeds walk %.1f clip %.1f stairs %.1f/%.1f drunk %.2f body %s" % (a.get_editor_property("walk_speed"), a.get_editor_property("walk_clip_speed"),
                   a.get_editor_property("stairs_up_speed"), a.get_editor_property("stairs_up_clip_speed"), a.get_editor_property("drunk_chance"),
                   nm(a.get_editor_property("body").get_skeletal_mesh_asset())))
    if isinstance(a, unreal.P9SilhouetteStalker):
        unreal.log("P9PY: %s (stalker) mesh %s" % (a.get_actor_label(), nm(a.get_editor_property("body").get_skeletal_mesh_asset()) if a.get_editor_property("body") else "?"))
