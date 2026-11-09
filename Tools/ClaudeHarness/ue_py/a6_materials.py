# Stage 6A: material instances for the new anomalies - each one a duplicate of the live material it replaces, with only the
# texture changed (so tint/roughness/intensity match). Never overwrites: an existing target is left alone and reported.
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TEX = "/Game/P9/Anomalies6A/Textures/"
MAT = "/Game/P9/Anomalies6A/Materials/"
JOBS = [
    ("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF9_10", "MI_P9_Anom6_Indicator_Panvel", "T_P9_Anom6_Indicator_Panvel"),
    ("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF9_10", "MI_P9_Anom6_Indicator_Dadar", "T_P9_Anom6_Indicator_Dadar"),
    ("/Game/P9/Environment/StationSignage/MI_P9_TrainIndicator_PF9_10", "MI_P9_Anom6_Indicator_Thane", "T_P9_Anom6_Indicator_Thane"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF4_5", "MI_P9_Anom6_FOBSignPF4_5_Arrows", "T_P9_Anom6_FOBSignPF4_5_Arrows"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF4_5", "MI_P9_Anom6_FOBSignPF4_5_Number", "T_P9_Anom6_FOBSignPF4_5_Number"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF6_7", "MI_P9_Anom6_FOBSignPF6_7_Arrows", "T_P9_Anom6_FOBSignPF6_7_Arrows"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF6_7", "MI_P9_Anom6_FOBSignPF6_7_Number", "T_P9_Anom6_FOBSignPF6_7_Number"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF8", "MI_P9_Anom6_FOBSignPF8_Arrows", "T_P9_Anom6_FOBSignPF8_Arrows"),
    ("/Game/P9/Environment/StationSignage/MI_P9_FOBSign_PF8", "MI_P9_Anom6_FOBSignPF8_Number", "T_P9_Anom6_FOBSignPF8_Number"),
    ("/Game/P9/Environment/Architecture/MI_P9_BookingCounter", "MI_P9_Anom6_BookingCounter_231", "T_P9_Anom6_BookingCounter_231"),
    ("/Game/P9/Environment/Architecture/MI_P9_BookingCounter", "MI_P9_Anom6_BookingCounter_312", "T_P9_Anom6_BookingCounter_312"),
    ("/Game/P9/Environment/Architecture/MI_P9_BookingCounter", "MI_P9_Anom6_BookingCounter_174", "T_P9_Anom6_BookingCounter_174"),
    ("/Game/P9/Environment/Architecture/MI_P9_BookingCounter", "MI_P9_Anom6_BookingCounter_123", "T_P9_Anom6_BookingCounter_123"),
    ("/Game/P9/Anomalies/Materials/MI_P9_AdPoster_Chai", "MI_P9_Anom6_AdPoster_Boy", "T_P9_Anom6_AdPoster_Boy"),
]
made = []
for src, name, tex in JOBS:
    dst = MAT + name
    if EAL.does_asset_exist(dst):
        unreal.log("P9PY: exists (left alone) %s" % dst)
        continue
    mi = EAL.duplicate_asset(src, dst)
    t = unreal.load_asset(TEX + tex)
    names = MEL.get_texture_parameter_names(mi)
    if not mi or not t or len(names) != 1:
        unreal.log_warning("P9PY: FAILED %s (mi %s tex %s params %s)" % (name, mi, t, names))
        continue
    MEL.set_material_instance_texture_parameter_value(mi, names[0], t)
    made.append(dst)
    unreal.log("P9PY: made %s from %s, %s = %s" % (name, src.split("/")[-1], names[0], tex))
# the pupils: the lit textured material, near black and a little glossy (a painted pupil would be flat - this one is not)
dst = MAT + "MI_P9_Anom6_PosterPupil"
if not EAL.does_asset_exist(dst):
    mi = EAL.duplicate_asset("/Game/P9/Anomalies/Materials/MI_P9_Anom_WallStain", dst)
    MEL.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(0.012, 0.010, 0.009, 1.0))
    MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", 0.35)
    made.append(dst)
    unreal.log("P9PY: made MI_P9_Anom6_PosterPupil (tint near black)")
# clock atlas: crisp, no mips (the face is redrawn from exact cells)
atlas = unreal.load_asset(TEX + "T_P9_Anom6_ClockDigits")
if atlas and atlas.get_editor_property("mip_gen_settings") != unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS:
    atlas.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    made.append(TEX + "T_P9_Anom6_ClockDigits")
for p in made:
    EAL.save_asset(p, only_if_is_dirty=False)
unreal.log("P9PY: a6_materials saved %d assets" % len(made))
