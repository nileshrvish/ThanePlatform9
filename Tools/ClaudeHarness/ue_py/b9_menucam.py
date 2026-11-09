# Stage 9B: the menu background camera - east end of Platform 9 looking west along the platform edge (does not save the map)
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
m = next(a for a in eas.get_all_level_actors() if a.get_actor_label() == "S9B_MainMenu")
m.set_editor_property("camera_from", unreal.Vector(2900, 7010, 235))
m.set_editor_property("camera_to", unreal.Vector(2250, 7050, 225))
m.set_editor_property("camera_rotation", unreal.Rotator(0.0, -3.0, 181.0))   # unreal.Rotator(roll, pitch, yaw) in Python
r = m.get_editor_property("camera_rotation")
unreal.log("P9PY: menu camera %s -> %s rot pitch %.1f yaw %.1f roll %.1f" % (m.get_editor_property("camera_from"), m.get_editor_property("camera_to"), r.pitch, r.yaw, r.roll))
