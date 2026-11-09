# Stage 6A: track 9 is under the Vashi local whenever the player reaches Platform 9 - unlist its point (actor kept).
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
d = [a for a in sub.get_all_level_actors() if isinstance(a, unreal.P9CampaignDirector)][0]
pts = [p for p in d.get_editor_property("spawn_points") if p and str(p.get_editor_property("point_id")) != "TRACK_T9"]
d.set_editor_property("spawn_points", pts)
unreal.log("P9PY: spawn points now %d: %s" % (len(pts), [str(p.get_editor_property("point_id")) for p in pts]))
