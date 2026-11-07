# Stage 5E (PIE only, runtime copy): play a clip in place on one NPC on open floor for screenshots. Set KEY, CLIP, POS (time).
import unreal
KEY = globals().get("KEY", "F1"); CLIP = globals().get("CLIP", "/Game/P9/Characters/Station3D/dhanraj/MX/SK_P9_NPC3D_dhanraj_MX_Walk")
POS = globals().get("POS", None)
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == KEY)
npc.set_actor_tick_enabled(False)
npc.set_actor_location(unreal.Vector(-60, 2700, 690), False, False); npc.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
b = npc.get_editor_property("body")
b.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
b.set_component_tick_enabled(True); b.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
b.play_animation(unreal.load_asset(CLIP), True)
if POS is not None: b.set_position(POS, False); b.set_play_rate(0.0)
unreal.log("P9PY: showing %s on %s" % (CLIP.split("/")[-1], KEY))
