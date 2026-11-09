# Stage 7A fixes (editor; map NOT saved): Unreal strips the "mixamorig:" prefix on import, so
#  1) LODS_P9_NPC3D_MX7A (new 7A asset) gets the real finger bone names and the six MX7A meshes regenerate their LODs;
#  2) the chaiwala's prop sockets are RightHand / LeftHand with the hand-space offsets measured in PIE (pie_7a_handpose.py).
import unreal, json
el = unreal.EditorAssetLibrary
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
R = "/Game/P9/Characters/Station3D/"
FINGERS = ["%sHand%s1" % (s, f) for s in ("Left", "Right") for f in ("Thumb", "Index", "Middle", "Ring", "Pinky")]
def bone_ref(name):
    r = unreal.BoneFilter(); r.set_editor_property("bone_name", name); r.set_editor_property("exclude_self", False); return r
lods = el.load_asset(R + "LODS_P9_NPC3D_MX7A")
groups = list(lods.get_editor_property("lod_groups"))
for i, g in enumerate(groups):
    if i >= 1:
        g.set_editor_property("bone_filter_action_option", unreal.BoneFilterActionOption.REMOVE)
        g.set_editor_property("bone_list", [bone_ref(b) for b in FINGERS])
lods.set_editor_property("lod_groups", groups)
el.save_loaded_asset(lods, False)
for n in ["Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"]:
    mesh = el.load_asset("%s%s/MX7A/SK_P9_NPC3D_%s_MX" % (R, n, n))
    mesh.modify()
    ss.regenerate_lod(mesh, 3, True, False)
    el.save_loaded_asset(mesh, False)
    comp = unreal.SkeletalMeshComponent(); comp.set_skeletal_mesh_asset(mesh)
    unreal.log("P9PY: %s LODs %d verts %s bones %d" % (n, ss.get_lod_count(mesh), [ss.get_num_verts(mesh, l) for l in range(ss.get_lod_count(mesh))], comp.get_num_bones()))
J = json.load(open(r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad\handpose_7a.json"))
cw = next(a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if a.get_actor_label() == "S5B_Chaiwala")
cw.modify()
cw.set_editor_property("held_socket_right", "RightHand"); cw.set_editor_property("held_socket_left", "LeftHand")
for side, prop in (("r", "held_offset_right"), ("l", "held_offset_left")):
    v = J[side]
    cw.set_editor_property(prop, unreal.Transform(unreal.Vector(*v["loc"]), unreal.Rotator(roll=v["rot"][0], pitch=v["rot"][1], yaw=v["rot"][2]), unreal.Vector(1, 1, 1)))
unreal.log("P9PY: chaiwala sockets %s/%s offsets %s" % (cw.get_editor_property("held_socket_right"), cw.get_editor_property("held_socket_left"), {k: [round(x, 2) for x in J[k]["loc"]] for k in J}))
