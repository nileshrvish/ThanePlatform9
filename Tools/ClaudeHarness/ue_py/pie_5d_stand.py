# Stage 5D test (PIE only): the player stands up from the Dadar local seat exactly as the Stand-up prompt does
# (UP9SeatComponent::Stand), to test that seated friends get off when the player leaves.
import unreal
world = unreal.UnrealEditorSubsystem().get_game_world()
done = False
for t in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.P9Train):
    for s in t.get_components_by_class(unreal.P9SeatComponent):
        if s.is_occupied():
            unreal.log("P9PY: stand: %s %s occupant=%s -> %s" % (t.get_name(), s.get_name(), s.get_occupant().get_name(), s.stand()))
            done = True
if not done: unreal.log("P9PY: stand: player not seated")
