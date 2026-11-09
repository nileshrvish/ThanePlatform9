import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC):
    k = str(a.get_editor_property("friend_key"))
    if k not in ("F1", "F2", "F3"): continue
    b = a.get_editor_property("body")
    names = [str(n) for n in b.get_all_socket_names()]
    out = []
    for n in ("LeftToeBase", "LeftFoot", "LeftToe_End", "RightToeBase"):
        if n in names:
            p = b.get_socket_location(n); out.append("%s z%.1f" % (n, p.z))
        else:
            out.append("%s missing" % n)
    h = unreal.SystemLibrary.line_trace_single(world, a.get_actor_location(), a.get_actor_location() - unreal.Vector(0, 0, 300), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [a], unreal.DrawDebugTrace.NONE, True).to_tuple()
    unreal.log("P9PY: %s actor z %.1f comp z %.1f floor %.1f %s ; bones %d leaf %s" % (k, a.get_actor_location().z, b.get_socket_location("None").z,
               h[4].z if h[0] else -1, ", ".join(out), len(names), [n for n in names if "Toe" in n]))
