# Stage 7B (PIE, read-only): per-frame sampler. For DURATION seconds of game time it samples the NPCs in KEYS (friend keys, or
# "KN" for Kanchana): clip, location, and per foot the toe joint's height above the floor straight under it (Visibility trace)
# and its horizontal speed. A foot counts as planted when the toe moves < 12 cm/s. Writes a summary to the log (P9PY) and the
# raw rows to OUT (csv). Distances between every pair of visible bodies are tracked too (min over the run).
import unreal, math, json
KEYS = globals().get("KEYS", ["F1"]); DURATION = globals().get("DURATION", 30.0)
OUT = globals().get("OUT", r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad\probe_7b.csv")
world = unreal.UnrealEditorSubsystem().get_game_world()
acts = {}
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC): acts[str(a.get_editor_property("friend_key"))] = a
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9SilhouetteStalker): acts["KN"] = a
bodies = {k: (a, a.get_editor_property("body_radius")) for k, a in acts.items()}
for k in KEYS:
    acts[k].get_editor_property("body").set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES, unreal.PropertyAccessChangeNotifyMode.NEVER)
def hidden(a):
    try: return bool(a.get_editor_property("hidden"))
    except Exception: return not a.is_actor_tick_enabled()
state = {"t": 0.0, "rows": [], "prev": {}, "mind": {}, "clips": {}}
def floor_z(p, ignore):
    h = unreal.SystemLibrary.line_trace_single(world, unreal.Vector(p.x, p.y, p.z + 30), unreal.Vector(p.x, p.y, p.z - 150),
                                               unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, ignore, unreal.DrawDebugTrace.NONE, True)
    t = h.to_tuple()
    return t[4].z if t[0] else None
def tick(dt):
    if not world or not unreal.UnrealEditorSubsystem().get_game_world():
        unreal.unregister_slate_post_tick_callback(handle); return
    now = unreal.GameplayStatics.get_time_seconds(world)
    if now <= state.get("now", -1.0): return            # the editor UI ticked without a game frame: no new sample
    dt = now - state["now"] if "now" in state else 0.016
    state["now"] = now
    state["t"] += dt
    for k in KEYS:
        a = acts[k]
        if hidden(a): continue
        b = a.get_editor_property("body")
        an = b.get_anim_instance()
        st = []
        for side in ("Left", "Right"):
            toe = b.get_socket_location(side + "ToeBase")
            fz = floor_z(toe, [a])
            key = (k, side)
            pv = state["prev"].get(key)
            sp = math.hypot(toe.x - pv[0], toe.y - pv[1]) / max(dt, 1e-3) if pv else 99.0
            state["prev"][key] = (toe.x, toe.y)
            st.append((round(toe.z - fz, 2) if fz is not None else None, round(sp, 1)))
        loc = a.get_actor_location()
        state["rows"].append([round(state["t"], 3), k, round(loc.x, 1), round(loc.y, 1), round(loc.z, 1), st[0][0], st[0][1], st[1][0], st[1][1]])
    # body spacing
    vis = [(k, a, r) for k, (a, r) in bodies.items() if not hidden(a)]
    for i in range(len(vis)):
        for j in range(i + 1, len(vis)):
            (k1, a1, r1), (k2, a2, r2) = vis[i], vis[j]
            l1, l2 = a1.get_actor_location(), a2.get_actor_location()
            f1 = l1.z - (0 if k1 == "KN" else 90); f2 = l2.z - (0 if k2 == "KN" else 90)
            if abs(f1 - f2) > 120: continue
            d = math.hypot(l1.x - l2.x, l1.y - l2.y) - r1 - r2
            kk = k1 + "-" + k2
            if d < state["mind"].get(kk, (1e9,))[0]: state["mind"][kk] = (round(d, 1), round(state["t"], 1))
    if state["t"] >= DURATION:
        unreal.unregister_slate_post_tick_callback(handle)
        summ = []
        # split by what the body was doing: stairs (moving on a PF1/PF9-10 flight), flat walking, standing
        def cat(r, prev):
            moving = prev is not None and math.hypot(r[2] - prev[2], r[3] - prev[3]) > 0.2
            onst = (-1370 < r[2] < -230 and -1660 < r[3] < -1390) or (300 < r[2] < 1400 and 7290 < r[3] < 7535)
            return ("stairs" if onst else "flat") if moving else "stand"
        lastrow = {}
        for r in state["rows"]:
            r.append(cat(r, lastrow.get(r[1]))); lastrow[r[1]] = r
        for k in KEYS:
            for c in ("stairs", "flat", "stand"):
                rr = [r for r in state["rows"] if r[1] == k and r[-1] == c]
                pl = sorted([r[5] for r in rr if r[5] is not None and r[6] < 12] + [r[7] for r in rr if r[7] is not None and r[8] < 12])
                sl = [r[6] for r in rr if r[6] < 12] + [r[8] for r in rr if r[8] < 12]
                low = [min(v for v in (r[5], r[7]) if v is not None) for r in rr if r[5] is not None or r[7] is not None]
                if low:
                    lo = sorted(low)
                    summ.append("%s %-6s stance(lower toe)-floor cm: p10 %.1f median %.1f p90 %.1f (frames %d)" % (k, c, lo[len(lo) // 10], lo[len(lo) // 2], lo[9 * len(lo) // 10], len(lo)))
                if False:
                    summ.append("%s %-6s planted toe-floor cm: min %.1f p10 %.1f median %.1f p90 %.1f max %.1f (n %d) | lowest toe per frame: min %.1f median %.1f | planted toe speed %.1f cm/s" % (
                        k, c, pl[0], pl[len(pl) // 10], pl[len(pl) // 2], pl[9 * len(pl) // 10], pl[-1], len(pl), min(low), sorted(low)[len(low) // 2], sum(sl) / len(sl)))
        for k in []:
            pl = [r[5] for r in state["rows"] if r[1] == k and r[5] is not None and r[6] < 12] + [r[7] for r in state["rows"] if r[1] == k and r[7] is not None and r[8] < 12]
            sl = [r[6] for r in state["rows"] if r[1] == k and r[6] < 12] + [r[8] for r in state["rows"] if r[1] == k and r[8] < 12]
            zs = [r[4] for r in state["rows"] if r[1] == k]
            if pl:
                pl.sort()
                summ.append("%s planted toe-floor cm: min %.1f p10 %.1f median %.1f p90 %.1f max %.1f (n %d); planted toe speed mean %.1f cm/s; z %.0f..%.0f" % (
                    k, pl[0], pl[len(pl) // 10], pl[len(pl) // 2], pl[9 * len(pl) // 10], pl[-1], len(pl), sum(sl) / len(sl), min(zs), max(zs)))
            else:
                summ.append("%s no planted samples (z %s)" % (k, (min(zs), max(zs)) if zs else "-"))
        unreal.log("P9PY: probe %.1fs\n" % state["t"] + "\n".join(summ) + "\nmin gap between bodies (cm, surface to surface): " + json.dumps(state["mind"]))
import builtins
for h in getattr(builtins, "P9_PROBES", []):
    try: unreal.unregister_slate_post_tick_callback(h)
    except Exception: pass
handle = unreal.register_slate_post_tick_callback(tick)
builtins.P9_PROBES = [handle]                 # a later probe (or _unreg) can remove it
unreal.log("P9PY: probe started %s for %.0fs" % (KEYS, DURATION))
