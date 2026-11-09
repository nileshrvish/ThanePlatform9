# Stage 6B cleanup, step 1 (map only, does not save): drop the dry run's DELETE candidates from the director's PreloadAssets
# and remove the anomaly spawn points the director no longer lists. Nothing that the 11 anomalies use is touched.
import json
import unreal

SCR = r"C:\Users\niles\AppData\Local\Temp\claude\858f1689b55618bc\80486331-8a39-4af2-867a-1e54249a2747\scratchpad"
plan = json.load(open(SCR + r"\cleanup_plan.json"))
drop = set(plan["delete"])
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
d = [a for a in actors if isinstance(a, unreal.P9CampaignDirector)][0]
pre = list(d.get_editor_property("preload_assets"))
kept = [o for o in pre if o and o.get_outermost().get_name() not in drop]
d.set_editor_property("preload_assets", kept)
listed = set(d.get_editor_property("spawn_points"))
gone = [a for a in actors if isinstance(a, unreal.P9AnomalySpawnPoint) and a not in listed and a.get_actor_label() in plan["points_to_remove"]]
labels = sorted(a.get_actor_label() for a in gone)
ok = sub.destroy_actors(gone)
unreal.log("P9PY: preload %d -> %d; destroyed %d spawn points (%s): %s" % (len(pre), len(kept), len(gone), ok, labels))
unreal.log("P9PY: director still lists %d points, all valid: %s" % (len(d.get_editor_property("spawn_points")), all(d.get_editor_property("spawn_points"))))
