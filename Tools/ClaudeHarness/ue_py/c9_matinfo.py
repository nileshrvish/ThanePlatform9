# Stage 9C (read-only): parameters + textures of the main station / NPC material instances
import unreal
log = lambda s: unreal.log("P9PY: " + s)
MEL = unreal.MaterialEditingLibrary
paths = ["/Game/P9/Environment/FootbridgeFloor/MI_P9_FOBChequer", "/Game/P9/Environment/StationProps/MI_P9_SteelPerforated", "/Game/P9/Environment/StationProps/MI_P9_SteelSatin",
         "/Game/P9/Environment/Canopy_v2/Materials/MI_P9_PaintedSteel", "/Game/P9/Environment/Architecture/MI_P9_Plaster", "/Game/P9/Environment/StairFinish/MI_P9_StairTread",
         "/Game/P9/Environment/PlatformSurface/MI_P9_IslandSurface", "/Game/P9/Environment/Canopy_v2/Materials/MI_P9_Concrete", "/Game/P9/Environment/Architecture/MI_P9_SteelGrey",
         "/Game/P9/Environment/Architecture/MI_P9_SteelBlue", "/Game/P9/Environment/Architecture/MI_P9_Granite", "/Game/P9/Environment/Architecture/MI_P9_CeilingPanel",
         "/Game/P9/Environment/Architecture/MI_P9_PrecastWall", "/Game/P9/Environment/Architecture/MI_P9_PlasterDado", "/Game/P9/Environment/Architecture/MI_P9_BookingCounter",
         "/Game/P9/Characters/Station3D/Materials/MI_P9_NPC3D_dhanraj", "/Game/P9/Characters/Station3D/Materials/MI_P9_NPC3D_shivshankar"]
seen = set()
for p in paths:
    mi = unreal.load_asset(p)
    if not mi:
        log("missing %s" % p); continue
    parent = mi.get_editor_property("parent")
    sc = ["%s=%.3f" % (x.parameter_info.name, x.parameter_value) for x in mi.get_editor_property("scalar_parameter_values")]
    vc = ["%s=(%.2f,%.2f,%.2f)" % (x.parameter_info.name, x.parameter_value.r, x.parameter_value.g, x.parameter_value.b) for x in mi.get_editor_property("vector_parameter_values")]
    tx = []
    for x in mi.get_editor_property("texture_parameter_values"):
        t = x.parameter_value
        if t:
            tx.append("%s=%s(%dx%d,%s,srgb=%s)" % (x.parameter_info.name, t.get_name(), t.blueprint_get_size_x(), t.blueprint_get_size_y(), str(t.get_editor_property("lod_group")).split(".")[-1], t.get_editor_property("srgb")))
    log("%s parent=%s | %s | %s | %s" % (mi.get_name(), parent.get_name() if parent else None, " ".join(sc), " ".join(vc), " ".join(tx)))
    if parent and parent.get_path_name() not in seen and isinstance(parent, unreal.Material):
        seen.add(parent.get_path_name())
        log("   master %s scalars %s vectors %s textures %s" % (parent.get_name(), MEL.get_scalar_parameter_names(parent), MEL.get_vector_parameter_names(parent), MEL.get_texture_parameter_names(parent)))
