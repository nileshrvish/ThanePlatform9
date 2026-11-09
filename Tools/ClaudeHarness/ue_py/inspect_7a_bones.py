# Stage 7A (read-only): bone names of the imported Mixamo meshes and the bones each LOD keeps.
import unreal
ss = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
for n in ["dhanraj/MX5G/SK_P9_NPC3D_dhanraj_MX", "Modi/MX7A/SK_P9_NPC3D_Modi_MX", "shivshankar/MX7A/SK_P9_NPC3D_shivshankar_MX", "Nilesh/MX7A/SK_P9_NPC3D_Nilesh_MX"]:
    m = unreal.load_asset("/Game/P9/Characters/Station3D/" + n)
    names = [str(x) for x in m.get_editor_property("skeleton").get_editor_property("bone_tree")] if False else None
    comp = unreal.SkeletalMeshComponent()
    comp.set_skeletal_mesh_asset(m)
    b = [str(comp.get_bone_name(i)) for i in range(comp.get_num_bones())]
    unreal.log("P9PY: %s %d bones: %s" % (n.split("/")[-1], len(b), b))
