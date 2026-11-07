# Stage 5E (PIE only, runtime copy): measure the natural speed of a retargeted walk clip. Called repeatedly: each call
# reads the feet of the pose set by the previous call, then steps the clip on. State in a JSON file. Set KEY + CLIP.
import unreal, json, os
STATE = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\footspeed.json"
KEY = globals().get("KEY", "F1"); CLIP = globals().get("CLIP", "/Game/P9/Characters/Station3D/dhanraj/MC/SK_P9_NPC3D_dhanraj_MC_Walk")
N = 20
st = json.load(open(STATE)) if os.path.exists(STATE) else {"i": -1, "samples": [], "clip": CLIP}
if st.get("clip") != CLIP: st = {"i": -1, "samples": [], "clip": CLIP}
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == KEY)
b = npc.get_editor_property("body"); anim = unreal.load_asset(CLIP); L = anim.get_play_length()
if st["i"] < 0:
    npc.set_actor_tick_enabled(False); b.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    b.set_editor_property("visibility_based_anim_tick_option", unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
    b.set_component_tick_enabled(True)
    b.play_animation(anim, True); b.set_play_rate(0.0)
else:
    f = [b.get_socket_transform(s, unreal.RelativeTransformSpace.RTS_COMPONENT).translation for s in ("foot_l", "foot_r")]
    st["samples"].append([st["i"] * L / N, f[0].x, f[0].z, f[1].x, f[1].z, f[0].y, f[1].y])
st["i"] += 1
if st["i"] <= N:
    b.set_position(st["i"] * L / N, False)
    json.dump(st, open(STATE, "w"))
    unreal.log("P9PY: footspeed sample %d/%d" % (st["i"], N))
else:
    # stance = the lower foot; its backward speed along the facing (component +X... mesh faces +X) is the walk speed
    sp = []
    s = st["samples"]
    for k in range(1, len(s)):
        dt = s[k][0] - s[k - 1][0]
        for fx, fz in ((1, 2), (3, 4)):
            low = s[k][fz] <= min(s[k][2], s[k][4]) + 0.5
            if low and dt > 0: sp.append(-(s[k][fx] - s[k - 1][fx]) / dt)
    sp.sort()
    med = sp[len(sp) // 2] if sp else 0
    xs_l = [r[1] for r in s]; xs_r = [r[3] for r in s]; ys = [round(r[1], 1) for r in s]
    exc = ((max(xs_l) - min(xs_l)) + (max(xs_r) - min(xs_r))) / 2.0
    unreal.log("P9PY: foot_l x samples %s" % ys)
    unreal.log("P9PY: mean foot y l %.1f r %.1f" % (sum(r[5] for r in s) / len(s), sum(r[6] for r in s) / len(s)))
    unreal.log("P9PY: foot y excursion l %.1f r %.1f | z range l %.1f..%.1f" % (max(r[5] for r in s) - min(r[5] for r in s), max(r[6] for r in s) - min(r[6] for r in s), min(r[2] for r in s), max(r[2] for r in s)))
    unreal.log("P9PY: foot excursion %.1f cm -> speed ~ %.1f cm/s (2*excursion/len)" % (exc, 2 * exc / L))
    unreal.log("P9PY: footspeed %s len %.2fs median stance speed %.1f cm/s (n=%d, range %.0f..%.0f)" % (CLIP.split("/")[-1], L, med, len(sp), sp[0] if sp else 0, sp[-1] if sp else 0))
    os.remove(STATE)
