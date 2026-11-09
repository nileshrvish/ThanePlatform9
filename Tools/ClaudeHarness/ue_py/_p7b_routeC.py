ROUTES = {
    "F1": [(0, 1200, 600, "W"), (0, 2200, 600, "W")],
    "F2": [(0, 2200, 600, "W"), (0, 1200, 600, "W")],
    "F3": [(80, 1000, 600, "W"), (80, 2300, 600, "W")],
    "F4": [(80, 1100, 600, "W"), (80, 2300, 600, "W")],
    "F5": [(-110, 1650, 600, "W"), (-110, 1700, 600, "W")],
    "CW": [(-110, 1700, 600, "W"), (-110, 1650, 600, "W")],
}
WAIT = (0.5, 1.5)
exec(open(r"C:/Users/niles/Documents/Unreal Projects/ThanePlatform9/Tools/ClaudeHarness/ue_py/pie_7b_route.py", encoding="utf-8").read())
