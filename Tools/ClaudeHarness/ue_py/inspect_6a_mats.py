# Stage 6A read-only: parameters of materials the new anomalies swap or drive. Modifies nothing.
import unreal

PATHS = ["/Game/P9/Environment/StationSignage/MI_P9_StationClock",
         "/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF9_10",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_Clock_A",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_IndicatorPF9_10_A",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_SignPF9_10_A",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_FOBSignPF9_10_A",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_FOBSignPF2_3_A",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_AdPoster_Watching",
         "/Game/P9/Anomalies/Materials/MI_P9_Anom_WallStain"]
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for p in PATHS:
    m = unreal.load_asset(p)
    if not m:
        unreal.log("P9PY: %s MISSING" % p)
        continue
    parent = m.get_editor_property("parent") if isinstance(m, unreal.MaterialInstance) else None
    tex = []
    for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(m):
        v = unreal.MaterialEditingLibrary.get_material_instance_texture_parameter_value(m, n) if isinstance(m, unreal.MaterialInstance) else None
        tex.append("%s=%s" % (n, v.get_path_name() if v else None))
    sc = [str(n) for n in unreal.MaterialEditingLibrary.get_scalar_parameter_names(m)]
    vc = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(m)]
    unreal.log("P9PY: %s parent=%s tex=%s scalar=%s vector=%s" % (p.split("/")[-1], parent.get_path_name() if parent else None, tex, sc, vc))
# all assets in the Anomalies and signage material folders
for folder in ["/Game/P9/Anomalies/Materials", "/Game/P9/Anomalies/Textures", "/Game/P9/Environment/StationSignage"]:
    names = sorted(str(a.asset_name) for a in reg.get_assets_by_path(folder, recursive=False))
    unreal.log("P9PY: [%s] %s" % (folder, " ".join(names)))
bo = unreal.load_asset("/Game/P9/Environment/Footbridge/MI_P9_BookingCounter") or None
unreal.log("P9PY: booking MI %s" % (bo.get_path_name() if bo else "not at guessed path"))
for a in reg.get_assets_by_path("/Game/P9", recursive=True):
    n = str(a.asset_name)
    if n in ("MI_P9_BookingCounter", "T_P9_BookingCounter", "MI_P9_AdPoster_Chai", "SM_P9_TrackPanel", "M_P9_Simple", "SM_P9_AdStand"):
        unreal.log("P9PY: found %s at %s" % (n, a.package_name))
