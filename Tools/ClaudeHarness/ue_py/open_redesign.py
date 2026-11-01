import unreal
def out(*a): unreal.log("P9PY: " + " ".join(str(x) for x in a))
d = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()] + [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
if d: out("STOP dirty", d); raise SystemExit
w = unreal.EditorLoadingAndSavingUtils.load_map("/Game/P9/Maps/L_Station_Redesign")
out("loaded", w.get_path_name(), len(unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor)))
