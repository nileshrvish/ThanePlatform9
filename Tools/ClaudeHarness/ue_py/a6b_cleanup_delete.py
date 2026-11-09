# Stage 6B cleanup, step 2: delete the confirmed-unused old anomaly assets (backed up in _backups/content_pre6B_*).
# Each asset is re-checked right before deletion: it must have no referencer outside the delete list.
import json
import unreal

SCR = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad"
plan = json.load(open(SCR + r"\cleanup_plan.json"))
dele = set(plan["delete"])
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
EAL = unreal.EditorAssetLibrary
done, refused = [], []
for p in sorted(dele):
    outside = [str(r) for r in (reg.get_referencers(p, opts) or []) if str(r) not in dele]
    if outside:
        refused.append((p, outside)); continue
    if EAL.delete_asset(p): done.append(p)
    else: refused.append((p, ["delete_asset failed"]))
for folder in ["/Game/P9/Anomalies/Audio4D", "/Game/P9/Entities/Materials", "/Game/P9/Entities/Textures", "/Game/P9/Entities"]:
    if EAL.does_directory_exist(folder) and not EAL.list_assets(folder, recursive=True):
        EAL.delete_directory(folder)
unreal.log("P9PY: deleted %d assets; refused %d %s" % (len(done), len(refused), refused))
