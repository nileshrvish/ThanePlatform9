# Stage 9C: the 96 footstep waves -> SC_P9_Player (the Effects slider), one-shot, like the 9A steps (assets only, no map change)
import unreal
EAL = unreal.EditorAssetLibrary
cls = unreal.load_asset("/Game/P9/Audio/OriginalAudioPack/Classes/SC_P9_Player")
n = 0
for p in EAL.list_assets("/Game/P9/Audio/OriginalAudioPack9C/Player", recursive=False):
    w = unreal.load_asset(p)
    if not isinstance(w, unreal.SoundWave): continue
    w.set_editor_property("looping", False)
    w.set_editor_property("sound_class_object", cls)
    w.set_editor_property("compression_quality", 70)
    EAL.save_loaded_asset(w, False); n += 1
unreal.log("P9PY: 9C step waves set: %d (class %s)" % (n, cls.get_name()))
