import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
npc = next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9FriendNPC) if str(a.get_editor_property("friend_key")) == "F1")
b = npc.get_editor_property("body")
b.stop(); b.set_animation(None); b.set_animation_mode(unreal.AnimationMode.ANIMATION_CUSTOM_MODE)
b.reset_all_bodies_simulate_physics() if hasattr(b, "reset_all_bodies_simulate_physics") else None
import time
for s in ("root", "pelvis", "spine_05", "head", "foot_l", "hand_r", "middle_01_r"):
    unreal.log("P9PY: refpose %s %s" % (s, b.get_socket_transform(s, unreal.RelativeTransformSpace.RTS_COMPONENT).translation))
a = unreal.load_asset("/Game/P9/Characters/Station3D/dhanraj/MQ/SK_P9_NPC3D_dhanraj_MQ_Walk")
src = unreal.load_asset("/Game/P9/Characters/MannyX/Anims/SKM_MannyX_Walk")
for an in (a, src):
    unreal.log("P9PY: anim %s skeleton %s len %.2f" % (an.get_name(), an.get_editor_property("skeleton").get_name(), an.get_play_length()))
