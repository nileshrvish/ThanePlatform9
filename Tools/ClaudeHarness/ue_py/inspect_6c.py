# Stage 6C (read-only): where the booking office stands on the footbridge and what is around the deck centre.
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
pts = [a for a in actors if isinstance(a, unreal.P9AnomalySpawnPoint)]
for p in pts:
    if str(p.get_editor_property("location_type")).endswith("TICKET_COUNTER") or "TICKET" in str(p.get_editor_property("point_id")).upper() or "CLOCK" in str(p.get_editor_property("point_id")).upper():
        t = p.get_editor_property("target_actor")
        if t:
            o, e = t.get_actor_bounds(False)
            unreal.log("P9PY: point %s -> %s at %s rot %s bounds o=%s e=%s" % (p.get_editor_property("point_id"), t.get_actor_label(), t.get_actor_location(), t.get_actor_rotation(), o, e))
keys = ["Booking", "Ticket", "Clock", "FOB", "Roof", "Canopy", "Concourse", "Deck"]
rows = []
for a in actors:
    lab = a.get_actor_label()
    o, e = a.get_actor_bounds(False)
    # anything overlapping the footbridge centre region x -800..800, y 2300..4300, z 550..1400
    if -800 < o.x + e.x and o.x - e.x < 800 and 2300 < o.y + e.y and o.y - e.y < 4300 and 550 < o.z + e.z and o.z - e.z < 1400:
        if e.x * e.y * e.z < 1e10:
            rows.append((lab, o, e))
for lab, o, e in sorted(rows, key=lambda r: (r[1].y, r[1].x)):
    unreal.log("P9PY: %-46s c=(%6.0f %6.0f %6.0f) half=(%5.0f %5.0f %5.0f)" % (lab[:46], o.x, o.y, o.z, e.x, e.y, e.z))
