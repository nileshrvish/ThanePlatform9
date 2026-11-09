# Stage 8A asset setup (editor; NEW assets only, the map is not touched). Idempotent: existing 8A assets are updated, not replaced.
#  - creature textures (normal: green flipped, linear masks), master material M_P9_Creature8A + MI_P9_LanternMan / MI_P9_Greeting, LODs
#  - endless footbridge / escalator pieces: slots mapped to the station's own material instances (by the source slot name)
#  - red seat material instances (parent of MI_P9_Train_Seat, recoloured textures)
#  - original audio pack: sound classes, attenuation presets, looping, compression, class/attenuation on each wave
import unreal

EAL = unreal.EditorAssetLibrary
AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
out = []


def log(s):
    out.append(s)
    unreal.log("P9PY: " + s)


def load(p):
    a = unreal.load_asset(p)
    if not a:
        raise RuntimeError("missing " + p)
    return a


def get_or_create(name, folder, cls, factory):
    p = "%s/%s" % (folder, name)
    if EAL.does_asset_exist(p):
        return unreal.load_asset(p)
    return AT.create_asset(name, folder, cls, factory)


# ---------------------------------------------------------------- creature textures
for f in ("/Game/P9/Creatures8A/LanternMan", "/Game/P9/Creatures8A/Greeting"):
    for p in EAL.list_assets(f, recursive=False):
        a = unreal.load_asset(p)
        if not isinstance(a, unreal.Texture2D):
            continue
        n = a.get_name()
        if n.endswith("_Normal"):
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            a.set_editor_property("srgb", False)
            a.set_editor_property("flip_green_channel", True)       # Blender bakes OpenGL normals
        elif n.endswith("_Roughness") or n.endswith("_Emissive"):
            a.set_editor_property("srgb", False)
            a.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        a.set_editor_property("max_texture_size", 2048 if not n.endswith("_Emissive") else 512)
        log("texture %s srgb=%s comp=%s" % (n, a.get_editor_property("srgb"), a.get_editor_property("compression_settings")))

# ---------------------------------------------------------------- creature master material
MF = "/Game/P9/Creatures8A/Materials"
mpath = MF + "/M_P9_Creature8A"
if not EAL.does_asset_exist(mpath):
    m = AT.create_asset("M_P9_Creature8A", MF, unreal.Material, unreal.MaterialFactoryNew())
    def tex_param(name, x, y, default, sampler):
        e = MEL.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("texture", load(default))
        e.set_editor_property("sampler_type", sampler)
        return e
    bc = tex_param("BaseColorTex", -800, -200, "/Engine/EngineResources/WhiteSquareTexture", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    nm = tex_param("NormalTex", -800, 100, "/Engine/EngineMaterials/DefaultNormal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rg = tex_param("RoughnessTex", -800, 400, "/Engine/EngineResources/WhiteSquareTexture", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    em = tex_param("EmissiveMaskTex", -800, 700, "/Engine/EngineResources/Black", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    rs = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 500); rs.set_editor_property("parameter_name", "RoughnessScale"); rs.set_editor_property("default_value", 1.0)
    rmul = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -300, 420)
    MEL.connect_material_expressions(rg, "R", rmul, "A"); MEL.connect_material_expressions(rs, "", rmul, "B")
    ec = MEL.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -600, 800); ec.set_editor_property("parameter_name", "EmissiveColor")
    ec.set_editor_property("default_value", unreal.LinearColor(1.0, 0.55, 0.22, 1.0))
    es = MEL.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -600, 950); es.set_editor_property("parameter_name", "EmissiveStrength"); es.set_editor_property("default_value", 0.0)
    m1 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 720)
    MEL.connect_material_expressions(em, "R", m1, "A"); MEL.connect_material_expressions(ec, "", m1, "B")
    m2 = MEL.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 800)
    MEL.connect_material_expressions(m1, "", m2, "A"); MEL.connect_material_expressions(es, "", m2, "B")
    MEL.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(nm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rmul, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(m2, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.recompile_material(m)
    log("created M_P9_Creature8A")
master = load(mpath)

def mi(name, folder, parent, scalars=None, vectors=None, textures=None):
    a = get_or_create(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(a, parent)
    for k, v in (scalars or {}).items(): MEL.set_material_instance_scalar_parameter_value(a, k, v)
    for k, v in (vectors or {}).items(): MEL.set_material_instance_vector_parameter_value(a, k, v)
    for k, v in (textures or {}).items(): MEL.set_material_instance_texture_parameter_value(a, k, load(v))
    MEL.update_material_instance(a)
    return a

LM, GR = "/Game/P9/Creatures8A/LanternMan", "/Game/P9/Creatures8A/Greeting"
mi_lm = mi("MI_P9_LanternMan", MF, master, {"EmissiveStrength": 6.0, "RoughnessScale": 1.0}, {"EmissiveColor": unreal.LinearColor(1.0, 0.52, 0.18, 1.0)},
           {"BaseColorTex": LM + "/T_P9_LanternMan_BaseColor", "NormalTex": LM + "/T_P9_LanternMan_Normal", "RoughnessTex": LM + "/T_P9_LanternMan_Roughness",
            "EmissiveMaskTex": LM + "/T_P9_LanternMan_Emissive"})
mi_gr = mi("MI_P9_Greeting", MF, master, {"EmissiveStrength": 0.0, "RoughnessScale": 1.0}, {},
           {"BaseColorTex": GR + "/T_P9_Greeting_BaseColor", "NormalTex": GR + "/T_P9_Greeting_Normal", "RoughnessTex": GR + "/T_P9_Greeting_Roughness"})

def lods(mesh, reductions):
    opts = unreal.EditorScriptingMeshReductionOptions()
    opts.set_editor_property("auto_compute_lod_screen_size", False)
    rs = []
    for pct, screen in reductions:
        s = unreal.EditorScriptingMeshReductionSettings(); s.set_editor_property("percent_triangles", pct); s.set_editor_property("screen_size", screen); rs.append(s)
    opts.set_editor_property("reduction_settings", rs)
    return unreal.EditorStaticMeshLibrary.set_lods(mesh, opts)

for path, mat, red in ((LM + "/SM_P9_LanternMan", mi_lm, [(1.0, 1.0), (0.45, 0.35), (0.18, 0.15)]),
                       (GR + "/SM_P9_Greeting_rest", mi_gr, [(1.0, 1.0), (0.5, 0.3)]),
                       (GR + "/SM_P9_Greeting_lunge", mi_gr, [(1.0, 1.0), (0.5, 0.3)])):
    sm = load(path)
    for i in range(len(sm.get_editor_property("static_materials"))):
        sm.set_material(i, mat)
    n = lods(sm, red)
    b = sm.get_bounding_box()
    log("%s: material %s, LODs %d, tris LOD0 %d, bounds %s .. %s" % (sm.get_name(), mat.get_name(), n, unreal.EditorStaticMeshLibrary.get_number_verts(sm, 0),
                                                                     [round(x) for x in (b.min.x, b.min.y, b.min.z)], [round(x) for x in (b.max.x, b.max.y, b.max.z)]))

# ---------------------------------------------------------------- endless pieces: the station's own material instances by slot name
def slot_map(paths):
    m = {}
    for p in paths:
        sm = load(p)
        for s in sm.get_editor_property("static_materials"):
            base = str(s.material_slot_name).split(".")[0]
            for suf in ("_002", "_001"):
                if base.endswith(suf): base = base[:-len(suf)]
            m[base] = s.material_interface
    return m

real = slot_map(["/Game/P9/Environment/Footbridge/SM_P9_FOBShell_B", "/Game/P9/Environment/FootbridgeFloor/SM_P9_FOBFloor_DeckB",
                 "/Game/P9/Environment/StairFinish/SM_P9_StairTreads"])
log("real slot map: %s" % {k: (v.get_name() if v else None) for k, v in real.items()})
EN = "/Game/P9/Anomalies8A/Endless"
for n in ("SM_P9_FOBB_South8A", "SM_P9_FOBB_End8A", "SM_P9_FOBB_Bay8A", "SM_P9_FOBB_BayLeadIn8A", "SM_P9_StairPF910_Upper8A", "SM_P9_EscalatorStep8A"):
    sm = load(EN + "/" + n)
    names = []
    for i, s in enumerate(sm.get_editor_property("static_materials")):
        base = str(s.material_slot_name).split(".")[0]
        for suf in ("_002", "_001"):
            if base.endswith(suf): base = base[:-len(suf)]
        mat = real.get(base) or (real.get("M_P9_Plaster") if "Plaster" in base else None)
        if mat:
            sm.set_material(i, mat)
        names.append("%s->%s" % (s.material_slot_name, mat.get_name() if mat else "UNMAPPED"))
    bs = sm.get_editor_property("body_setup")
    convex = len(bs.get_editor_property("agg_geom").get_editor_property("convex_elems")) if bs else 0
    b = sm.get_bounding_box()
    log("%s: %s, convex %d, bounds %s .. %s" % (n, names, convex, [round(x, 1) for x in (b.min.x, b.min.y, b.min.z)], [round(x, 1) for x in (b.max.x, b.max.y, b.max.z)]))

# ---------------------------------------------------------------- red seat material instances
seat = load("/Game/P9/Environment/Trains/Materials/MI_P9_Train_Seat")
parent = seat.get_editor_property("parent")
for name, texn in (("MI_P9_Anom8_TrainSeat_Red", "T_P9_Anom8_TrainSeat_Red"), ("MI_P9_Anom8_TrainSeat_DarkRed", "T_P9_Anom8_TrainSeat_DarkRed")):
    a = mi(name, "/Game/P9/Anomalies8A/Materials", parent, {"Roughness": 0.25}, {}, {"BaseTexture": "/Game/P9/Anomalies8A/Textures/" + texn})
    log("%s parent %s" % (name, parent.get_name()))

# ---------------------------------------------------------------- audio
AU = "/Game/P9/Audio/OriginalAudioPack"
classes = {}
for n in ("SC_P9_Music", "SC_P9_StationAmbience", "SC_P9_Railway", "SC_P9_Player", "SC_P9_NPC", "SC_P9_Creature", "SC_P9_HorrorFX"):
    classes[n] = get_or_create(n, AU + "/Classes", unreal.SoundClass, unreal.SoundClassFactory())
def attenuation(name, inner, falloff):
    a = get_or_create(name, AU + "/Classes", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    s = a.get_editor_property("attenuation")
    s.set_editor_property("attenuate", True); s.set_editor_property("spatialize", True)
    s.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0, 0)); s.set_editor_property("falloff_distance", falloff)
    a.set_editor_property("attenuation", s)
    return a
att_near = attenuation("ATT_P9_Positional_Near", 150.0, 1500.0)
att_far = attenuation("ATT_P9_Positional_Far", 400.0, 4000.0)
counts = {}
for folder in ("Music", "Ambience", "SFX", "Creatures"):
    for p in EAL.list_assets(AU + "/" + folder, recursive=False):
        w = unreal.load_asset(p)
        if not isinstance(w, unreal.SoundWave):
            continue
        n = w.get_name()
        loop = n.endswith("_Loop") or n in ("BGM_Main_Station_Ambience", "BGM_Platform9_Horror", "BGM_Anomaly_Ambience")
        w.set_editor_property("looping", loop)
        if folder == "Music":
            cls = "SC_P9_Music"; w.set_editor_property("compression_quality", 45)
        elif folder == "Ambience":
            cls = "SC_P9_StationAmbience"; w.set_editor_property("compression_quality", 45)
        elif n.startswith(("SFX_Train", "SFX_Rail")):
            cls = "SC_P9_Railway"; w.set_editor_property("attenuation_settings", att_far)
        elif n.startswith(("SFX_Footstep", "SFX_Jump", "SFX_Land", "SFX_Interact", "SFX_Gate")):
            cls = "SC_P9_Player"
        elif n.startswith("SFX_Horror"):
            cls = "SC_P9_HorrorFX"
        elif n.startswith("SFX_Kanchana"):
            cls = "SC_P9_NPC"; w.set_editor_property("attenuation_settings", att_near)
        else:
            cls = "SC_P9_Creature"
            if n.startswith("SFX_LanternMan") or n == "SFX_Greeting_DistantCall":
                w.set_editor_property("attenuation_settings", att_far)
        w.set_editor_property("sound_class_object", classes[cls])
        counts[cls] = counts.get(cls, 0) + 1
log("audio classes: %s" % counts)

for d in ("/Game/P9/Creatures8A", "/Game/P9/Anomalies8A", AU):
    EAL.save_directory(d, only_if_is_dirty=True, recursive=True)
log("saved new asset folders")
