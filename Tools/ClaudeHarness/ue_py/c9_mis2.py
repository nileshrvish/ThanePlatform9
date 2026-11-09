# Stage 9C pass 2 (after viewing under the concourse tubes): plaster / ceiling / steel bench still read washed out
import unreal
MEL = unreal.MaterialEditingLibrary
E = "/Game/P9/Environment/"
for path, sc in (("Architecture/MI_P9_Plaster", dict(BaseBrightness=0.8, BaseSaturation=0.9)),
                 ("Architecture/MI_P9_CeilingPanel", dict(BaseBrightness=0.82)),
                 ("StationProps/MI_P9_SteelPerforated", dict(BaseBrightness=0.5)),
                 ("StationProps/MI_P9_SteelSatin", dict(BaseBrightness=0.55))):
    mi = unreal.load_asset(E + path)
    for k, v in sc.items():
        MEL.set_material_instance_scalar_parameter_value(mi, k, v)
    MEL.update_material_instance(mi)
    unreal.EditorAssetLibrary.save_loaded_asset(mi, False)
    unreal.log("P9PY: %s %s" % (mi.get_name(), sc))
