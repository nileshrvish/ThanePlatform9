# Stage 9C: extend M_P9_TexturedLit (backed up in content_pre9C_*). Every new parameter defaults to the old look:
#   BaseColor = Desaturate(Tex * Tint, 1 - BaseSaturation[1]) * BaseBrightness[1]
#   Roughness = clamp(Roughness + RoughnessVariation[0] * (0.5 - luminance(Tex)), 0.04, 1)
#   Normal    = UseNormalMap[false] ? lerp((0,0,1), NormalMap, NormalStrength[1]) : (0,0,1)
#   Specular  = Specular[0.5]
import unreal
MEL = unreal.MaterialEditingLibrary
log = lambda s: unreal.log("P9PY: " + s)
m = unreal.load_asset("/Game/P9/Environment/Shared/M_P9_TexturedLit")
if "BaseBrightness" in [str(n) for n in MEL.get_scalar_parameter_names(m)]:
    raise SystemExit("already upgraded")
mul = MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_BASE_COLOR)
tex = MEL.get_inputs_for_material_expression(m, mul)[0]
rough = MEL.get_material_property_input_node(m, unreal.MaterialProperty.MP_ROUGHNESS)
X = lambda cls, x, y: MEL.create_material_expression(m, cls, x, y)
def sparam(name, val, x, y):
    e = X(unreal.MaterialExpressionScalarParameter, x, y); e.set_editor_property("parameter_name", name); e.set_editor_property("default_value", val); return e
C = MEL.connect_material_expressions
# base colour
sat = sparam("BaseSaturation", 1.0, -300, -500)
om = X(unreal.MaterialExpressionOneMinus, -150, -500); C(sat, "", om, "")
des = X(unreal.MaterialExpressionDesaturation, 0, -400)
ok1 = C(mul, "", des, "") or C(mul, "", des, "Input"); ok2 = C(om, "", des, "Fraction")
bri = sparam("BaseBrightness", 1.0, 0, -550)
bm = X(unreal.MaterialExpressionMultiply, 200, -450); C(des, "", bm, "A"); C(bri, "", bm, "B")
MEL.connect_material_property(bm, "", unreal.MaterialProperty.MP_BASE_COLOR)
# roughness variation
w = X(unreal.MaterialExpressionConstant3Vector, -400, 200); w.set_editor_property("constant", unreal.LinearColor(0.2126, 0.7152, 0.0722, 0))
dot = X(unreal.MaterialExpressionDotProduct, -200, 150); C(tex, "RGB", dot, "A"); C(w, "", dot, "B")
half = X(unreal.MaterialExpressionConstant, -200, 260); half.set_editor_property("r", 0.5)
sub = X(unreal.MaterialExpressionSubtract, 0, 200); C(half, "", sub, "A"); C(dot, "", sub, "B")
rv = sparam("RoughnessVariation", 0.0, 0, 320)
rm = X(unreal.MaterialExpressionMultiply, 150, 250); C(sub, "", rm, "A"); C(rv, "", rm, "B")
ad = X(unreal.MaterialExpressionAdd, 300, 150); C(rough, "", ad, "A"); C(rm, "", ad, "B")
cl = X(unreal.MaterialExpressionClamp, 450, 150); cl.set_editor_property("min_default", 0.04); cl.set_editor_property("max_default", 1.0); C(ad, "", cl, "")
MEL.connect_material_property(cl, "", unreal.MaterialProperty.MP_ROUGHNESS)
# normal
nt = X(unreal.MaterialExpressionTextureSampleParameter2D, -300, 500); nt.set_editor_property("parameter_name", "NormalMap")
nt.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
nt.set_editor_property("texture", unreal.load_asset("/Engine/EngineMaterials/DefaultNormal"))
flat = X(unreal.MaterialExpressionConstant3Vector, -100, 650); flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 0))
ns = sparam("NormalStrength", 1.0, -100, 750)
lp = X(unreal.MaterialExpressionLinearInterpolate, 100, 550); C(flat, "", lp, "A"); C(nt, "RGB", lp, "B"); C(ns, "", lp, "Alpha")
sw = X(unreal.MaterialExpressionStaticSwitchParameter, 300, 600); sw.set_editor_property("parameter_name", "UseNormalMap"); sw.set_editor_property("default_value", False)
C(lp, "", sw, "True"); C(flat, "", sw, "False")
MEL.connect_material_property(sw, "", unreal.MaterialProperty.MP_NORMAL)
spec = sparam("Specular", 0.5, 300, 400)
MEL.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
MEL.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m, False)
log("master upgraded: desat %s/%s scalars %s static %s" % (ok1, ok2, MEL.get_scalar_parameter_names(m), MEL.get_static_switch_parameter_names(m)))
