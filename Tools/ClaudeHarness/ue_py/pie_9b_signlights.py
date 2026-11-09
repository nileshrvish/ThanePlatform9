# Stage 9B (PIE): switch the 18 S9A_SignGlow lights on / off (mode file) for a frame-time comparison; reports how many were set.
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
on = open(unreal.Paths.project_dir() + "Tools/ClaudeHarness/ab_mode.txt").read().strip() == "on"
n = 0
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PointLight):
    if a.get_actor_label().startswith("S9A_SignGlow_") if hasattr(a, "get_actor_label") else a.get_name().startswith("S9A_SignGlow_"):
        a.get_component_by_class(unreal.PointLightComponent).set_visibility(on)
        n += 1
unreal.log("P9PY: sign glow lights %s: %d" % ("on" if on else "off", n))
