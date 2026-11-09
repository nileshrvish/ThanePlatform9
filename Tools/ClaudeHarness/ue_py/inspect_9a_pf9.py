# Stage 9A (editor, read-only): actors on / around the Platform 9/10 island (y 6800..8300), to find the indicator, lamps, benches,
# pillars, signs and the 8A Lantern / red-light points. Prints label, class, location, bounds.
import unreal
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rows = []
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    p = a.get_actor_location()
    if not (6700 <= p.y <= 8400) and not any(k in l for k in ("Indicator", "LANTERN", "REDLIGHT", "PF9_10")):
        continue
    if p.z > 1500:
        continue
    o, e = a.get_actor_bounds(False)
    rows.append((p.x, "%-48s %-26s loc(%7.0f %7.0f %6.0f) yaw %4.0f ext(%5.0f %5.0f %5.0f) ctr(%6.0f %6.0f %5.0f)" % (
        l[:48], a.get_class().get_name()[:26], p.x, p.y, p.z, a.get_actor_rotation().yaw, e.x, e.y, e.z, o.x, o.y, o.z)))
for _, r in sorted(rows):
    unreal.log("P9PY: " + r)
unreal.log("P9PY: %d actors" % len(rows))
