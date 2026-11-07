import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
ar.scan_paths_synchronous(["/Game/Characters"], True)
for p in ["/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple", "/Game/Characters/Mannequins/Meshes/SK_Mannequin", "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle", "/Game/Characters/Mannequins/Anims/Unarmed/Walk/MF_Unarmed_Walk_Fwd"]:
    a = unreal.EditorAssetLibrary.load_asset(p)
    unreal.log("P9PY: %s -> %s %s" % (p.split("/")[-1], a.get_class().get_name() if a else None, ("len %.2fs" % a.get_play_length()) if isinstance(a, unreal.AnimSequence) else ""))
sk = unreal.EditorAssetLibrary.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
if sk:
    s = sk.skeleton
    unreal.log("P9PY: manny skeleton %s" % s.get_name())
