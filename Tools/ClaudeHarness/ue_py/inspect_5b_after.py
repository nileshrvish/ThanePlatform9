# Stage 5B read-only inspection: lights, tube meshes, trains, NPCs, spawn points, director catalogue.
# Writes JSON to the scratchpad; modifies nothing.
import json
import unreal

OUT = r"C:\Users\niles\AppData\Local\Temp\claude\C--Users-niles-Documents-Unreal-Projects-ThanePlatform9\ad925f3e-6a98-4fc3-a5f7-bf718824e24b\scratchpad\inspect_5b_after.json"

def v3(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]

def safe(obj, prop, default=None):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        return default

def enum_s(e):
    return str(e).split(".")[-1].rstrip(">").split(":")[0] if e is not None else None

sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = sub.get_all_level_actors()
out = {"lights": [], "tubes": [], "trains": [], "npcs": [], "points": [], "director": {}, "level_instances": []}

for a in actors:
    name = a.get_actor_label()
    folder = str(a.get_folder_path())
    cls = a.get_class().get_name()
    tags = [str(t) for t in a.tags]
    if isinstance(a, unreal.LevelInstance) or "LevelInstance" in cls:
        out["level_instances"].append({"name": name, "cls": cls, "loc": v3(a.get_actor_location())})
    for lc in a.get_components_by_class(unreal.LightComponent):
        d = {"actor": name, "folder": folder, "tags": tags, "comp": lc.get_name(), "cls": lc.get_class().get_name(),
             "mobility": enum_s(safe(lc, "mobility")), "intensity": safe(lc, "intensity"),
             "units": enum_s(safe(lc, "intensity_units")), "visible": safe(lc, "visible"),
             "loc": v3(lc.get_world_location()), "rot": [round(lc.get_world_rotation().pitch, 1), round(lc.get_world_rotation().yaw, 1)],
             "color": str(safe(lc, "light_color")), "use_temp": safe(lc, "use_temperature"), "temp": safe(lc, "temperature"),
             "shadows": safe(lc, "cast_shadows"), "radius": safe(lc, "attenuation_radius"),
             "src_radius": safe(lc, "source_radius"), "src_len": safe(lc, "source_length"), "soft_radius": safe(lc, "soft_source_radius"),
             "inner": safe(lc, "inner_cone_angle"), "outer": safe(lc, "outer_cone_angle"),
             "src_w": safe(lc, "source_width"), "src_h": safe(lc, "source_height"), "barn": safe(lc, "barn_door_angle"),
             "inv_sq": safe(lc, "use_inverse_squared_falloff"), "max_draw": safe(lc, "max_draw_distance"),
             "fade": safe(lc, "max_distance_fade_range"), "spec": safe(lc, "specular_scale"), "vol": safe(lc, "volumetric_scattering_intensity")}
        out["lights"].append(d)
    for sm in a.get_components_by_class(unreal.StaticMeshComponent):
        mesh = sm.static_mesh
        if mesh and ("Tube" in mesh.get_name() or "Lamp" in mesh.get_name()):
            out["tubes"].append({"actor": name, "folder": folder, "mesh": mesh.get_name(), "loc": v3(sm.get_world_location()),
                                 "yaw": round(sm.get_world_rotation().yaw, 1), "hidden": a.is_hidden_ed() or safe(a, "hidden", False)})
    if cls.startswith("P9Train") or isinstance(a, unreal.P9Train):
        out["trains"].append({"name": name, "cls": cls, "loc": v3(a.get_actor_location()),
                              "auto_cycle": safe(a, "auto_cycle"), "start_stop": safe(a, "start_stop_index"),
                              "stops": [round(x, 1) for x in (safe(a, "stop_distances") or [])],
                              "dest": safe(a, "destination_text"), "max_speed": safe(a, "max_speed"), "hidden": safe(a, "hidden", None)})
    if isinstance(a, unreal.P9FriendNPC):
        out["npcs"].append({"name": name, "cls": cls, "key": str(safe(a, "friend_key")), "role": enum_s(safe(a, "npc_role")),
                            "speech_radius": safe(a, "speech_radius"), "cooldown": safe(a, "speech_cooldown"),
                            "line": str(safe(a, "proximity_line")), "route": len(safe(a, "route") or [])})
    if isinstance(a, unreal.P9SilhouetteStalker):
        out["npcs"].append({"name": name, "cls": cls, "day_chance": safe(a, "day_chance"), "max": safe(a, "max_appearances_per_day"),
                            "cooldown": safe(a, "cooldown"), "only_anom": safe(a, "only_on_anomalous_days")})
    if isinstance(a, unreal.P9AnomalySpawnPoint):
        ta = safe(a, "target_actor"); sa = safe(a, "secondary_actor"); ex = safe(a, "extra_actors") or []
        out["points"].append({"label": name, "id": str(safe(a, "point_id")), "type": enum_s(safe(a, "location_type")),
                              "loc": v3(a.get_actor_location()), "target": ta.get_actor_label() if ta else None,
                              "secondary": sa.get_actor_label() if sa else None, "extras": [e.get_actor_label() if e else None for e in ex],
                              "target_comp": str(safe(a, "target_component"))})
    if isinstance(a, unreal.P9CampaignDirector):
        defs = []
        for d in safe(a, "anomalies") or []:
            defs.append({"id": str(d.get_editor_property("id")), "name": d.get_editor_property("display_name"),
                         "enabled": d.get_editor_property("enabled"), "cat": enum_s(d.get_editor_property("category")),
                         "handler": enum_s(d.get_editor_property("handler")), "weight": d.get_editor_property("weight"),
                         "locs": [enum_s(x) for x in d.get_editor_property("location_types")],
                         "scare": enum_s(d.get_editor_property("jumpscare")), "scare_chance": d.get_editor_property("jumpscare_chance"),
                         "variants": [{"id": str(v.get_editor_property("id")), "diff": v.get_editor_property("difficulty"),
                                       "mat": str(v.get_editor_property("material")), "snd": str(v.get_editor_property("sound")),
                                       "mesh": str(v.get_editor_property("mesh")), "text": v.get_editor_property("text"),
                                       "later": v.get_editor_property("change_after_look_away")} for v in d.get_editor_property("variants")]})
        tr = safe(a, "train"); ot = safe(a, "opening_train")
        out["director"] = {"label": name, "num_days": safe(a, "num_days"), "defs": defs,
                           "points": len(safe(a, "spawn_points") or []), "train": tr.get_actor_label() if tr else None,
                           "opening_train": ot.get_actor_label() if ot else None,
                           "chaiwala_mesh": str(safe(a, "chaiwala_mesh"))}

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(out, f, indent=1, default=str)
unreal.log("P9PY: inspect_5b lights=%d tubes=%d trains=%d npcs=%d points=%d defs=%d li=%d -> %s" % (
    len(out["lights"]), len(out["tubes"]), len(out["trains"]), len(out["npcs"]), len(out["points"]),
    len(out["director"].get("defs", [])), len(out["level_instances"]), OUT))
