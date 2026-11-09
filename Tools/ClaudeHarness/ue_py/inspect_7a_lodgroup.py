import unreal
g = unreal.SkeletalMeshLODGroupSettings()
unreal.log("P9PY: %s" % [n for n in dir(g) if not n.startswith("_")])
unreal.log("P9PY: doc %s" % [l for l in (unreal.SkeletalMeshLODGroupSettings.__doc__ or "").splitlines() if "-" in l][:30])
