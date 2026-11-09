ROUTES = {
    "F4": [(-1430, -1600, 0, "W"), (-90, -1600, 600, "W")],
    "F5": [(-1430, -1525, 0, "W"), (-90, -1525, 600, "W")],
    "CW": [(-1430, -1450, 0, "W"), (-90, -1450, 600, "W")],
}
WAIT = (0.5, 1.5)
import unreal
NN = unreal.PropertyAccessChangeNotifyMode.NEVER
_w = unreal.UnrealEditorSubsystem().get_game_world()
for _a in unreal.GameplayStatics.get_all_actors_of_class(_w, unreal.P9FriendNPC):
    if str(_a.get_editor_property("friend_key")) == "CW":
        _a.set_editor_property("hide_until_campaign_complete", True, NN)   # PIE copy only: lets ForceUnlock 0/1 restart his routine
exec(open(r"C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Tools/ClaudeHarness/ue_py/pie_7b_route.py", encoding="utf-8").read())
