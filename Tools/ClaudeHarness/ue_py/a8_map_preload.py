# Stage 8A (editor; not saved): the director preloads M_P9_Creature8A_v2 (used) instead of M_P9_Creature8A (did not compile, unused).
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
d = next(a for a in eas.get_all_level_actors() if isinstance(a, unreal.P9CampaignDirector))
pre = [a for a in d.get_editor_property("preload_assets") if a and a.get_name() != "M_P9_Creature8A"]
v2 = unreal.load_asset("/Game/P9/Creatures8A/Materials/M_P9_Creature8A_v2")
if v2 not in pre:
    pre.append(v2)
d.set_editor_property("preload_assets", pre)
names = [a.get_name() for a in pre]
unreal.log("P9PY: preload %d assets; v2 %s, old %s; 8A meshes %s" % (len(pre), "M_P9_Creature8A_v2" in names, "M_P9_Creature8A" in names,
           [n for n in names if n.startswith(("SM_P9_FOBB", "SM_P9_Escal", "SM_P9_Stair", "SM_P9_Lantern", "SM_P9_Greeting"))]))
