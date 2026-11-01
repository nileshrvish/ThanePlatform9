# Stage 5B read-only: names in the director's PreloadAssets list.
import unreal
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for a in sub.get_all_level_actors():
    if isinstance(a, unreal.P9CampaignDirector):
        names = sorted(o.get_name() for o in a.get_editor_property("preload_assets") if o)
        unreal.log("P9PY: preload %d: %s" % (len(names), " ".join(names)))
