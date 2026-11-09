# Stage 9B setup (editor; saves the new assets, does NOT save the map):
#  - menu audio: classes (music -> SC_P9_Music, UI -> SC_P9_Player), looping
#  - DT_P9_Captions (FP9CaptionRow) filled from P9_Assets/Captions9B/captions_9b.csv (timed from the audio)
#  - S9B_MainMenu (AP9MainMenu) placed once, wired to the menu sounds and the caption table
import csv, json
import unreal
EAL = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = lambda s: unreal.log("P9PY: " + s)
C = "/Game/P9/Audio/OriginalAudioPack/Classes/"
music = unreal.load_asset(C + "SC_P9_Music"); player = unreal.load_asset(C + "SC_P9_Player")
for p in EAL.list_assets("/Game/P9/Audio/OriginalAudioPack9B", recursive=True):
    w = unreal.load_asset(p)
    if not isinstance(w, unreal.SoundWave):
        continue
    is_music = "/Music/" in p
    w.set_editor_property("looping", is_music)
    w.set_editor_property("sound_class_object", music if is_music else player)
    w.set_editor_property("compression_quality", 45 if is_music else 60)
    EAL.save_loaded_asset(w, False)
    log("wave %s class %s loop %s" % (w.get_name(), "Music" if is_music else "Player", is_music))

path = "/Game/P9/UI/DT_P9_Captions"
dt = unreal.load_asset(path) if EAL.does_asset_exist(path) else None
if dt is None:
    f = unreal.DataTableFactory()
    f.set_editor_property("struct", unreal.P9CaptionRow.static_struct())
    dt = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DT_P9_Captions", "/Game/P9/UI", unreal.DataTable, f)
rows = []
with open("C:/Users/niles/Documents/Blender Projects/P9_Assets/Captions9B/captions_9b.csv", encoding="utf-8") as fh:
    for r in csv.DictReader(fh):
        rows.append({"Name": r["Name"], "SoundName": r["SoundName"], "Order": int(r["Order"]), "Start": float(r["Start"]), "End": float(r["End"]), "Text": r["Text"]})
ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(dt, json.dumps(rows, ensure_ascii=False))
EAL.save_loaded_asset(dt, False)
log("DT_P9_Captions filled=%s rows=%d" % (ok, len(unreal.DataTableFunctionLibrary.get_data_table_row_names(dt))))

actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
m = actors.get("S9B_MainMenu")
if m is None:
    m = eas.spawn_actor_from_class(unreal.P9MainMenu, unreal.Vector(0, 0, 1400), unreal.Rotator(0, 0, 0))
    m.set_actor_label("S9B_MainMenu")
    m.set_folder_path("P9/UI")
A = "/Game/P9/Audio/OriginalAudioPack9B/"
m.set_editor_property("menu_music", unreal.load_asset(A + "Music/BGM_Menu_9B"))
m.set_editor_property("click_sound", unreal.load_asset(A + "UI/UI_Click_9B"))
m.set_editor_property("confirm_sound", unreal.load_asset(A + "UI/UI_Confirm_9B"))
m.set_editor_property("back_sound", unreal.load_asset(A + "UI/UI_Back_9B"))
m.set_editor_property("caption_table", dt)
log("menu actor %s music %s table %s" % (m.get_actor_label(), m.get_editor_property("menu_music"), m.get_editor_property("caption_table")))
