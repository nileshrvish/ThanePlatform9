# Stage 7B (PIE only, runtime copy): freeze NPC KEY in single-node mode on CLIP property at time POS (rate 0) at a flat spot,
# facing +X, and log each foot's ankle / toe joint position relative to the actor's feet (x forward, y right, z up).
import unreal
KEY = globals().get("KEY", "F2"); PROP = globals().get("PROP", "stairs_up_anim"); POS = globals().get("POS", 0.0)
world = unreal.UnrealEditorSubsystem().get_game_world()
a = next(x for x in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(x.get_editor_property("friend_key")) == KEY)
READ = globals().get("READ", False)
b = a.get_editor_property("body")
clip = a.get_editor_property(PROP)
if not READ:
  a.set_actor_tick_enabled(False)
  a.set_actor_location(unreal.Vector(0, 2525, 690), False, True); a.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
  b.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
  b.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
  b.play_animation(clip, True); b.set_position(POS, False); b.set_play_rate(0.0)
L = a.get_actor_location()
out = []
for s in ("Left", "Right"):
    for bone in ("Foot", "ToeBase"):
        p = b.get_socket_location(s + bone)
        out.append("%s%s (%.1f, %.1f, %.1f)" % (s[0], bone, p.x - L.x, p.y - L.y, p.z - (L.z - 90)))
unreal.log("P9PY: freeze %s %s t=%.2f: %s" % (KEY, clip.get_name(), POS, ", ".join(out)))
