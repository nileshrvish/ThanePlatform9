# Stage 7B (PIE, read-only): every game frame for DURATION s, surface-to-surface gap between every pair of visible station
# bodies (friends, chaiwala, Kanchana, player; capsule radii) on the same level. Logs per pair: min gap, time below 0
# (overlap), and per NPC the longest time without moving (stuck check). Also logs NPC clip changes count.
import unreal, math, builtins
DURATION = globals().get("DURATION", 60.0)
world = unreal.UnrealEditorSubsystem().get_game_world()
B = {}
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC): B[str(a.get_editor_property("friend_key"))] = (a, a.get_editor_property("body_radius"), 90.0)
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9SilhouetteStalker): B["KN"] = (a, a.get_editor_property("body_radius"), 0.0)
pp = unreal.GameplayStatics.get_player_pawn(world, 0)
B["PL"] = (pp, 34.0, 90.0)
st = {"t": 0.0, "now": -1.0, "min": {}, "over": {}, "last": {}, "still": {}, "maxstill": {}}
def vis(k, a):
    if k == "PL": return True
    try: return not bool(a.get_editor_property("hidden"))
    except Exception: return a.is_actor_tick_enabled()
def tick(_):
    if not unreal.UnrealEditorSubsystem().get_game_world():
        unreal.unregister_slate_post_tick_callback(handle); return
    now = unreal.GameplayStatics.get_time_seconds(world)
    if now <= st["now"]: return
    dt = now - st["now"] if st["now"] > 0 else 0.016
    st["now"] = now; st["t"] += dt
    P = {k: (a.get_actor_location(), r, off) for k, (a, r, off) in B.items() if vis(k, a)}
    ks = sorted(P)
    for i in range(len(ks)):
        for j in range(i + 1, len(ks)):
            (l1, r1, o1), (l2, r2, o2) = P[ks[i]], P[ks[j]]
            if abs((l1.z - o1) - (l2.z - o2)) > 120: continue
            g = math.hypot(l1.x - l2.x, l1.y - l2.y) - r1 - r2
            kk = ks[i] + "-" + ks[j]
            if g < st["min"].get(kk, (1e9, 0))[0]: st["min"][kk] = (round(g, 1), round(st["t"], 1))
            if g < 0: st["over"][kk] = st["over"].get(kk, 0.0) + dt
    for k, (l, r, o) in P.items():
        if k == "PL": continue
        pl = st["last"].get(k)
        moved = pl is None or math.hypot(l.x - pl.x, l.y - pl.y) > 0.5
        st["last"][k] = l
        st["still"][k] = 0.0 if moved else st["still"].get(k, 0.0) + dt
        st["maxstill"][k] = max(st["maxstill"].get(k, 0.0), st["still"][k])
    if st["t"] >= DURATION:
        unreal.unregister_slate_post_tick_callback(handle)
        close = {k: v for k, v in st["min"].items() if v[0] < 60}
        unreal.log("P9PY: gaps %.0fs min gap (cm, at s) for pairs that came within 60 cm: %s" % (st["t"], close))
        unreal.log("P9PY: gaps overlap seconds: %s" % {k: round(v, 2) for k, v in st["over"].items()})
        unreal.log("P9PY: gaps longest still (s): %s" % {k: round(v, 1) for k, v in st["maxstill"].items()})
for h in getattr(builtins, "P9_PROBES", []):
    try: unreal.unregister_slate_post_tick_callback(h)
    except Exception: pass
handle = unreal.register_slate_post_tick_callback(tick)
builtins.P9_PROBES = [handle]
unreal.log("P9PY: gaps started %.0fs %s" % (DURATION, sorted(B)))
