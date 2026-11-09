# Stage 9C (PIE, read-only): Specs' state and, when seated / sitting, pelvis / knee / foot bone world positions vs the PF1 bench
# (seat top z 45, front edge y ~ -1246, backrest face y ~ -1298)
import unreal
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.P9FriendNPC):
    if "Specs" not in a.get_name() and "F3" not in a.get_name():
        continue
    sk = a.get_component_by_class(unreal.SkeletalMeshComponent)
    p = a.get_actor_location()
    bones = {}
    for b in ("Hips", "LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase", "RightFoot", "Head"):
        try:
            v = sk.get_socket_location(b); bones[b] = "(%.0f %.0f %.0f)" % (v.x, v.y, v.z)
        except Exception:
            pass
    anim = sk.get_anim_instance()
    unreal.log("P9PY: %s loc (%.0f %.0f %.0f) yaw %.0f bones %s" % (a.get_name(), p.x, p.y, p.z - 90, a.get_actor_rotation().yaw, bones))
