# Stage 6C (read-only): master materials and their parameters, to pick parents for the analog clock material instances.
import unreal

reg = unreal.AssetRegistryHelpers.get_asset_registry()
MEL = unreal.MaterialEditingLibrary
for a in reg.get_assets_by_path("/Game/P9", recursive=True):
    if str(a.asset_class_path.asset_name) != "Material":
        continue
    m = unreal.load_asset(str(a.package_name))
    try:
        s = MEL.get_scalar_parameter_names(m); v = MEL.get_vector_parameter_names(m); t = MEL.get_texture_parameter_names(m)
    except Exception as e:
        s = v = t = ["?"]
    unreal.log("P9PY: %s blend=%s shading=%s two=%s | S%s V%s T%s" % (a.package_name, m.get_editor_property("blend_mode"), m.get_editor_property("shading_model"),
                                                               m.get_editor_property("two_sided"), list(map(str, s)), list(map(str, v)), list(map(str, t))))
for p in ["/Game/P9/Anomalies6A/Materials/MI_P9_Anom6_PosterPupil", "/Game/P9/Environment/StationSignage/MI_P9_SteelBlue"]:
    mi = unreal.load_asset(p)
    if mi:
        unreal.log("P9PY: %s parent %s" % (p, mi.get_editor_property("parent").get_path_name()))
