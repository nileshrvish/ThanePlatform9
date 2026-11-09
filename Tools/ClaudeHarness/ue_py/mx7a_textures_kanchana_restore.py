# Stage 7A: Kanchana is protected - her Normal/Roughness go back to their original max size 0 (no cap).
import unreal
el = unreal.EditorAssetLibrary
for k in ("Normal", "Roughness"):
    t = el.load_asset("/Game/P9/Characters/Station3D/Textures/T_P9_NPC3D_kanchana_" + k)
    t.set_editor_property("max_texture_size", 0); el.save_loaded_asset(t, False)
    unreal.log("P9PY: kanchana_%s %dx%d max %d" % (k, t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("max_texture_size")))
