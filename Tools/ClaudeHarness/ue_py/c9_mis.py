# Stage 9C: normal-map texture settings + material-instance tuning (instances backed up in content_pre9C_*)
import unreal
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
log = lambda s: unreal.log("P9PY: " + s)
T = "/Game/P9/Environment/Materials9C/Textures/"
for n in ("T_P9_FOBChequer_N9C", "T_P9_StairTread_N9C", "T_P9_IslandSurface_N9C", "T_P9_Plaster_N9C", "T_P9_PrecastWall_N9C", "T_P9_CeilingPanel_N9C", "T_P9_SteelPerforated_N9C", "T_P9_Granite_N9C"):
    t = unreal.load_asset(T + n)
    t.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    t.set_editor_property("srgb", False)
    t.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    EAL.save_loaded_asset(t, False)
a = unreal.load_asset(T + "T_P9_FOBChequer_9C"); a.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD); EAL.save_loaded_asset(a, False)
E = "/Game/P9/Environment/"
# (instance, base texture override, normal map, normal strength, scalars)
CFG = [
    ("FootbridgeFloor/MI_P9_FOBChequer", "T_P9_FOBChequer_9C", "T_P9_FOBChequer_N9C", 1.0, dict(Roughness=0.42, RoughnessVariation=0.25)),
    ("StationProps/MI_P9_SteelPerforated", None, "T_P9_SteelPerforated_N9C", 0.8, dict(BaseBrightness=0.6, Roughness=0.48, RoughnessVariation=0.1)),
    ("StationProps/MI_P9_SteelSatin", None, None, 1.0, dict(BaseBrightness=0.62, Roughness=0.4, RoughnessVariation=0.12)),
    ("Architecture/MI_P9_Plaster", None, "T_P9_Plaster_N9C", 1.0, dict(RoughnessVariation=0.12, BaseBrightness=0.94)),
    ("StairFinish/MI_P9_StairTread", None, "T_P9_StairTread_N9C", 1.0, dict(BaseSaturation=0.55, Roughness=0.78, RoughnessVariation=0.2)),
    ("PlatformSurface/MI_P9_IslandSurface", None, "T_P9_IslandSurface_N9C", 1.0, dict(Roughness=0.62, RoughnessVariation=0.22, BaseSaturation=0.92)),
    ("Architecture/MI_P9_PrecastWall", None, "T_P9_PrecastWall_N9C", 1.0, dict(RoughnessVariation=0.1)),
    ("Architecture/MI_P9_CeilingPanel", None, "T_P9_CeilingPanel_N9C", 0.8, dict(RoughnessVariation=0.08)),
    ("Architecture/MI_P9_Granite", None, "T_P9_Granite_N9C", 0.7, dict(Roughness=0.5, RoughnessVariation=0.15)),
]
for path, base, nrm, ns, sc in CFG:
    mi = unreal.load_asset(E + path)
    if base:
        MEL.set_material_instance_texture_parameter_value(mi, "BaseTexture", unreal.load_asset(T + base))
    if nrm:
        MEL.set_material_instance_texture_parameter_value(mi, "NormalMap", unreal.load_asset(T + nrm))
        MEL.set_material_instance_static_switch_parameter_value(mi, "UseNormalMap", True)
        MEL.set_material_instance_scalar_parameter_value(mi, "NormalStrength", ns)
    for k, v in sc.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    log("%s: base %s normal %s %s" % (mi.get_name(), base, nrm, sc))
