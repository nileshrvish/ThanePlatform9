# Stage 6C map setup (does NOT save): the analog clock in the footbridge concourse (hung from the ceiling in front of the
# booking office, dials facing north and south along the walking route), its anomaly spawn point, the director's catalogue
# (copied from the C++ defaults: 12 anomalies) and point list. Idempotent: existing actors (by label) are updated.
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actors = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
V = unreal.Vector
X, Y = -250.0, 3150.0
FLANGE = 55.0                                      # the body's ceiling flange top above the clock centre (cm)

def trace(z0, z1):
    hit = unreal.SystemLibrary.line_trace_single(world, V(X, Y, z0), V(X, Y, z1), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None, None
    t = hit.to_tuple()
    return t[4].z, t[9]                            # impact point z, hit actor
ceil_z, ceil_actor = trace(700, 1200)
floor_z, floor_actor = trace(800, 400)
unreal.log("P9PY: ceiling z %s (%s), floor z %s (%s)" % (ceil_z, ceil_actor.get_actor_label() if ceil_actor else None, floor_z, floor_actor.get_actor_label() if floor_actor else None))
assert ceil_z and 880 < ceil_z < 960, "unexpected ceiling"
centre = V(X, Y, ceil_z - FLANGE)

clock = actors.get("S6C_AnalogClock_Concourse")
if not clock:
    clock = sub.spawn_actor_from_class(unreal.P9AnalogClock, centre, unreal.Rotator(0, 0, 0))
    clock.set_actor_label("S6C_AnalogClock_Concourse")
clock.set_actor_location(centre, False, False)
clock.set_actor_rotation(unreal.Rotator(0, 0, 0), False)       # dials face +Y (north) and -Y (south)
clock.set_folder_path("P9/Environment/AnalogClock6C")
for comp in clock.get_components_by_class(unreal.StaticMeshComponent):
    if comp.get_name() not in ("Body",):
        comp.set_editor_property("ld_max_draw_distance", 4500.0)  # hands and glass are tiny beyond 45 m
o, e = clock.get_actor_bounds(False)
unreal.log("P9PY: clock at %s, bounds centre %s half %s, bottom z %.1f (%.1f cm above the floor)" % (centre, o, e, o.z - e.z, o.z - e.z - (floor_z or 600)))

# anything else the clock's bounds touch (the roof/shell it hangs from is expected)
lo, hi = o - e, o + e
touch = []
for lab, a in actors.items():
    if a == clock:
        continue
    ao, ae = a.get_actor_bounds(False)
    if ae.x * ae.y * ae.z == 0 or ae.x > 5000:
        continue
    if all(ao.to_tuple()[i] - ae.to_tuple()[i] < hi.to_tuple()[i] and ao.to_tuple()[i] + ae.to_tuple()[i] > lo.to_tuple()[i] for i in range(3)):
        touch.append(lab)
unreal.log("P9PY: clock bounds touch: %s" % touch)

point = actors.get("S6C_Point_ANALOG_Clock")
if not point:
    point = sub.spawn_actor_from_class(unreal.P9AnomalySpawnPoint, centre, unreal.Rotator(0, 0, 0))
    point.set_actor_label("S6C_Point_ANALOG_Clock")
point.set_actor_location(centre, False, False)
point.set_folder_path("P9/Campaign/AnomalyPoints6C")
point.set_editor_property("point_id", "ANALOG_Clock")
point.set_editor_property("location_type", unreal.P9AnomalyLocation.ANALOG_CLOCK)
point.set_editor_property("target_actor", clock)

d = [a for a in actors.values() if isinstance(a, unreal.P9CampaignDirector)]
assert len(d) == 1
d = d[0]
cdo = unreal.get_default_object(unreal.P9CampaignDirector)
d.set_editor_property("anomalies", cdo.get_editor_property("anomalies"))
pts = list(d.get_editor_property("spawn_points"))
if point not in pts:
    pts.append(point)
d.set_editor_property("spawn_points", pts)
cat = d.get_editor_property("anomalies")
unreal.log("P9PY: director: %d anomalies %s; %d spawn points" % (len(cat), [str(x.get_editor_property("id")) for x in cat], len(pts)))
last = cat[-1]
unreal.log("P9PY: #12 %s weight %.2f handler %s variants %s" % (last.get_editor_property("id"), last.get_editor_property("weight"), last.get_editor_property("handler"),
          [(str(v.get_editor_property("id")), v.get_editor_property("difficulty"), v.get_editor_property("param"), v.get_editor_property("scale").to_tuple()) for v in last.get_editor_property("variants")]))
