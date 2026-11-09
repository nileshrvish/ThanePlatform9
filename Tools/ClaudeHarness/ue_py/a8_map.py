# Stage 8A map setup (editor world; does NOT save). Idempotent: points / actors found by label are updated, not duplicated.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EAL = unreal.EditorAssetLibrary
out = []
def log(s):
    out.append(s); unreal.log("P9PY: " + s)

actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
def A(label):
    a = actors.get(label)
    if not a:
        raise RuntimeError("missing actor " + label)
    return a

director = next(a for a in actors.values() if isinstance(a, unreal.P9CampaignDirector))
L = unreal.P9AnomalyLocation

def point(label, pid, loc_type, loc, yaw=0.0, target=None, secondary=None, extras=None, comp=None):
    p = actors.get(label)
    if p is None:
        p = eas.spawn_actor_from_class(unreal.P9AnomalySpawnPoint, unreal.Vector(*loc), unreal.Rotator(0, 0, yaw))
        p.set_actor_label(label)
        p.set_folder_path("P9/Campaign/AnomalyPoints8A")
        actors[label] = p
    p.set_actor_location(unreal.Vector(*loc), False, False)
    p.set_actor_rotation(unreal.Rotator(0, 0, yaw), False)
    p.set_editor_property("point_id", pid)
    p.set_editor_property("location_type", loc_type)
    p.set_editor_property("target_actor", A(target) if target else None)
    p.set_editor_property("secondary_actor", A(secondary) if secondary else None)
    p.set_editor_property("extra_actors", [A(x) for x in (extras or [])])
    p.set_editor_property("target_component", comp or "None")
    return p

new_points = [
    point("S8A_Point_REDLIGHT_W1", "REDLIGHT_W1", L.PLATFORM_LAMP, (-1900, 7410, 450), 0, "S2C_Lamp_PF9_10_W1", "S3B3_TubeLight_PF9_10_W1", ["S3C_Fill_PF9_10_W1"]),
    point("S8A_Point_REDLIGHT_W2", "REDLIGHT_W2", L.PLATFORM_LAMP, (-700, 7410, 450), 0, "S2C_Lamp_PF9_10_W2", "S3B3_TubeLight_PF9_10_W2", ["S3C_Fill_PF9_10_W2"]),
    point("S8A_Point_REDLIGHT_E", "REDLIGHT_E", L.PLATFORM_LAMP, (1900, 7410, 450), 0, "S2C_Lamp_PF9_10_E", "S3B3_TubeLight_PF9_10_E", ["S3C_Fill_PF9_10_E"]),
    point("S8A_Point_TRAINSEATS_Vashi", "TRAINSEATS_Vashi", L.TRAIN_SEATS, (0, 6740, 200), 0, "S4B_VashiLocal_PF9"),
    point("S8A_Point_TRAINLIGHT_Car0", "TRAINLIGHT_Car0", L.TRAIN_LIGHTS, (1066, 6740, 300), 0, "S4B_VashiLocal_PF9", comp="Car0"),
    point("S8A_Point_TRAINLIGHT_Car1", "TRAINLIGHT_Car1", L.TRAIN_LIGHTS, (-1066, 6740, 300), 0, "S4B_VashiLocal_PF9", comp="Car1"),
    point("S8A_Point_LANTERN_PF9East", "LANTERN_PF9East", L.LANTERN_SPOT, (2870, 7700, 0), 190, "S2C_Lamp_PF9_10_E", "S3B3_TubeLight_PF9_10_E", ["S3C_Fill_PF9_10_E"]),
]
# the two Stage 6A loop points get their Stage 8A setup
pts = {str(p.get_editor_property("point_id")): p for p in actors.values() if isinstance(p, unreal.P9AnomalySpawnPoint)}
fob = pts["LOOP_FOB"]
fob.set_editor_property("loop_trigger", unreal.Box(unreal.Vector(-195, 6300, 540), unreal.Vector(195, 6550, 760)))
fob.set_editor_property("loop_direction", unreal.Vector(0, 1, 0))
fob.set_editor_property("extra_actors", [A(x) for x in ("S3C_FOBShell_B", "S3B2_FOBFloor_DeckB", "S3B1_FOBSign_PF9_10", "S3B3_TubeLight_FOB_7410", "S2C_FOB_7410",
                                                         "S3B3_CCTV_FOB_N", "S2_FOB_Parapet_N_End")])
stair = pts["LOOP_Stair"]
stair.set_editor_property("target_actor", A("S3B2_StairFinish_PF9_10"))
log("LOOP_FOB trigger %s extras %d; LOOP_Stair target %s" % (fob.get_editor_property("loop_trigger"), len(fob.get_editor_property("extra_actors")),
                                                            stair.get_editor_property("target_actor").get_actor_label()))

# director: catalogue from the C++ defaults (16 anomalies), point list, preload list
cdo = unreal.get_default_object(unreal.P9CampaignDirector)
director.set_editor_property("anomalies", cdo.get_editor_property("anomalies"))
sp = list(director.get_editor_property("spawn_points"))
for p in new_points:
    if p not in sp:
        sp.append(p)
director.set_editor_property("spawn_points", sp)
for prop in ("lantern_man_mesh", "greeting_stand_mesh", "greeting_lunge_mesh", "bridge_south_mesh", "bridge_end_mesh", "bridge_bay_mesh", "bridge_lead_in_mesh",
             "bridge_tube_mesh", "escalator_upper_mesh", "escalator_step_mesh"):
    director.set_editor_property(prop, cdo.get_editor_property(prop))
pre = list(director.get_editor_property("preload_assets"))
paths = []
for d in ("/Game/P9/Creatures8A", "/Game/P9/Anomalies8A", "/Game/P9/Audio/OriginalAudioPack"):
    paths += [p for p in EAL.list_assets(d, recursive=True) if not p.split(".")[-1].startswith("T_")]
added = 0
for p in paths:
    a = unreal.load_asset(p)
    if a and not isinstance(a, unreal.Texture) and a not in pre:
        pre.append(a); added += 1
director.set_editor_property("preload_assets", pre)
cat = director.get_editor_property("anomalies")
log("director: %d anomalies %s; %d spawn points; preload +%d (now %d)" % (len(cat), [str(x.get_editor_property("id")) for x in cat], len(sp), added, len(pre)))

# audio director
ad = actors.get("S8A_AudioDirector")
if ad is None:
    ad = eas.spawn_actor_from_class(unreal.P9AudioDirector, unreal.Vector(0, 0, 1300), unreal.Rotator(0, 0, 0))
    ad.set_actor_label("S8A_AudioDirector")
    ad.set_folder_path("P9/Audio")
log("audio director %s" % ad.get_actor_label())

# Dhanraj (F1): no more drunk legs - his normal Mixamo walk on every leg (the drunk clip stays assigned, unused)
f1 = A("S5B_Friend_F1_Curly")
log("F1 before: drunk_chance %.2f walk %s stairsDown %s fitted %s" % (f1.get_editor_property("drunk_chance"), f1.get_editor_property("walk_anim").get_name(),
                                                                     f1.get_editor_property("stairs_down_anim").get_name() if f1.get_editor_property("stairs_down_anim") else None,
                                                                     f1.get_editor_property("stair_clips_fitted")))
f1.set_editor_property("drunk_chance", 0.0)
for lab in ("S5B_Friend_F2_Stripe", "S5B_Friend_F1_Curly"):
    a = A(lab)
    log("%s: walk %s walkSpeed %.1f walkClipSpeed %.1f stairsUp %s/%.1f stairsDown %s/%.1f fitted %s drunk %.2f" % (lab, a.get_editor_property("walk_anim").get_name(),
        a.get_editor_property("walk_speed"), a.get_editor_property("walk_clip_speed"), a.get_editor_property("stairs_up_anim").get_name(), a.get_editor_property("stairs_up_speed"),
        a.get_editor_property("stairs_down_anim").get_name(), a.get_editor_property("stairs_down_speed"), a.get_editor_property("stair_clips_fitted"), a.get_editor_property("drunk_chance")))
