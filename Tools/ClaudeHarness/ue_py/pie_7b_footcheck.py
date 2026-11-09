# Stage 7B (PIE, read-only): for NPCs on the PF1 lower flight (risers every 30 cm from x -1370, tops at k * 16.667) report each
# foot: the tread under the foot's centre (between ankle and toe joint), the toe joint height above that tread, the toe's
# overhang past the tread's nosing/riser, and the actor's feet height vs the stair ramp. Compare the toe height with the same
# NPC's planted toe height on flat floor (FLAT).
import unreal, math
FLAT = {"F1": -3.5, "F2": -9.8, "F3": -9.9, "F4": -9.5, "F5": -3.5, "CW": -15.5}
R, T, X0 = 50.0 / 3.0, 30.0, -1370.0
world = unreal.UnrealEditorSubsystem().get_game_world()
out = []
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC):
    k = str(a.get_editor_property("friend_key")); L = a.get_actor_location()
    if not (-1360 < L.x < -870 and -1660 < L.y < -1390): continue
    b = a.get_editor_property("body")
    feet = L.z - 90.0
    d = L.x - X0
    ramp = R * (d / T + 0.5)
    v = feet / (2 * R); ph = v - math.floor(v)
    rel = []
    for s in ("Left", "Right"):
        an = b.get_socket_location(s + "Foot")
        rel.append("%s(%.1f,%.1f)" % (s[0], (an.x - L.x) * (1 if a.get_actor_rotation().yaw > -90 and a.get_actor_rotation().yaw < 90 else -1), an.z - feet))
    row = "%s x%.0f feet %.1f ramp %.1f phase %.2f yaw %.0f ankles rel %s |" % (k, L.x, feet, ramp, ph, a.get_actor_rotation().yaw, " ".join(rel))
    for s in ("Left", "Right"):
        an = b.get_socket_location(s + "Foot"); to = b.get_socket_location(s + "ToeBase")
        c = (an.x + to.x) / 2.0
        kk = max(0, math.ceil((c - X0) / T))           # tread k spans (X0 + (k-1)T, X0 + kT]
        top = kk * R
        rel = to.z - top
        row += " %s centre x%.0f tread %d (%.0f..%.0f) toe %+.1f vs flat %+.1f => %+.1f |" % (s[0], c, kk, X0 + (kk - 1) * T, X0 + kk * T, rel, FLAT[k], rel - FLAT[k])
    out.append(row)
for r in out: unreal.log("P9PY: fc " + r)
if not out: unreal.log("P9PY: fc none on the lower flight " + str([(str(a.get_editor_property("friend_key")), round(a.get_actor_location().x), round(a.get_actor_location().y)) for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC)]))
