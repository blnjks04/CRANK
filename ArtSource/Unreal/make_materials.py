"""Create the Crank master materials (vertex-color tin toy look + FX materials)."""
import unreal

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
DEST = "/Game/Crank/Materials"
MP = unreal.MaterialProperty


def new_material(name):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    return tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def const(mat, value, x, y):
    return expr(mat, unreal.MaterialExpressionConstant, x, y, r=value)


def scalar_param(mat, name, value, x, y):
    return expr(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def vector_param(mat, name, color, x, y):
    return expr(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*color))


def link(a, a_out, b, b_in):
    mel.connect_material_expressions(a, a_out, b, b_in)


def to_prop(e, out, prop):
    mel.connect_material_property(e, out, prop)


def finish(mat, ism=False, skeletal=False):
    if ism:
        mat.set_editor_property("used_with_instanced_static_meshes", True)
    if skeletal:
        mat.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    print("material", mat.get_path_name())


def vc_material(name, metallic, roughness, tint=None, skeletal=True, specular=0.5):
    mat = new_material(name)
    vc = expr(mat, unreal.MaterialExpressionVertexColor, -700, 0)
    base = vc
    if tint:
        t = vector_param(mat, "Tint", tint, -700, -200)
        mul = expr(mat, unreal.MaterialExpressionMultiply, -400, 0)
        link(vc, "", mul, "A")
        link(t, "", mul, "B")
        base = mul
    to_prop(base, "", MP.MP_BASE_COLOR)
    to_prop(scalar_param(mat, "Metallic", metallic, -400, 200), "", MP.MP_METALLIC)
    to_prop(scalar_param(mat, "Roughness", roughness, -400, 300), "", MP.MP_ROUGHNESS)
    to_prop(const(mat, specular, -400, 400), "", MP.MP_SPECULAR)
    finish(mat, ism=True, skeletal=skeletal)
    return mat


# --- Doll paint: player color x vertex color, glossy tin
mat = new_material("M_DollPaint")
vc = expr(mat, unreal.MaterialExpressionVertexColor, -800, 0)
paint = vector_param(mat, "PaintColor", (0.8, 0.12, 0.08, 1), -800, -220)
mul = expr(mat, unreal.MaterialExpressionMultiply, -500, -50)
link(vc, "", mul, "A")
link(paint, "", mul, "B")
to_prop(mul, "", MP.MP_BASE_COLOR)
to_prop(scalar_param(mat, "Metallic", 0.45, -500, 150), "", MP.MP_METALLIC)
to_prop(scalar_param(mat, "Roughness", 0.28, -500, 250), "", MP.MP_ROUGHNESS)
finish(mat, ism=True, skeletal=True)

# --- Vertex color families
vc_material("M_VC_Metal", 0.85, 0.3)
vc_material("M_VC_Matte", 0.0, 0.7)
vc_material("M_VC_Gloss", 0.0, 0.2, specular=0.6)
vc_material("M_VC_Brass", 1.0, 0.26)
vc_material("M_VC_Wood", 0.0, 0.55)
vc_material("M_VC_Cloth", 0.0, 0.92, specular=0.3)

# --- Generic solid color (graybox / fallback)
mat = new_material("M_Solid")
to_prop(vector_param(mat, "Color", (0.6, 0.6, 0.6, 1), -500, 0), "", MP.MP_BASE_COLOR)
to_prop(scalar_param(mat, "Metallic", 0.0, -500, 150), "", MP.MP_METALLIC)
to_prop(scalar_param(mat, "Roughness", 0.6, -500, 250), "", MP.MP_ROUGHNESS)
finish(mat, ism=True, skeletal=True)

# --- Emissive (vertex color glow), e.g. lamps, LEDs
mat = new_material("M_VC_Emissive")
vc = expr(mat, unreal.MaterialExpressionVertexColor, -800, 0)
strength = scalar_param(mat, "Strength", 6.0, -800, 200)
mul = expr(mat, unreal.MaterialExpressionMultiply, -500, 0)
link(vc, "", mul, "A")
link(strength, "", mul, "B")
to_prop(vc, "", MP.MP_BASE_COLOR)
to_prop(mul, "", MP.MP_EMISSIVE_COLOR)
finish(mat, ism=True, skeletal=True)

# --- Boss eyes: parameter driven emissive
mat = new_material("M_BossEyes")
eye = vector_param(mat, "EyeColor", (0.2, 8.0, 0.3, 1), -600, 0)
to_prop(eye, "", MP.MP_EMISSIVE_COLOR)
to_prop(const(mat, 0.02, -600, 200), "", MP.MP_BASE_COLOR)
to_prop(const(mat, 0.1, -600, 300), "", MP.MP_ROUGHNESS)
finish(mat)

# --- Glass (translucent)
mat = new_material("M_Glass")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("two_sided", True)
vc = expr(mat, unreal.MaterialExpressionVertexColor, -700, 0)
to_prop(vc, "", MP.MP_BASE_COLOR)
fres = expr(mat, unreal.MaterialExpressionFresnel, -700, 250)
lerp = expr(mat, unreal.MaterialExpressionLinearInterpolate, -400, 250)
link(const(mat, 0.18, -900, 250), "", lerp, "A")
link(const(mat, 0.75, -900, 350), "", lerp, "B")
link(fres, "", lerp, "Alpha")
to_prop(lerp, "", MP.MP_OPACITY)
to_prop(const(mat, 0.05, -400, 400), "", MP.MP_ROUGHNESS)
to_prop(const(mat, 0.0, -400, 480), "", MP.MP_METALLIC)
finish(mat, ism=True)


# --- FX: per-instance custom data (R, G, B, A) from the mesh particle system
def custom_rgb(mat, x, y):
    r = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y, data_index=0)
    g = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y + 80, data_index=1)
    b = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, x, y + 160, data_index=2)
    rg = expr(mat, unreal.MaterialExpressionAppendVector, x + 200, y + 40)
    link(r, "", rg, "A")
    link(g, "", rg, "B")
    rgb = expr(mat, unreal.MaterialExpressionAppendVector, x + 360, y + 80)
    link(rg, "", rgb, "A")
    link(b, "", rgb, "B")
    return rgb


mat = new_material("M_FX_Puff")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
rgb = custom_rgb(mat, -1000, 0)
to_prop(rgb, "", MP.MP_EMISSIVE_COLOR)
alpha = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -1000, 300, data_index=3)
fres = expr(mat, unreal.MaterialExpressionFresnel, -1000, 420)
inv = expr(mat, unreal.MaterialExpressionOneMinus, -800, 420)
link(fres, "", inv, "")
soft = expr(mat, unreal.MaterialExpressionMultiply, -600, 350)
link(alpha, "", soft, "A")
link(inv, "", soft, "B")
to_prop(soft, "", MP.MP_OPACITY)
finish(mat, ism=True)

mat = new_material("M_FX_Unlit")
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
rgb = custom_rgb(mat, -1000, 0)
to_prop(rgb, "", MP.MP_EMISSIVE_COLOR)
finish(mat, ism=True)

mat = new_material("M_FX_Lit")
rgb = custom_rgb(mat, -1000, 0)
to_prop(rgb, "", MP.MP_BASE_COLOR)
to_prop(const(mat, 0.6, -500, 300), "", MP.MP_METALLIC)
to_prop(const(mat, 0.35, -500, 380), "", MP.MP_ROUGHNESS)
finish(mat, ism=True)

# --- Water puddles (ISM, custom data 0 = fade)
mat = new_material("M_Water")
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
fade = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, 200, data_index=0)
op = expr(mat, unreal.MaterialExpressionMultiply, -600, 200)
link(fade, "", op, "A")
link(const(mat, 0.5, -900, 300), "", op, "B")
to_prop(op, "", MP.MP_OPACITY)
to_prop(vector_param(mat, "WaterColor", (0.35, 0.6, 0.85, 1), -600, 0), "", MP.MP_EMISSIVE_COLOR)
finish(mat, ism=True)
print("done")
