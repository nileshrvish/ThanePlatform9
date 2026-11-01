# Stage 5B read-only: anomaly asset folders and whether each is referenced by anything (package referencers).
import json
import unreal

OUT = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\inspect_5b_assets.json"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
res = {}
for folder in ["/Game/P9/Anomalies", "/Game/P9/Entities", "/Game/P9/Announcements"]:
    for path in unreal.EditorAssetLibrary.list_assets(folder, recursive=True, include_folder=False):
        pkg = path.split(".")[0]
        refs = reg.get_referencers(pkg, opts) or []
        res[pkg] = [str(r) for r in refs]
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(res, f, indent=1)
unreal.log("P9PY: inspect_5b_assets %d assets -> %s" % (len(res), OUT))
