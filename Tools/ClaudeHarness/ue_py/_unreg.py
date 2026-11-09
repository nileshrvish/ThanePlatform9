import unreal, builtins
for h in getattr(builtins, "P9_PROBES", []):
    unreal.unregister_slate_post_tick_callback(h)
builtins.P9_PROBES = []
unreal.log("P9PY: probes cleared")
