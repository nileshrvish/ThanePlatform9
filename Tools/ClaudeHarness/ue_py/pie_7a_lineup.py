# Stage 7A (PIE only, runtime copies): the six station NPCs side by side on the concourse floor, each playing its OWN clip
# from the property PROP (idle_anim / walk_anim / stairs_up_anim / sit_idle_anim ...), for comparison screenshots.
# NPC ticks are disabled (restart PIE afterwards). Facing +X (towards a camera at x 330).
import unreal
PROP = globals().get("PROP", "idle_anim"); POS = globals().get("POS", None)
world = unreal.UnrealEditorSubsystem().get_game_world()
order = ["F1", "F2", "F3", "F4", "F5", "CW"]
npcs = {str(a.get_editor_property("friend_key")): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC)}
out = []
for i, k in enumerate(order):
    npc = npcs[k]
    clip = npc.get_editor_property(PROP)
    npc.set_actor_tick_enabled(False)
    npc.set_actor_hidden_in_game(False)
    npc.set_actor_location(unreal.Vector(-60, 2525 + i * 130, 690), False, True); npc.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    b = npc.get_editor_property("body")
    b.set_hidden_in_game(False)
    b.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    b.set_component_tick_enabled(True); b.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    if clip:
        b.play_animation(clip, True)
        if POS is not None:
            b.set_position(min(POS, clip.get_play_length() - 0.01), False); b.set_play_rate(0.0)
    out.append("%s:%s" % (k, clip.get_name() if clip else "-"))
unreal.log("P9PY: lineup %s %s" % (PROP, out))
