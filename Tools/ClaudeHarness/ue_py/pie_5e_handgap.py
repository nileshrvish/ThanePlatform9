# Stage 5E (PIE only): how far the hands hang out from the body in a clip (component space |y| of hand vs upperarm).
import unreal
KEY = globals().get("KEY", "CW"); CLIPS = globals().get("CLIPS", [])
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == KEY)
b = npc.get_editor_property("body")
for s in ("upperarm_l", "lowerarm_l", "hand_l", "hand_r"):
    t = b.get_socket_transform(s, unreal.RelativeTransformSpace.RTS_COMPONENT).translation
    unreal.log("P9PY: handgap %s %s: x %.1f y %.1f z %.1f" % (KEY, s, t.x, t.y, t.z))
