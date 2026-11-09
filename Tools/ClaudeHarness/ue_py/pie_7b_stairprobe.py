# Stage 7B (PIE, read-only apart from the tick option): per game frame, for every station NPC on a PF1 flight (lower: risers
# every 30 cm from x -1370, tops k*16.667; upper: from x -710, tops 300 + k*16.667), bones refreshed every frame:
#   stance = the lower foot: toe joint height above the tread under the foot's centre (correct plant: about +2 cm, the toe
#            joint's own height above the sole - measured +1.9 on the frozen clip), and its horizontal speed (sliding);
#   swing  = the higher foot: toe joint clearance above the tread straight under the toe (negative = toe in a step).
# Summary per NPC and direction (up/down) after DURATION s.
import unreal, math, builtins
DURATION = globals().get("DURATION", 60.0)
R, T = 50.0 / 3.0, 30.0
FLIGHTS = [(-1370.0, -860.0, 0.0), (-710.0, -230.0, 300.0)]           # first riser x, last x, base z
world = unreal.UnrealEditorSubsystem().get_game_world()
npcs = {str(a.get_editor_property("friend_key")): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC)}
def tread_top(x):
    for x0, x1, z0 in FLIGHTS:
        if x0 - 30 < x <= x1 + 30:
            k = math.ceil((x - x0) / T)
            k = max(0, min(k, 18))
            return z0 + k * R
    return None
st = {"t": 0.0, "now": -1.0, "prev": {}, "data": {}}
def tick(_):
    w = unreal.UnrealEditorSubsystem().get_game_world()
    if not w:
        unreal.unregister_slate_post_tick_callback(handle); return
    now = unreal.GameplayStatics.get_time_seconds(world)
    if now <= st["now"]: return
    dt = now - st["now"] if st["now"] > 0 else 0.016
    st["now"] = now; st["t"] += dt
    for k, a in npcs.items():
        b = a.get_editor_property("body")
        b.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES, unreal.PropertyAccessChangeNotifyMode.NEVER)
        L = a.get_actor_location()
        if not (-1400 < L.x < -200 and -1660 < L.y < -1390): continue
        if tread_top(L.x) is None: continue
        feet = []
        for s in ("Left", "Right"):
            an = b.get_socket_location(s + "Foot"); to = b.get_socket_location(s + "ToeBase")
            feet.append((an, to))
        yaw = a.get_actor_rotation().yaw
        d = "up" if -90 < yaw < 90 else "down"
        # planted foot = the foot whose toe moved least since the last frame (both feet known)
        pv = st["prev"].get(k)
        st["prev"][k] = [(f[1].x, f[1].y) for f in feet]
        if not pv: continue
        sp = [math.hypot(feet[i][1].x - pv[i][0], feet[i][1].y - pv[i][1]) / max(dt, 1e-3) for i in range(2)]
        si = 0 if sp[0] <= sp[1] else 1
        an, to = feet[si]; sw = feet[1 - si][1]
        c = (an.x + to.x) / 2.0
        top = tread_top(c); tops = tread_top(sw.x)
        if top is None or tops is None: continue
        st["data"].setdefault((k, d), []).append((to.z - top, sw.z - tops, sp[si]))
    if st["t"] >= DURATION:
        unreal.unregister_slate_post_tick_callback(handle)
        out = []
        for (k, d), rows in sorted(st["data"].items()):
            s = sorted(r[0] for r in rows); w = sorted(r[1] for r in rows); v = sorted(r[2] for r in rows if r[2] is not None)
            q = lambda arr, f: arr[min(len(arr) - 1, int(f * len(arr)))] if arr else float("nan")
            out.append("%s %-4s frames %4d | stance toe above tread cm: p5 %+.1f p50 %+.1f p95 %+.1f | swing toe clearance: min %+.1f p5 %+.1f | stance toe speed cm/s: p50 %.1f p90 %.1f" % (
                k, d, len(rows), q(s, 0.05), q(s, 0.5), q(s, 0.95), w[0], q(w, 0.05), q(v, 0.5), q(v, 0.9)))
        for line in out: unreal.log("P9PY: stairprobe " + line)
        unreal.log("P9PY: stairprobe done %.0fs" % st["t"])
for h in getattr(builtins, "P9_PROBES", []):
    try: unreal.unregister_slate_post_tick_callback(h)
    except Exception: pass
handle = unreal.register_slate_post_tick_callback(tick)
builtins.P9_PROBES = [handle]
unreal.log("P9PY: stairprobe started %.0fs" % DURATION)
