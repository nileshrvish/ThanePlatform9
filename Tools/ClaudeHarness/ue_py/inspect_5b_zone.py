# Stage 5B read-only: the placed director's approach/start zones (editor world).
import unreal
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if isinstance(a, unreal.P9CampaignDirector):
        for p in ("platform9_approach_zone", "campaign_start_zone", "platform1_zone"):
            b = a.get_editor_property(p)
            unreal.log("P9PY: %s min=%s max=%s" % (p, b.min, b.max))
