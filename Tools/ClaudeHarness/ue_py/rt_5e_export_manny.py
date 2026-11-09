# Stage 5E: export the Mannequin mesh + unarmed idle/walk to FBX so they can be re-imported facing +X (like the NPCs).
import unreal, os
OUT = r"C:\Users\niles\Documents\Blender Projects\P9_Assets\MannyX\Export"
os.makedirs(OUT, exist_ok=True)
items = {"SKM_MannyX": "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple",
         "SKM_MannyX_Idle": "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle",
         "SKM_MannyX_Walk": "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"}
for name, path in items.items():
    fn = os.path.join(OUT, name + ".fbx")
    if os.path.exists(fn): raise RuntimeError("exists " + fn)
    t = unreal.AssetExportTask()
    t.object = unreal.EditorAssetLibrary.load_asset(path); t.filename = fn
    t.automated = True; t.prompt = False; t.replace_identical = False
    o = unreal.FbxExportOption(); o.export_morph_targets = False; o.level_of_detail = False; o.collision = False
    o.export_local_time = True
    t.options = o
    ok = unreal.Exporter.run_asset_export_task(t)
    unreal.log("P9PY: export %s -> %s %s (%d bytes)" % (path.split("/")[-1], fn, ok, os.path.getsize(fn) if os.path.exists(fn) else 0))
