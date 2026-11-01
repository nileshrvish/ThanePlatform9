# Stage 5B test helper - PIE world ONLY (runtime copies, never saved): turn on the new 5B instance flags so the
# behaviour can be tested before the Step 3 map edit. Chaiwala SpeechCooldown is set to 0 here so that only the
# once-per-day rule can be what blocks a second speech.
import unreal

world = unreal.UnrealEditorSubsystem().get_game_world()
if not world or "UEDPIE" not in world.get_path_name():
    unreal.log("P9PY: pie_5b_flags - no PIE world, nothing changed")
else:
    n = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC):
        key = str(a.get_editor_property("friend_key"))
        if key in ("F1", "F2", "F3", "F4", "F5"):
            a.set_editor_property("hide_until_campaign_complete", True); n += 1
        elif key == "CW":
            a.set_editor_property("once_per_campaign_day", True)
            a.set_editor_property("speech_cooldown", 0.0)
    for s in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9SilhouetteStalker):
        s.set_editor_property("appear_in_epilogue", True)
    unreal.log("P9PY: pie_5b_flags set on %s - %d friends locked, chaiwala daily (cooldown 0), silhouette epilogue override" % (world.get_path_name(), n))
