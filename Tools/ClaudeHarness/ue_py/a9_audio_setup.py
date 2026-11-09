# Stage 9A: sound settings of the imported 9A waves (classes, looping, compression) + the level wiring (does NOT save the map):
#  - S9A_TrainAudio (AP9TrainAudio) placed once;
#  - every AP9Train: RunLoopSound / BrakeSound -> none (the 9A train audio plays them), HornSound -> 9A long horn;
#  - the audio director: 9A wind, footsteps and thunder (explicit, in case the instance holds 8A values).
import unreal
EAL = unreal.EditorAssetLibrary
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
log = lambda s: unreal.log("P9PY: " + s)
AU = "/Game/P9/Audio/OriginalAudioPack"
cls = {n: unreal.load_asset(AU + "/Classes/" + n) for n in ("SC_P9_Player", "SC_P9_Railway", "SC_P9_StationAmbience", "SC_P9_HorrorFX")}
counts = {}
for path in EAL.list_assets("/Game/P9/Audio/OriginalAudioPack9A", recursive=True):
    w = unreal.load_asset(path)
    if not isinstance(w, unreal.SoundWave):
        continue
    n = w.get_name()
    w.set_editor_property("looping", n.endswith("_Loop"))
    if "/Player/" in path:
        c = "SC_P9_Player"; q = 60
    elif "/Train/" in path:
        c = "SC_P9_Railway"; q = 55
    elif "/Weather/" in path:
        c = "SC_P9_StationAmbience"; q = 45
    else:
        c = "SC_P9_StationAmbience"; q = 40
    w.set_editor_property("compression_quality", q)
    if cls[c]:
        w.set_editor_property("sound_class_object", cls[c])
    EAL.save_loaded_asset(w, False)
    counts[c] = counts.get(c, 0) + 1
log("9A waves: %s" % counts)

actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
ta = actors.get("S9A_TrainAudio")
if ta is None:
    ta = eas.spawn_actor_from_class(unreal.P9TrainAudio, unreal.Vector(0, 0, 1300), unreal.Rotator(0, 0, 0))
    ta.set_actor_label("S9A_TrainAudio")
    ta.set_folder_path("P9/Audio")
log("train audio actor: %s" % ta.get_actor_label())
horn = unreal.load_asset(AU + "9A/Train/SFX_Train9A_Horn_Long")
for a in actors.values():
    if isinstance(a, unreal.P9Train):
        old = (a.get_editor_property("run_loop_sound"), a.get_editor_property("brake_sound"), a.get_editor_property("horn_sound"))
        a.set_editor_property("run_loop_sound", None)
        a.set_editor_property("brake_sound", None)
        a.set_editor_property("horn_sound", horn)
        log("train %s: run %s brake %s horn %s -> none / none / %s" % (a.get_actor_label(), old[0], old[1], old[2], horn.get_name() if horn else None))
ad = next(a for a in actors.values() if isinstance(a, unreal.P9AudioDirector))
ad.set_editor_property("platform_wind", unreal.load_asset(AU + "9A/Ambience/AMB_Platform_Wind9A_Loop"))
ad.set_editor_property("wind_volume", 0.20)
log("audio director wind %s vol %.2f thunder sets %d/%d/%d steps9A %d" % (ad.get_editor_property("platform_wind"), ad.get_editor_property("wind_volume"),
    len(ad.get_editor_property("thunder_distant")), len(ad.get_editor_property("thunder_strong")), len(ad.get_editor_property("thunder_close")),
    len(ad.get_editor_property("steps9a_platform_walk_l"))))
