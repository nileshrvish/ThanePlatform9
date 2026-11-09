# Stage 9A A/B helper: MODE identity -> the Modi / dhanraj MIs behave as before 9A; MODE cal -> the 9A calibration values.
import unreal
mel = unreal.MaterialEditingLibrary
MODE = open(unreal.Paths.project_dir() + "Tools/ClaudeHarness/ab_mode.txt").read().strip()
vals = {"identity": {"Modi": (1, 1, 1, 0.5), "dhanraj": (1, 1, 1, 0.5)},
        "cal": {"Modi": (0.32, 0.66, 1.08, 0.4), "dhanraj": (0.28, 0.62, 1.18, 0.4)}}[MODE]
for n, (k, mx, s, sp) in vals.items():
    mi = unreal.load_asset("/Game/P9/Characters/Station3D/Materials/MI_P9_NPC3D_%s" % n)
    for pn, v in (("AlbedoKnee", k), ("AlbedoMax", mx), ("AlbedoSaturation", s), ("SpecularLevel", sp)):
        mel.set_material_instance_scalar_parameter_value(mi, pn, float(v))
    mel.update_material_instance(mi)
unreal.log("P9PY: NPC material mode %s" % MODE)
