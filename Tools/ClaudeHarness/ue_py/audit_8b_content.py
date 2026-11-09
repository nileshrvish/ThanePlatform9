# Stage 8B (read-only): will the cook include every Stage 8A asset? Walks the asset registry's HARD package dependencies from the
# station map (what the cooker follows), lists every asset in the 8A folders as reachable or not, and checks that every soft path the
# C++ code loads at runtime exists. Writes production/qa/evidence/stage8b/content_audit_prebuild.txt.
import unreal
AR = unreal.AssetRegistryHelpers.get_asset_registry()
EAL = unreal.EditorAssetLibrary
OUT = r"C:\Users\niles\Documents\Unreal Projects\ThanePlatform9\production\qa\evidence\stage8b\content_audit_prebuild.txt"
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=False, include_hard_package_references=True,
                                             include_searchable_names=False, include_soft_management_references=False, include_hard_management_references=False)
seen, todo = set(), ["/Game/P9/Maps/L_Station_Redesign"]
while todo:
    p = todo.pop()
    if p in seen:
        continue
    seen.add(p)
    for d in AR.get_dependencies(p, opts) or []:
        d = str(d)
        if d.startswith("/Game/") and d not in seen:
            todo.append(d)
lines = ["Stage 8B content inclusion audit (pre-build) - hard dependency closure of /Game/P9/Maps/L_Station_Redesign: %d /Game packages" % len(seen), ""]
missing = []
for folder in ("/Game/P9/Creatures8A", "/Game/P9/Anomalies8A", "/Game/P9/Audio/OriginalAudioPack"):
    pk = sorted(set(str(a).split(".")[0] for a in EAL.list_assets(folder, recursive=True)))
    inside = [p for p in pk if p in seen]
    out = [p for p in pk if p not in seen]
    lines.append("%s: %d assets, %d reachable from the map, %d NOT reachable" % (folder, len(pk), len(inside), len(out)))
    for p in out:
        lines.append("   not reachable: " + p)
    missing += out
lines.append("")
# soft paths used by C++ at runtime (defaults in constructors): they must exist AND be reachable
soft = []
cdo = unreal.get_default_object(unreal.P9CampaignDirector)
for prop in ("lantern_man_mesh", "greeting_stand_mesh", "greeting_lunge_mesh", "bridge_south_mesh", "bridge_end_mesh", "bridge_bay_mesh", "bridge_lead_in_mesh",
             "bridge_tube_mesh", "escalator_upper_mesh", "escalator_step_mesh"):
    soft.append(("director." + prop, str(cdo.get_editor_property(prop).get_path_name() if hasattr(cdo.get_editor_property(prop), "get_path_name") else cdo.get_editor_property(prop))))
for d in cdo.get_editor_property("anomalies"):
    for v in d.get_editor_property("variants"):
        for prop in ("material", "sound", "mesh"):
            sp = v.get_editor_property(prop)
            s = str(sp)
            if s and s != "None" and "/Game/" in s:
                soft.append(("%s.%s.%s" % (d.get_editor_property("id"), v.get_editor_property("id"), prop), s))
for cls in (unreal.P9AudioDirector,):
    o = unreal.get_default_object(cls)
    for prop in ("main_ambience", "platform9_horror", "anomaly_ambience", "epilogue_music", "platform_wind", "footbridge_bed", "jump_sound", "land_sound",
                 "kanchana_breathing", "kanchana_presence"):
        soft.append(("audio." + prop, str(o.get_editor_property(prop))))
    for prop in ("steps_platform", "steps_stairs", "steps_footbridge", "kanchana_steps_close", "kanchana_steps_distant"):
        for i, x in enumerate(o.get_editor_property(prop)):
            soft.append(("audio.%s[%d]" % (prop, i), str(x)))
for n in ("SFX_LanternMan_Flame_Loop", "SFX_LanternMan_LowRumble_Loop", "SFX_LanternMan_ElectricalFlicker", "SFX_LanternMan_Stinger",
          "SFX_Greeting_Scream", "SFX_Greeting_DistortedLayer", "SFX_Greeting_JumpscareImpact"):
    soft.append(("code path " + n, "/Game/P9/Audio/OriginalAudioPack/Creatures/%s.%s" % (n, n)))
bad_soft = 0
for name, s in soft:
    import re
    m = re.search(r"(/Game/[^'\"\s]+)", s)
    if not m:
        continue
    pkg = m.group(1).split(".")[0]
    exists = EAL.does_asset_exist(pkg)
    reach = pkg in seen
    if not exists or not reach:
        bad_soft += 1
    lines.append("%-55s %-80s exists=%s cooked-via-map=%s" % (name, pkg, exists, reach))
lines.append("")
lines.append("RESULT: %d Stage 8A assets not reachable from the map, %d soft paths missing or not reachable" % (len(missing), bad_soft))
open(OUT, "w").write("\n".join(lines) + "\n")
unreal.log("P9PY: audit packages %d; 8A not reachable %d; soft bad %d of %d" % (len(seen), len(missing), bad_soft, len(soft)))
for p in missing:
    unreal.log("P9PY: NOT REACHABLE " + p)
