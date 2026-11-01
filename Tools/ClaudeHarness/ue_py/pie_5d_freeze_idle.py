# Stage 5D test (PIE only): freeze the chaiwala's PIE copy in his idle clip, frame 0, so hand offsets can be measured.
import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
cw = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "CW")
cw.set_actor_tick_enabled(False)
b = cw.get_editor_property("body")
b.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
b.play_animation(cw.get_editor_property("idle_anim"), False); b.set_position(0.0, False); b.set_play_rate(0.0)
unreal.log("P9PY: chaiwala frozen in idle frame 0 (PIE copy)")
cw.set_actor_location(unreal.Vector(-60, 2700, 690), False, False)
cw.set_actor_rotation(unreal.Rotator(0, 0, 90), False)
unreal.log("P9PY: frozen chaiwala moved to the open concourse (PIE copy)")
