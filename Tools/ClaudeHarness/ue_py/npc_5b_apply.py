# Stage 5B instance settings (editor world, not saved here): the five friends locked until the campaign is completed,
# the chaiwala's rally line once per campaign day, the silhouette locked on Days 1-10 and allowed in the epilogue.
import unreal

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
done = []
for a in sub.get_all_level_actors():
    label = a.get_actor_label()
    if isinstance(a, unreal.P9FriendNPC):
        key = str(a.get_editor_property("friend_key"))
        a.modify()
        if key in ("F1", "F2", "F3", "F4", "F5"):
            a.set_editor_property("hide_until_campaign_complete", True)
            done.append("%s lock" % label)
        elif key == "CW":
            a.set_editor_property("once_per_campaign_day", True)
            done.append("%s daily" % label)
    elif isinstance(a, unreal.P9SilhouetteStalker):
        a.modify()
        a.set_editor_property("hide_until_campaign_complete", True)
        a.set_editor_property("appear_in_epilogue", True)
        a.set_editor_property("epilogue_chance", 1.0)
        done.append("%s lock+epilogue" % label)
unreal.log("P9PY: npc_5b applied %d: %s" % (len(done), ", ".join(done)))
