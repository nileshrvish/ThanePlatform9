# Stage 7A (Android memory): Normal and Roughness maps of the six non-Dhanraj NPCs capped at 1024 (BaseColor stays 2048 for
# faces/clothing). Backed up in _backups/textures_pre7A_*. Dhanraj's textures are not touched.
import unreal
el = unreal.EditorAssetLibrary
T = "/Game/P9/Characters/Station3D/Textures/T_P9_NPC3D_%s_%s"
out = []
for n in ["Nilesh", "shivshankar", "Rohit", "rupesh", "Modi", "kanchana"]:
    for k in ("BaseColor", "Normal", "Roughness"):
        t = el.load_asset(T % (n, k))
        if k != "BaseColor" and t.get_editor_property("max_texture_size") != 1024:
            t.set_editor_property("max_texture_size", 1024)
            el.save_loaded_asset(t, False)
        out.append("%s_%s %dx%d max %d %s" % (n, k, t.blueprint_get_size_x(), t.blueprint_get_size_y(), t.get_editor_property("max_texture_size"),
                                            str(t.get_editor_property("compression_settings")).split(".")[-1].split(":")[0]))
unreal.log("P9PY: textures: " + "; ".join(out))
