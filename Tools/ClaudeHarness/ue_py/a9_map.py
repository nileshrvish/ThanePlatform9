# Stage 9A map + material setup (editor world; does NOT save the map; saves the edited material assets).
# Idempotent: re-running sets the same values.
#  1. Lantern Man point -> dark west end of Platform 9/10 by the PF9/10 train indicator; his flickering lamp = W1 (nearest).
#  2. RedLightPF9 -> only the W1 lamp (beside the indicator): REDLIGHT_W2 / REDLIGHT_E are unlisted from the director (actors kept).
#  3. NPC data: Specs' bench stop out of the bench collision; chaiwala's return path round the concourse bench.
#  4. M_P9_SignFace: internally lit sign face (emissive from the sign texture with a contrast curve); CLOSED plate unlit.
#  5. M_P9_NPC3D: scan-albedo calibration (highlight knee + saturation) with per-character values on the existing MIs.
import unreal

eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
log = lambda s: unreal.log("P9PY: " + s)
actors = {a.get_actor_label(): a for a in eas.get_all_level_actors()}
A = lambda l: actors[l]
director = next(a for a in actors.values() if isinstance(a, unreal.P9CampaignDirector))

# ---- 1. Lantern Man
lp = A("S8A_Point_LANTERN_PF9East")
lp.set_actor_location(unreal.Vector(-2860, 7440, 0), False, False)
lp.set_actor_rotation(unreal.Rotator(0, 0, -10), False)          # faces east down the platform, a little towards the PF9 edge
lp.set_editor_property("target_actor", A("S2C_Lamp_PF9_10_W1"))
lp.set_editor_property("secondary_actor", A("S3B3_TubeLight_PF9_10_W1"))
lp.set_editor_property("extra_actors", [A("S3C_Fill_PF9_10_W1")])
log("lantern point -> %s yaw %.0f lamp W1" % (lp.get_actor_location(), lp.get_actor_rotation().yaw))

# ---- 2. Red light: W1 only
pts = list(director.get_editor_property("spawn_points"))
before = len(pts)
keep = [p for p in pts if p and p.get_actor_label() not in ("S8A_Point_REDLIGHT_W2", "S8A_Point_REDLIGHT_E")]
director.set_editor_property("spawn_points", keep)
log("director spawn points %d -> %d (REDLIGHT_W2/E unlisted, actors kept); W1 listed=%s" % (before, len(keep), any(p.get_actor_label() == "S8A_Point_REDLIGHT_W1" for p in keep)))

# ---- 3. NPC data
specs = A("S5B_Friend_F3_Specs")
route = list(specs.get_editor_property("route"))
for i, rp in enumerate(route):
    loc = rp.get_editor_property("location")
    if rp.get_editor_property("stop") == unreal.P9NPCStop.SIT:
        rp.set_editor_property("location", unreal.Vector(loc.x, -1230.0, loc.z))
        log("Specs sit stop %d: (%.0f %.0f) -> (%.0f -1230) (bench collision ends at y -1239)" % (i, loc.x, loc.y, loc.x))
specs.set_editor_property("route", route)
cw = A("S5B_Chaiwala")
path = [unreal.Vector(p.x, p.y, p.z) for p in cw.get_editor_property("epilogue_return_path")]
out = []
for p in path:
    if abs(p.x - 300) < 1 and abs(p.y - 3350) < 1:
        out += [unreal.Vector(300, 3335, p.z), unreal.Vector(470, 3350, p.z)]       # round the concourse bench (x 245..435, y 3389..3461)
    elif abs(p.x - 300) < 1 and abs(p.y - 3335) < 1:
        out.append(p)
    else:
        out.append(p)
# idempotent: drop a duplicate (470,3350) if the script ran before
dedup = []
for p in out:
    if not dedup or (abs(dedup[-1].x - p.x) > 1 or abs(dedup[-1].y - p.y) > 1):
        dedup.append(p)
cw.set_editor_property("epilogue_return_path", dedup)
log("chaiwala return path: %s" % " ".join("(%.0f,%.0f,%.0f)" % (p.x, p.y, p.z) for p in dedup))

# ---- 4. sign face material
def node_by_class(m, cls):
    # walk from the properties we know
    return None

sf = unreal.load_asset("/Game/P9/Environment/StationSignage/M_P9_SignFace")
if mel.get_material_property_input_node(sf, unreal.MaterialProperty.MP_EMISSIVE_COLOR) is None:
    mul = mel.get_material_property_input_node(sf, unreal.MaterialProperty.MP_BASE_COLOR)
    tex = [n for n in mel.get_inputs_for_material_expression(sf, mul) if isinstance(n, unreal.MaterialExpressionTextureSampleParameter2D)][0]
    tint = [n for n in mel.get_inputs_for_material_expression(sf, mul) if isinstance(n, unreal.MaterialExpressionVectorParameter)][0]
    pw = mel.create_material_expression(sf, unreal.MaterialExpressionPower, -300, 400)
    con = mel.create_material_expression(sf, unreal.MaterialExpressionScalarParameter, -500, 480)
    con.set_editor_property("parameter_name", "EmissiveContrast"); con.set_editor_property("default_value", 1.6)
    mel.connect_material_expressions(tex, "RGB", pw, "Base")
    mel.connect_material_expressions(con, "", pw, "Exp")
    m1 = mel.create_material_expression(sf, unreal.MaterialExpressionMultiply, -100, 420)
    mel.connect_material_expressions(pw, "", m1, "A")
    mel.connect_material_expressions(tint, "", m1, "B")
    st = mel.create_material_expression(sf, unreal.MaterialExpressionScalarParameter, -300, 560)
    st.set_editor_property("parameter_name", "EmissiveStrength"); st.set_editor_property("default_value", 1.0)
    m2 = mel.create_material_expression(sf, unreal.MaterialExpressionMultiply, 100, 460)
    mel.connect_material_expressions(m1, "", m2, "A")
    mel.connect_material_expressions(st, "", m2, "B")
    mel.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(sf)
    log("M_P9_SignFace: emissive added (texture^EmissiveContrast x Tint x EmissiveStrength)")
else:
    log("M_P9_SignFace: emissive already present")
EAL.save_loaded_asset(sf, False)
closed = unreal.load_asset("/Game/P9/Environment/StationSignage/MI_P9_SignClosed")
mel.set_material_instance_scalar_parameter_value(closed, "EmissiveStrength", 0.0)      # a painted plate, not a lit sign
mel.update_material_instance(closed); EAL.save_loaded_asset(closed, False)

# ---- 5. NPC albedo calibration
nm = unreal.load_asset("/Game/P9/Characters/Station3D/Materials/M_P9_NPC3D")
if not any(str(n) == "AlbedoKnee" for n in mel.get_scalar_parameter_names(nm)):
    tex = mel.get_material_property_input_node(nm, unreal.MaterialProperty.MP_BASE_COLOR)
    cu = mel.create_material_expression(nm, unreal.MaterialExpressionCustom, -200, -200)
    cu.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    cu.set_editor_property("description", "P9 scan albedo calibration (9A)")
    cu.set_editor_property("code", "float L = dot(C, float3(0.2126, 0.7152, 0.0722));\n"
                                    "float Lc = L;\n"
                                    "if (L > Knee) { float x = L - Knee; float r = max(MaxAlbedo - Knee, 0.001); Lc = Knee + x / (1.0 + x / r); }\n"
                                    "float3 c2 = C * (Lc / max(L, 0.0001));\n"
                                    "return max(lerp(Lc.xxx, c2, Sat), 0.0);")
    ins = []
    for nme in ("C", "Knee", "MaxAlbedo", "Sat"):
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", nme); ins.append(ci)
    cu.set_editor_property("inputs", ins)
    mel.connect_material_expressions(tex, "RGB", cu, "C")
    for i, (pname, dv, y) in enumerate((("AlbedoKnee", 1.0, -120), ("AlbedoMax", 1.0, -60), ("AlbedoSaturation", 1.0, 0))):
        p = mel.create_material_expression(nm, unreal.MaterialExpressionScalarParameter, -450, y)
        p.set_editor_property("parameter_name", pname); p.set_editor_property("default_value", dv)
        mel.connect_material_expressions(p, "", cu, ("Knee", "MaxAlbedo", "Sat")[i])
    mel.connect_material_property(cu, "", unreal.MaterialProperty.MP_BASE_COLOR)
    sp = mel.create_material_expression(nm, unreal.MaterialExpressionScalarParameter, -200, 120)
    sp.set_editor_property("parameter_name", "SpecularLevel"); sp.set_editor_property("default_value", 0.5)
    mel.connect_material_property(sp, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(nm)
    log("M_P9_NPC3D: albedo calibration + SpecularLevel added (identity defaults)")
EAL.save_loaded_asset(nm, False)
# per character: knee / max linear albedo / saturation / specular (from the measured scan albedo; kanchana stays as she is)
cal = {"dhanraj": (0.28, 0.62, 1.18, 0.4), "Modi": (0.32, 0.66, 1.08, 0.4), "Rohit": (0.30, 0.64, 1.10, 0.4),
       "Nilesh": (0.32, 0.66, 1.0, 0.45), "shivshankar": (0.32, 0.66, 1.05, 0.45), "rupesh": (0.35, 0.7, 1.0, 0.45)}
for n, (k, mx, sat, spec) in cal.items():
    mi = unreal.load_asset("/Game/P9/Characters/Station3D/Materials/MI_P9_NPC3D_%s" % n)
    for pn, v in (("AlbedoKnee", k), ("AlbedoMax", mx), ("AlbedoSaturation", sat), ("SpecularLevel", spec)):
        mel.set_material_instance_scalar_parameter_value(mi, pn, v)
    mel.update_material_instance(mi)
    EAL.save_loaded_asset(mi, False)
    log("MI_P9_NPC3D_%s: knee %.2f max %.2f sat %.2f spec %.2f" % (n, k, mx, sat, spec))
