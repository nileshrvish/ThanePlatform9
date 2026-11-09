# Stage 6C: LOD screen sizes of the clock body (1.0 / 0.25 / 0.10) - the FBX import left 2.0 / 0.75 / 0.56 (LOD2 far too early).
import unreal

sme = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
p = "/Game/P9/Anomalies6C/SM_P9_AnalogClock_Body"
sm = unreal.load_asset(p)
api = [n for n in dir(sme) if "screen" in n.lower()]
unreal.log("P9PY: screen-size API %s" % api)
if hasattr(sme, "set_lod_screen_sizes"):
    pass
    ok = sme.set_lod_screen_sizes(sm, [1.0, 0.25, 0.10])
    unreal.EditorAssetLibrary.save_asset(p, only_if_is_dirty=False)
    unreal.log("P9PY: set %s -> %s" % (ok, list(sme.get_lod_screen_sizes(sm))))
