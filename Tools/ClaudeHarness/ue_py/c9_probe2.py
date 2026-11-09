# Stage 9C probe 2: everything under the feet (traces start just above each floor; hidden collision cubes included)
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
log = lambda s: unreal.log("P9PY: " + s)
pts = {"PF1": (1100, -1050, 0), "PF1east": (2500, -1100, 0), "PF1stairs": (-800, -1525, 300), "PF1stairsLow": (-1200, -1525, 100), "FOBdeck": (0, 3000, 600), "Concourse": (-250, 3150, 600), "Booking": (-600, 3300, 600),
       "FOBnorth": (0, 6400, 600), "FOBsouth": (0, 1000, 600), "PF9": (1700, 7100, 0), "PF910stairs": (850, 7410, 300), "PF910landing": (1450, 7410, 300), "PF9east": (2800, 7100, 0),
       "PF9west": (-2800, 7440, 0), "TrainFloor": (1066, -720, 35), "PF23": (0, 1500, 0), "StallFront": (330, 3640, 600)}
ignore = []
for k, (x, y, z) in pts.items():
    s = []
    ign = []
    for i in range(5):
        h = unreal.SystemLibrary.line_trace_single(w, unreal.Vector(x, y, z + 140), unreal.Vector(x, y, z - 120), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ign, unreal.DrawDebugTrace.NONE, True)
        if h is None:
            break
        t = h.to_tuple()
        if not t[0]:
            break
        act, comp = t[9], t[10]
        mats = []
        if comp and hasattr(comp, "get_materials"):
            mats = sorted(set(m.get_name() for m in comp.get_materials() if m))
        hig = act.get_editor_property("hidden") if act else None
        s.append("%s z=%.1f hidG=%s mats=%s" % (act.get_actor_label(), t[4].z, hig, ",".join(mats)[:90]))
        ign.append(act)
    log("%s: %s" % (k, " || ".join(s)))
