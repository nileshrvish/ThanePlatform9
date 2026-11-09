# Stage 6A map setup (does NOT save): new anomaly spawn points, the director's catalogue / point list / Kanchana / preload.
# Idempotent: points that already exist (by label) are updated, not duplicated. Old points stay in the level (unlisted).
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
L = unreal.P9AnomalyLocation
V = unreal.Vector
FOLDER = "P9/Campaign/AnomalyPoints6A"

def box(lo, hi):
    b = unreal.Box()
    b.set_editor_property("min", V(*lo)); b.set_editor_property("max", V(*hi)); b.set_editor_property("is_valid", True)
    return b

def point(label, pid, ltype, loc, yaw=0.0, target=None, secondary=None, extras=(), trigger=None, offset=None, direction=None):
    a = actors.get(label)
    if not a:
        a = sub.spawn_actor_from_class(unreal.P9AnomalySpawnPoint, V(*loc), unreal.Rotator(0, 0, yaw))
        a.set_actor_label(label)
        actors[label] = a
    a.set_actor_location(V(*loc), False, False); a.set_actor_rotation(unreal.Rotator(0, 0, yaw), False)
    a.set_folder_path(FOLDER)
    a.set_editor_property("point_id", pid)
    a.set_editor_property("location_type", ltype)
    for prop, name in (("target_actor", target), ("secondary_actor", secondary)):
        if name:
            if name not in actors: raise RuntimeError("missing actor " + name)
            a.set_editor_property(prop, actors[name])
    a.set_editor_property("extra_actors", [actors[n] for n in extras])
    if trigger:
        a.set_editor_property("loop_trigger", box(*trigger))
        a.set_editor_property("loop_offset", V(*offset))
        a.set_editor_property("loop_direction", V(*direction))
    return a

new = [
    # 6. footbridge signs on the route
    point("S6A_Point_FOB_SignPF45", "FOB_SignPF45", L.FOB_SIGN, (0, 2250, 900), target="S3B1_FOBSign_PF4_5"),
    point("S6A_Point_FOB_SignPF67", "FOB_SignPF67", L.FOB_SIGN, (0, 4070, 900), target="S3B1_FOBSign_PF6_7"),
    point("S6A_Point_FOB_SignPF8", "FOB_SignPF8", L.FOB_SIGN, (0, 5930, 900), target="S3B1_FOBSign_PF8"),
    # 11. booking office
    point("S6A_Point_TICKET_Counter", "TICKET_Counter", L.TICKET_COUNTER, (-560, 3150, 700), target="S3C_BookingOffice"),
    # 7. closed stair heads: panel, its collision block (and the PF6/7 tarp on the stair), the CLOSED plate
    point("S6A_Point_CLOSED_PF45", "CLOSED_PF45", L.CLOSED_ENTRY, (210, 2250, 700), target="S3C_Closure_Gate_PF4_5",
          secondary="S3D_SignClosed_PF4_5", extras=["S2_Gate_PF4_5"]),
    point("S6A_Point_CLOSED_PF67", "CLOSED_PF67", L.CLOSED_ENTRY, (210, 4070, 700), target="S3C_Closure_Hoarding_PF6_7",
          secondary="S3D_SignClosed_PF6_7", extras=["S2_Scaffold_PF6_7_Panel", "S2_Scaffold_PF6_7_Tarp", "S3C_Closure_Tarp_PF6_7"]),
    point("S6A_Point_CLOSED_PF8", "CLOSED_PF8", L.CLOSED_ENTRY, (210, 5930, 700), target="S3C_Closure_Barricade_PF8",
          secondary="S3D_SignClosed_PF8", extras=["S2_Barricade_PF8"]),
    # 10. tracks either side of the Platform 9/10 island (points inside the anomaly zone)
    point("S6A_Point_TRACK_T9", "TRACK_T9", L.TRACK, (-900, 6740, -75), target="S3C_Track_T9"),
    point("S6A_Point_TRACK_T10", "TRACK_T10", L.TRACK, (-900, 7950, -75), target="S3C_Track_T10"),
    # 3. endless footbridge: walking north past y 6900 on deck B -> back 14.8 m (PF9 stair opening -> PF8 opening)
    point("S6A_Point_LOOP_FOB", "LOOP_FOB", L.FOB_LOOP, (0, 6950, 650),
          trigger=((-200, 6900, 540), (200, 7000, 800)), offset=(0, -1480, 0), direction=(0, 1, 0)),
    # 9. endless stair: 3 steps from the bottom of the lower flight -> 3 steps above the landing (same step, 6.6 m back, 3 m up)
    point("S6A_Point_LOOP_Stair", "LOOP_Stair", L.STAIR_LOOP, (1300, 7410, 60),
          trigger=((1295, 7262, -40), (1345, 7558, 160)), offset=(-660, 0, 300), direction=(1, 0, 0)),
    # 4. where Kanchana may wait (footbridge, concourse, Platform 9/10)
    point("S6A_Point_KANCHANA_DeckA", "KANCHANA_DeckA", L.STALKER_SPOT, (60, 1900, 600), yaw=-90),
    point("S6A_Point_KANCHANA_Concourse", "KANCHANA_Concourse", L.STALKER_SPOT, (-380, 3500, 600), yaw=-60),
    point("S6A_Point_KANCHANA_DeckB", "KANCHANA_DeckB", L.STALKER_SPOT, (-90, 5150, 600), yaw=-90),
    point("S6A_Point_KANCHANA_DeckBNorth", "KANCHANA_DeckBNorth", L.STALKER_SPOT, (80, 7780, 600), yaw=-90),
    point("S6A_Point_KANCHANA_PF9West", "KANCHANA_PF9West", L.STALKER_SPOT, (-1800, 7300, 0), yaw=0),
    point("S6A_Point_KANCHANA_PF9East", "KANCHANA_PF9East", L.STALKER_SPOT, (2300, 7500, 0), yaw=180),
]
keep = ["PF910_SignW", "PF910_SignE", "FOB_SignPF23", "PF910_Indicator", "PF910_Clock", "FOB_SignPF910", "PA_Station", "POSTER_FOB", "POSTER_PF9"]
olds = {str(a.get_editor_property("point_id")): a for a in actors.values() if isinstance(a, unreal.P9AnomalySpawnPoint)}
points = [olds[k] for k in keep] + new

directors = [a for a in actors.values() if isinstance(a, unreal.P9CampaignDirector)]
assert len(directors) == 1
d = directors[0]
cdo = unreal.get_default_object(unreal.P9CampaignDirector)
cat = cdo.get_editor_property("anomalies")
# the open-entry anomaly keeps only its "Open" variant (the C++ default changes the same way at the next build)
for df in cat:
    if str(df.get_editor_property("id")) == "ClosedEntryOpen":
        vs = [v for v in df.get_editor_property("variants") if str(v.get_editor_property("id")) != "OpenSignStays"]
        for v in vs:
            if str(v.get_editor_property("id")) == "OpenSignGone":
                v.set_editor_property("id", "Open"); v.set_editor_property("param", 1.0)
        df.set_editor_property("variants", vs)
d.set_editor_property("anomalies", cat)
d.set_editor_property("anomaly_count_weights", [1.0])
d.set_editor_property("spawn_points", points)
d.set_editor_property("stalker", actors["S5B_Silhouette"])
pre = list(d.get_editor_property("preload_assets"))
paths = ["/Engine/BasicShapes/Sphere", "/Game/P9/Anomalies6A/Textures/T_P9_Anom6_ClockDigits", "/Game/P9/Anomalies6A/SM_P9_TrackPanel_GapRails",
         "/Game/P9/Anomalies6A/SM_P9_TrackPanel_GapAll", "/Game/P9/Announcements/SW_P9_PA_PF9_Anom6_Platform10", "/Game/P9/Announcements/SW_P9_PA_PF9_Anom6_Panvel"]
reg = unreal.AssetRegistryHelpers.get_asset_registry()
paths += [str(x.package_name) for x in reg.get_assets_by_path("/Game/P9/Anomalies6A/Materials", recursive=False)]
added = 0
for p in paths:
    o = unreal.load_asset(p)
    if o and o not in pre:
        pre.append(o); added += 1
    elif not o:
        unreal.log_warning("P9PY: preload missing " + p)
d.set_editor_property("preload_assets", pre)
names = [str(x.get_editor_property("id")) for x in d.get_editor_property("anomalies")]
unreal.log("P9PY: a6_setup - %d new/updated points, director: %d anomalies %s, %d spawn points, stalker %s, preload +%d (%d)"
           % (len(new), len(names), names, len(points), d.get_editor_property("stalker").get_actor_label(), added, len(pre)))
