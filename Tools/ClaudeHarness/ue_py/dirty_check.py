import unreal
d = [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()] + [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
unreal.log("P9PY: dirty " + str(d))
