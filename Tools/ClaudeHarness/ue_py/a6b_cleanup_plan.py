# Stage 6B cleanup - DRY RUN (modifies nothing). Classifies every asset under /Game/P9/Anomalies and /Game/P9/Entities:
#   KEEP_CODE  named by C++ (code_keep.txt) or needed by such an asset
#   KEEP_USED  referenced by anything outside the candidate folders (other than the director's PreloadAssets list)
#   DELETE     referenced by nothing, or only by other DELETE assets / the director's PreloadAssets list
# and lists the anomaly spawn points the director no longer uses. Writes cleanup_plan.json to the scratchpad.
import json
import unreal

SCR = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad"
FOLDERS = ["/Game/P9/Anomalies", "/Game/P9/Entities"]
MAP = "/Game/P9/Maps/L_Station_Redesign"
reg = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False,
                                             include_hard_management_references=False)
cands = {}
for f in FOLDERS:
    for a in reg.get_assets_by_path(f, recursive=True):
        cands[str(a.package_name)] = str(a.asset_class_path.asset_name)
code_names = set(open(SCR + r"\code_keep.txt").read().split())
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
d = [a for a in actors if isinstance(a, unreal.P9CampaignDirector)][0]
preload = {o.get_outermost().get_name() for o in d.get_editor_property("preload_assets") if o}

keep = {p for p in cands if p.split("/")[-1] in code_names}
changed = True
while changed:                                         # dependencies of kept assets are kept; anything used outside is kept
    changed = False
    for p in list(cands):
        if p in keep:
            for dep in reg.get_dependencies(p, opts) or []:
                dep = str(dep)
                if dep in cands and dep not in keep:
                    keep.add(dep); changed = True
            continue
        refs = [str(r) for r in (reg.get_referencers(p, opts) or [])]
        outside = [r for r in refs if r not in cands and not (r == MAP and p in preload)]
        if outside or any(r in keep for r in refs if r in cands):
            keep.add(p); changed = True
plan = {"delete": sorted(p for p in cands if p not in keep), "keep": {}}
for p in sorted(keep):
    refs = [str(r) for r in (reg.get_referencers(p, opts) or [])]
    plan["keep"][p] = "code" if p.split("/")[-1] in code_names else ("used by " + ", ".join(r for r in refs if r not in cands)[:120] or "needed by a kept asset")
plan["preload_to_remove"] = sorted(p for p in plan["delete"] if p in preload)
listed = {p.get_actor_label() for p in d.get_editor_property("spawn_points") if p}
plan["points_to_remove"] = sorted(a.get_actor_label() for a in actors if isinstance(a, unreal.P9AnomalySpawnPoint) and a.get_actor_label() not in listed)
# anything else in the level that points at an unlisted spawn point?
pointers = []
for a in actors:
    if isinstance(a, unreal.P9AnomalySpawnPoint) or isinstance(a, unreal.P9CampaignDirector):
        continue
    for r in (reg.get_referencers(MAP, opts) or [])[:0]:
        pass
plan["points_listed"] = len(listed)
json.dump(plan, open(SCR + r"\cleanup_plan.json", "w"), indent=1)
unreal.log("P9PY: cleanup plan - %d candidates: delete %d, keep %d (preload entries to drop %d); spawn points to remove %d (listed %d)"
           % (len(cands), len(plan["delete"]), len(keep), len(plan["preload_to_remove"]), len(plan["points_to_remove"]), len(listed)))
