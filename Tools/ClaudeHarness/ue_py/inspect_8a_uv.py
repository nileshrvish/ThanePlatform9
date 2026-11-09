# Stage 8A (read-only): UV channels of the creature meshes and the material's compiled state.
import unreal
for p in ("/Game/P9/Creatures8A/Greeting/SM_P9_Greeting_rest", "/Game/P9/Creatures8A/Greeting/SM_P9_Greeting_lunge", "/Game/P9/Creatures8A/LanternMan/SM_P9_LanternMan"):
    sm = unreal.load_asset(p)
    unreal.log("P9PY: %s uv channels LOD0 %d, LOD1 %d" % (sm.get_name(), unreal.EditorStaticMeshLibrary.get_num_uv_channels(sm, 0),
                                                          unreal.EditorStaticMeshLibrary.get_num_uv_channels(sm, 1) if unreal.EditorStaticMeshLibrary.get_lod_count(sm) > 1 else -1))
m = unreal.load_asset("/Game/P9/Creatures8A/Materials/M_P9_Creature8A")
stats = unreal.MaterialEditingLibrary.get_statistics(m)
unreal.log("P9PY: master stats vs %s ps %s samplers %s" % (stats.num_vertex_shader_instructions, stats.num_pixel_shader_instructions, stats.num_samplers))
for n in ("BaseColorTex", "NormalTex", "RoughnessTex", "EmissiveMaskTex"):
    for e in unreal.MaterialEditingLibrary.get_material_selected_nodes(m) or []:
        pass
unreal.log("P9PY: used by %s" % [x.get_name() for x in unreal.MaterialEditingLibrary.get_child_instances(m)] if hasattr(unreal.MaterialEditingLibrary, "get_child_instances") else "P9PY: -")
