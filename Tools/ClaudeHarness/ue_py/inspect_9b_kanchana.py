import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
dt = unreal.load_asset("/Game/P9/UI/DT_P9_Captions")
names = set()
for a in eas.get_all_level_actors():
    if isinstance(a, unreal.P9SilhouetteStalker):
        for s in a.get_editor_property("lines"):
            p = str(s.get_asset_name()) if hasattr(s, "get_asset_name") else str(s)
            unreal.log("P9PY: Kanchana line %s" % p)
    if isinstance(a, unreal.P9CampaignDirector):
        unreal.log("P9PY: director wake %s call %s run %s" % (a.get_editor_property("chaiwala_wake_line"), a.get_editor_property("chaiwala_call_line"), a.get_editor_property("train_run_ambience")))
unreal.log("P9PY: caption rows %s" % sorted(set(str(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, "SoundName")))) [:0])
unreal.log("P9PY: caption sounds %s" % sorted(set(unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, "SoundName"))))
