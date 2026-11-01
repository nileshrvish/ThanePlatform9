# Stage 5D (PIE, read-only): where the chaiwala's held props really are, vs his hands.
import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
cw = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "CW")
b = cw.get_editor_property("body")
for c in cw.get_components_by_class(unreal.StaticMeshComponent):
    t = c.get_world_transform()
    unreal.log("P9PY: comp %s mesh=%s vis=%s parent=%s socket=%s loc=%s rot=%s scale=%s rel=%s" % (c.get_name(), c.static_mesh.get_name() if c.static_mesh else None, c.is_visible(),
               c.get_attach_parent().get_name() if c.get_attach_parent() else None, c.get_attach_socket_name(), t.translation, t.rotation.rotator(), t.scale3d, c.get_editor_property("relative_location")))
for s in ("hand_r", "hand_l"):
    t = b.get_socket_transform(s, unreal.RelativeTransformSpace.RTS_WORLD)
    unreal.log("P9PY: %s loc=%s rot=%s" % (s, t.translation, t.rotation.rotator()))
