# Stage 7B (PIE only, runtime copies): give the NPCs in ROUTES a test route and restart their routines (availability toggle via
# p9.Friends.ForceUnlock is done by the caller: 0 then 1). ROUTES = {key: [(x, y, z, stop), ...]}, stop "W" wait / "S" stand / "P" pass.
# Roles become Commuter (end to end, short waits). NPCs not listed keep their own routines.
import unreal
NN = unreal.PropertyAccessChangeNotifyMode.NEVER
ROUTES = globals().get("ROUTES", {})
WAIT = globals().get("WAIT", (1.0, 2.0))
world = unreal.UnrealEditorSubsystem().get_game_world()
npcs = {str(a.get_editor_property("friend_key")): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC)}
STOP = {"W": unreal.P9NPCStop.WAIT, "S": unreal.P9NPCStop.STAND, "P": unreal.P9NPCStop.PASS}
out = []
for k, pts in ROUTES.items():
    n = npcs[k]
    route = []
    for (x, y, z, s) in pts:
        p = unreal.P9NPCRoutePoint()
        p.set_editor_property("location", unreal.Vector(x, y, z), NN); p.set_editor_property("stop", STOP[s], NN)
        p.set_editor_property("min_wait", WAIT[0], NN); p.set_editor_property("max_wait", WAIT[1], NN)
        route.append(p)
    n.set_editor_property("route", route, NN); n.set_editor_property("start_index", 0, NN)
    n.set_editor_property("npc_role", unreal.P9NPCRole.COMMUTER, NN)
    n.set_editor_property("drunk_chance", 0.0, NN)
    n.set_editor_property("ride_epilogue_train", False, NN)
    n.get_editor_property("body").set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES, NN)
    out.append("%s %d pts" % (k, len(route)))
unreal.log("P9PY: test routes " + ", ".join(out))
