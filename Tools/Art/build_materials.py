"""Builds the world's materials inside the Unreal editor, headless.

Imports the photo-scanned textures fetched by fetch_textures.py and builds:

  M_DTG_Surface   triplanar PBR for generated geometry: colour, normal and ORM projected from world
                  space, so procedural meshes need no UVs. Vertex colour can tint it (TintStrength).
  M_DTG_Paint     clear-coat car paint, colour from the vertex colour.
  M_DTG_Plain     vertex colour with roughness/metallic/colour-scale parameters: glass, chrome,
                  rubber, skin, cloth, leaves, and the fallback for anything not yet classified.

and one material instance per surface kind under /Game/Art/Materials. Run from the repo root:

  UnrealEditor-Cmd.exe DustToGlory.uproject -run=pythonscript -script="Tools/Art/build_materials.py"
      -unattended -nosplash -stdout

Re-running replaces everything it made; nothing else under /Game is touched.
"""
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SOURCE = os.path.join(ROOT, "ArtSource", "PolyHaven")
TEXTURES = "/Game/Art/Textures"
MATERIALS = "/Game/Art/Materials"

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# The parameters the game drives at runtime (ADTGSky): Night, for the windows that light up, and Wet, for
# the roads in the rain. Made once and kept, since the materials reference it.
COLLECTION = None


def build_collection():
    path = MATERIALS + "/MPC_DTG"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        collection = unreal.load_asset(path)
    else:
        collection = tools.create_asset("MPC_DTG", MATERIALS, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    # Keep what is there. Every parameter carries an id the materials reference by, and a fresh list
    # with fresh ids leaves every material built against the old one failing to compile (seen: a grey
    # checkerboard world after a window-only rebuild). Only what is missing is added.
    params = list(collection.get_editor_property("scalar_parameters"))
    have = {str(param.get_editor_property("parameter_name")) for param in params}
    added = []
    for name, value in (("Night", 0.0), ("Wet", 0.0)):
        if name in have:
            continue
        param = unreal.CollectionScalarParameter()
        param.set_editor_property("parameter_name", name)
        param.set_editor_property("default_value", value)
        params.append(param)
        added.append(name)
    if added:
        collection.set_editor_property("scalar_parameters", params)
        log("collection parameters added: " + ", ".join(added))
    return collection


def collection_param(material, name, x, y):
    node = expr(material, unreal.MaterialExpressionCollectionParameter, x, y)
    node.set_editor_property("collection", COLLECTION)
    node.set_editor_property("parameter_name", name)
    return node

# Kind -> (tile size in cm, how much the vertex colour tints it, roughness scale).
# Tint 0 keeps the photo's own colour (roads, ground, bark); facades take the building's colour.
# Kind -> (tile cm, tint strength, roughness scale, weathering) on M_DTG_Surface.
SURFACES = {
    "Asphalt": (400.0, 0.0, 1.0, 0.0),
    "Sidewalk": (200.0, 0.0, 1.0, 0.0),
    "Concrete": (300.0, 0.35, 1.0, 0.7),
    "Brick": (180.0, 0.25, 1.0, 0.8),
    "Stucco": (250.0, 0.85, 1.0, 1.0),
    "WoodSiding": (200.0, 0.9, 1.0, 0.6),
    "ShingleRoof": (300.0, 0.5, 1.0, 0.3),
    "MetalRoof": (250.0, 0.6, 0.8, 0.5),
    # Poly Haven's grass photographs are all dry or leaf-strewn (leafy_grass averages sRGB 151,131,89, as
    # tan as the sand), so the photo supplies the detail and the park's vertex colour supplies the green.
    "Grass": (300.0, 0.85, 1.0, 0.0),
    "Sand": (300.0, 0.0, 1.0, 0.0),
    "Dirt": (300.0, 0.0, 1.0, 0.0),
    "ForestFloor": (300.0, 0.0, 1.0, 0.0),
    "PineBark": (120.0, 0.0, 1.0, 0.0),
    "PalmBark": (120.0, 0.0, 1.0, 0.0),
    "Bark": (120.0, 0.0, 1.0, 0.0),
}

# Kind -> (roughness, metallic, colour scale) on M_DTG_Plain.
PLAIN = {
    "Matte": (0.75, 0.0, 1.0),
    "Glass": (0.04, 0.9, 0.25),
    "Chrome": (0.12, 1.0, 1.0),
    "Rubber": (0.9, 0.0, 0.6),
    "Skin": (0.55, 0.0, 1.0),
    "Cloth": (0.85, 0.0, 1.0),
    "Leaves": (0.65, 0.0, 1.0),
    "Water": (0.03, 0.0, 1.0),
}

# The terrain's vertex colour is the ground's make-up, set in DTGWorldBuilder::BuildChunk: R grass, G sand,
# B clay (lake bed, upland rock), A forest floor (needles and leaf litter under the trees). Weights, not a
# colour to classify: the first version classified a photographic colour by its nearest class, and the
# colour a triangle interpolates between a grass vertex and a sand vertex passed nearest the clay class,
# which drew a red seam along every shoulder and field edge.
TERRAIN_WEIGHTS = """
float4 Wt = float4(saturate(C), saturate(CA)) + 1e-4; Wt /= dot(Wt, float4(1.0, 1.0, 1.0, 1.0));
float3 Grass = float3(0.17, 0.30, 0.12);
float2 Uv = P.xy;
"""

TERRAIN_COLOUR = TERRAIN_WEIGHTS + """
// The grass photo is dry and tan; keep its detail and take the colour from the class green.
float3 GrassPhoto = Texture2DSample(GrassD, GrassDSampler, Uv).rgb;
float GrassLum = dot(GrassPhoto, float3(0.3, 0.59, 0.11));
float3 GrassC = lerp(GrassPhoto, saturate(GrassLum * 2.2 * Grass), 0.85);
return GrassC * Wt.x
     + Texture2DSample(SandD, SandDSampler, Uv).rgb * Wt.y
     + Texture2DSample(DirtD, DirtDSampler, Uv).rgb * Wt.z
     + Texture2DSample(FloorD, FloorDSampler, Uv).rgb * Wt.w;
"""

TERRAIN_ORM = TERRAIN_WEIGHTS + """
return Texture2DSample(GrassO, GrassOSampler, Uv).rgb * Wt.x
     + Texture2DSample(SandO, SandOSampler, Uv).rgb * Wt.y
     + Texture2DSample(DirtO, DirtOSampler, Uv).rgb * Wt.z
     + Texture2DSample(FloorO, FloorOSampler, Uv).rgb * Wt.w;
"""

TERRAIN_NORMAL = TERRAIN_WEIGHTS + """
float2 T = (Texture2DSample(GrassN, GrassNSampler, Uv).rg * 2.0 - 1.0) * Wt.x
         + (Texture2DSample(SandN, SandNSampler, Uv).rg * 2.0 - 1.0) * Wt.y
         + (Texture2DSample(DirtN, DirtNSampler, Uv).rg * 2.0 - 1.0) * Wt.z
         + (Texture2DSample(FloorN, FloorNSampler, Uv).rg * 2.0 - 1.0) * Wt.w;
float3 N = normalize(Nrm);
float3 TN = float3(T, sqrt(saturate(1.0 - dot(T, T))));
return normalize(float3(TN.xy + N.xy, abs(TN.z) * N.z));
"""

# Shared by the three triplanar nodes: weights and per-axis UVs (after Ben Golus's whiteout triplanar).
# A second copy of the texture, rotated a quarter turn and rescaled, is blended in by a world-space
# noise (Mask), so the repeat never lines up along a wall: the thing that made every brick building
# read as a texture rather than as brick.
TRIPLANAR_SETUP = """
float3 N = normalize(Nrm);
float3 W = pow(abs(N), 4.0); W /= dot(W, float3(1.0, 1.0, 1.0));
float3 S = float3(N.x < 0 ? -1.0 : 1.0, N.y < 0 ? -1.0 : 1.0, N.z < 0 ? -1.0 : 1.0);
float2 UvX = P.zy; float2 UvY = P.xz; float2 UvZ = P.xy;
UvX.x *= S.x; UvY.x *= S.y; UvZ.x *= -S.z;
float2 VX = float2(-UvX.y, UvX.x) * 0.71 + 0.37; float2 VY = float2(-UvY.y, UvY.x) * 0.71 + 0.37; float2 VZ = float2(-UvZ.y, UvZ.x) * 0.71 + 0.37;
// The rotated copy is for walls. On anything facing up it turns the ribs of a metal roof and the courses
// of a shingle roof sideways in rectangles the size of the noise's lattice (the blotches on every roof
// in the Milestone 2 captures), so it fades out as the surface turns upward.
float Upright = 1.0 - smoothstep(0.3, 0.7, abs(N.z));
float Blend = smoothstep(0.35, 0.65, Mask) * Upright;
"""



def hash2(q):
    return "frac(sin(dot(%s, float2(127.1, 311.7))) * 43758.5453)" % q


def value_noise(p):
    """Smooth 0..1 value noise of a float2 expression, expanded inline (custom nodes cannot declare functions)."""
    f = "floor(%s)" % p
    t = "smoothstep(0.0, 1.0, frac(%s))" % p
    return ("lerp(lerp(%s, %s, %s.x), lerp(%s, %s, %s.x), %s.y)" % (
        hash2(f), hash2(f + " + float2(1.0, 0.0)"), t, hash2(f + " + float2(0.0, 1.0)"), hash2(f + " + float2(1.0, 1.0)"), t, t))


# Weathering, for walls: rain streaks running down in patches, and blotches of grime and repair, so a
# wall is not one photograph repeated end to end. u runs along the wall whichever way it faces; the
# coordinates are in tiles, so a tile of 2.5 m gives streaks a hand wide and patches a few metres across.
WEATHER_CODE = """
float u = abs(N.x) > abs(N.y) ? P.y : P.x;
float2 sp = float2(u * 9.0, P.z * 0.45);
float2 pp = float2(u * 0.55 + 3.1, P.z * 0.55);
float2 bp = float2(u * 0.8 + 11.7, P.z * 0.8 + 5.2);
float Streak = %s;
float Patch = smoothstep(0.4, 0.85, %s);
float Blotch = %s;
// Streaks run down walls; on a roof P.z is constant and they would be bands, so the weathering fades out as
// the surface turns upward.
float Dirt = Weathering * Patch * Streak * Streak * Upright;
""" % (value_noise("sp"), value_noise("pp"), value_noise("bp"))

COLOUR_CODE = TRIPLANAR_SETUP + WEATHER_CODE + """
float3 C1 = Texture2DSample(Tex, TexSampler, UvX).rgb * W.x + Texture2DSample(Tex, TexSampler, UvY).rgb * W.y + Texture2DSample(Tex, TexSampler, UvZ).rgb * W.z;
float3 C2 = Texture2DSample(Tex, TexSampler, VX).rgb * W.x + Texture2DSample(Tex, TexSampler, VY).rgb * W.y + Texture2DSample(Tex, TexSampler, VZ).rgb * W.z;
float3 C = lerp(C1, C2, Blend);
float Lum = dot(C, float3(0.3, 0.59, 0.11));
C = lerp(C, saturate(Lum * 2.0 * Tint), TintStrength);
// Grime and damp gather at the foot of a wall: vertex alpha is the height above the ground, 0 to 1 over 2.5 m.
C *= lerp(0.62, 1.0, Grime) * lerp(0.92, 1.0, Mask);
// Rain darkens whatever faces up.
C *= lerp(1.0, 0.5, Wet * saturate(N.z));
// Weathering: the streaks darken, the blotches go either way.
C *= (1.0 - 0.3 * Dirt) * lerp(1.0, lerp(0.92, 1.06, Blotch), Weathering * Upright);
return C;
"""

ORM_CODE = TRIPLANAR_SETUP + WEATHER_CODE + """
float3 O1 = Texture2DSample(Tex, TexSampler, UvX).rgb * W.x + Texture2DSample(Tex, TexSampler, UvY).rgb * W.y + Texture2DSample(Tex, TexSampler, UvZ).rgb * W.z;
float3 O2 = Texture2DSample(Tex, TexSampler, VX).rgb * W.x + Texture2DSample(Tex, TexSampler, VY).rgb * W.y + Texture2DSample(Tex, TexSampler, VZ).rgb * W.z;
float3 O = lerp(O1, O2, Blend);
// Damp at the foot of the wall is glossier, and rain makes whatever faces up shine.
O.g *= lerp(0.8, 1.0, Grime) * lerp(1.0, 0.18, Wet * saturate(N.z));
// Dirt is rougher than the wall it is on.
O.g = saturate(O.g + 0.25 * Dirt);
return O;
"""

NORMAL_CODE = TRIPLANAR_SETUP + """
float2 Nx = lerp(Texture2DSample(Tex, TexSampler, UvX).rg, Texture2DSample(Tex, TexSampler, VX).gr * float2(1.0, -1.0) + float2(0.0, 1.0), Blend) * 2.0 - 1.0;
float2 Ny = lerp(Texture2DSample(Tex, TexSampler, UvY).rg, Texture2DSample(Tex, TexSampler, VY).gr * float2(1.0, -1.0) + float2(0.0, 1.0), Blend) * 2.0 - 1.0;
float2 Nz = lerp(Texture2DSample(Tex, TexSampler, UvZ).rg, Texture2DSample(Tex, TexSampler, VZ).gr * float2(1.0, -1.0) + float2(0.0, 1.0), Blend) * 2.0 - 1.0;
float3 TX = float3(Nx, sqrt(saturate(1.0 - dot(Nx, Nx))));
float3 TY = float3(Ny, sqrt(saturate(1.0 - dot(Ny, Ny))));
float3 TZ = float3(Nz, sqrt(saturate(1.0 - dot(Nz, Nz))));
TX.x *= S.x; TY.x *= S.y; TZ.x *= -S.z;
TX = float3(TX.xy + N.zy, abs(TX.z) * N.x);
TY = float3(TY.xy + N.xz, abs(TY.z) * N.y);
TZ = float3(TZ.xy + N.xy, abs(TZ.z) * N.z);
return normalize(TX.zyx * W.x + TY.xzy * W.y + TZ.xyz * W.z);
"""


def log(message):
    unreal.log("DTGArt: " + message)


def import_textures():
    # Only what is missing. Re-importing an unchanged texture rewrites its .uasset byte for byte
    # differently, and every rewrite is another full copy in Git LFS. Pass --reimport to force it.
    reimport = "--reimport" in unreal.SystemLibrary.get_command_line()
    tasks = []
    for name in sorted(os.listdir(SOURCE)):
        if not name.lower().endswith(".jpg"):
            continue
        if not reimport and unreal.EditorAssetLibrary.does_asset_exist(TEXTURES + "/" + os.path.splitext(name)[0]):
            continue
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", os.path.join(SOURCE, name))
        task.set_editor_property("destination_path", TEXTURES)
        task.set_editor_property("destination_name", os.path.splitext(name)[0])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for task in tasks:
        for path in task.get_editor_property("imported_object_paths"):
            texture = unreal.load_asset(path)
            name = texture.get_name()
            if name.endswith("_N"):
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                texture.set_editor_property("srgb", False)
            elif name.endswith("_ORM"):
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                texture.set_editor_property("srgb", False)
            texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    log("imported %d textures" % len(tasks))


def fresh_material(name):
    path = MATERIALS + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    return tools.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())


def expr(material, cls, x, y):
    return mel.create_material_expression(material, cls, x, y)


def scalar(material, name, value, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def custom(material, code, output_type, inputs, x, y, description):
    node = expr(material, unreal.MaterialExpressionCustom, x, y)
    node.set_editor_property("code", code)
    node.set_editor_property("output_type", output_type)
    node.set_editor_property("description", description)
    pins = []
    for name in inputs:
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", name)
        pins.append(pin)
    node.set_editor_property("inputs", pins)
    return node


def world_noise(material, x, y):
    """Smooth world-space noise, 0..1, with features a few metres across."""
    node = expr(material, unreal.MaterialExpressionNoise, x, y)
    node.set_editor_property("scale", 0.003)
    node.set_editor_property("levels", 2)
    node.set_editor_property("output_min", 0.0)
    node.set_editor_property("output_max", 1.0)
    node.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_ALU)
    world = expr(material, unreal.MaterialExpressionWorldPosition, x - 250, y)
    mel.connect_material_expressions(world, "", node, "Position")
    return node


# Window glass with a room behind it, computed in the shader from the view direction (interior
# mapping): a lit or dark box per window, the back wall, ceiling and floor shaded differently, fading
# with depth. The glass itself is a smooth dielectric, so Lumen puts the sky and the street in it.
INTERIOR_CODE = """
float3 T = normalize(cross(float3(0.0, 0.0, 1.0), N));
float3 d = -float3(dot(V, T), V.z, dot(V, N));
d.z = min(d.z, -0.08);
float2 cell = floor(UV);
float2 f = UV - cell;
float3 p = float3(f.x, f.y, 0.0);
float3 inv = 1.0 / d;
float3 tm = float3(d.x > 0.0 ? (1.0 - p.x) : -p.x, d.y > 0.0 ? (1.0 - p.y) : -p.y, -Depth) * inv;
float t = min(min(tm.x, tm.y), tm.z);
float3 hit = p + d * t;
float h = frac(sin(dot(cell + Seed, float2(12.9898, 78.233))) * 43758.5453);
float lit = h < LitFraction ? 1.0 : 0.0;
float3 room = float3(0.60, 0.56, 0.50) * (0.3 + 0.7 * lit);
float3 c;
if (t == tm.z) { c = room * 0.6 + lit * 0.25 * float3(1.0, 0.93, 0.8) * saturate(1.0 - 1.4 * abs(hit.x - 0.5)); }
else if (t == tm.y) { c = d.y > 0.0 ? room * 1.1 : room * 0.35; }
else { c = room * 0.75; }
c *= lerp(1.0, 0.5, saturate(-hit.z / Depth));
return float4(c, lit);
"""


def build_window_master():
    material = fresh_material("M_DTG_Window")
    uv = expr(material, unreal.MaterialExpressionTextureCoordinate, -1200, -200)
    uv.set_editor_property("coordinate_index", 0)
    normal_ws = expr(material, unreal.MaterialExpressionVertexNormalWS, -1200, -50)
    camera = expr(material, unreal.MaterialExpressionCameraVectorWS, -1200, 100)
    depth = scalar(material, "RoomDepth", 1.1, -1200, 250)
    lit = scalar(material, "LitFraction", 0.45, -1200, 350)
    seed = scalar(material, "Seed", 0.0, -1200, 450)
    interior = custom(material, INTERIOR_CODE, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ["UV", "N", "V", "Depth", "LitFraction", "Seed"],
                      -800, 0, "Interior mapping")
    for node, name in ((uv, "UV"), (normal_ws, "N"), (camera, "V"), (depth, "Depth"), (lit, "LitFraction"), (seed, "Seed")):
        mel.connect_material_expressions(node, "", interior, name)
    rgb = expr(material, unreal.MaterialExpressionComponentMask, -500, -50)
    for flag, value in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(flag, value)
    mel.connect_material_expressions(interior, "", rgb, "")
    alpha = expr(material, unreal.MaterialExpressionComponentMask, -500, 150)
    for flag, value in (("r", False), ("g", False), ("b", False), ("a", True)):
        alpha.set_editor_property(flag, value)
    mel.connect_material_expressions(interior, "", alpha, "")
    tint = scalar(material, "GlassTint", 0.5, -500, -200)
    tinted = expr(material, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(rgb, "", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")
    # Lit rooms glow after dark: Night is 0 by day; the sky can raise it.
    night = collection_param(material, "Night", -500, 300)
    glow = expr(material, unreal.MaterialExpressionMultiply, -300, 150)
    mel.connect_material_expressions(rgb, "", glow, "A")
    mel.connect_material_expressions(alpha, "", glow, "B")
    glow_night = expr(material, unreal.MaterialExpressionMultiply, -150, 200)
    mel.connect_material_expressions(glow, "", glow_night, "A")
    mel.connect_material_expressions(night, "", glow_night, "B")
    # Tungsten, and about half the strength of the room as seen by day, or the windows blow out white
    # against moonlight once the exposure has settled on the street.
    warm = expr(material, unreal.MaterialExpressionConstant3Vector, -150, 350)
    warm.set_editor_property("constant", unreal.LinearColor(0.55, 0.43, 0.27, 1.0))
    glow_warm = expr(material, unreal.MaterialExpressionMultiply, 0, 250)
    mel.connect_material_expressions(glow_night, "", glow_warm, "A")
    mel.connect_material_expressions(warm, "", glow_warm, "B")
    roughness = scalar(material, "Roughness", 0.06, -500, 450)
    specular = scalar(material, "Specular", 1.0, -500, 550)
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(glow_warm, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


def tile_position(material, tile_cm):
    """World position in tiles, wrapped every 64 tiles so the large-world coordinates stay precise."""
    world = expr(material, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tile = scalar(material, "TileCm", tile_cm, -1600, 150)
    in_tiles = expr(material, unreal.MaterialExpressionDivide, -1400, 0)
    mel.connect_material_expressions(world, "", in_tiles, "A")
    mel.connect_material_expressions(tile, "", in_tiles, "B")
    per_wrap = expr(material, unreal.MaterialExpressionDivide, -1250, 0)
    per_wrap.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(in_tiles, "", per_wrap, "A")
    wrapped = expr(material, unreal.MaterialExpressionFrac, -1100, 0)
    mel.connect_material_expressions(per_wrap, "", wrapped, "")
    position = expr(material, unreal.MaterialExpressionMultiply, -950, 0)
    position.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(wrapped, "", position, "A")
    return position


def build_terrain_master():
    material = fresh_material("M_DTG_Terrain")
    material.set_editor_property("tangent_space_normal", False)
    position = tile_position(material, 450.0)
    normal_ws = expr(material, unreal.MaterialExpressionVertexNormalWS, -950, 200)
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -950, 350)

    def texture(name, y, sampler):
        node = expr(material, unreal.MaterialExpressionTextureObject, -950, y)
        node.set_editor_property("texture", unreal.load_asset(TEXTURES + "/" + name))
        node.set_editor_property("sampler_type", sampler)
        return node

    colour_s = unreal.MaterialSamplerType.SAMPLERTYPE_COLOR
    normal_s = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    mask_s = unreal.MaterialSamplerType.SAMPLERTYPE_MASKS
    float3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    outputs = {}
    for row, (suffix, code, sampler, label) in enumerate((("D", TERRAIN_COLOUR, colour_s, "Terrain colour"),
                                                           ("N", TERRAIN_NORMAL, normal_s, "Terrain normal"),
                                                           ("O", TERRAIN_ORM, mask_s, "Terrain ORM"))):
        file_suffix = {"D": "D", "N": "N", "O": "ORM"}[suffix]
        kinds = (("Grass", "Grass"), ("Sand", "Sand"), ("Dirt", "Dirt"), ("Floor", "ForestFloor"))
        names = [kind + suffix for kind, _ in kinds]
        node = custom(material, code, float3, names + ["C", "CA", "P", "Nrm"], -600, -700 + row * 250, label)
        for col, (kind, file_kind) in enumerate(kinds):
            tex = texture("T_%s_%s" % (file_kind, file_suffix), -900 + row * 250 + col * 60, sampler)
            mel.connect_material_expressions(tex, "", node, names[col])
        mel.connect_material_expressions(vertex_colour, "", node, "C")
        mel.connect_material_expressions(vertex_colour, "A", node, "CA")
        mel.connect_material_expressions(position, "", node, "P")
        mel.connect_material_expressions(normal_ws, "", node, "Nrm")
        outputs[suffix] = node

    def channel(r, g, b, y):
        mask = expr(material, unreal.MaterialExpressionComponentMask, -300, y)
        for flag, value in (("r", r), ("g", g), ("b", b), ("a", False)):
            mask.set_editor_property(flag, value)
        mel.connect_material_expressions(outputs["O"], "", mask, "")
        return mask

    mel.connect_material_property(outputs["D"], "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(outputs["N"], "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(channel(True, False, False, -250), "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(channel(False, True, False, -150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    return material


def build_surface_master():
    material = fresh_material("M_DTG_Surface")
    material.set_editor_property("tangent_space_normal", False)

    # P: world position in tiles, wrapped every 64 tiles so the large-world coordinates stay precise.
    world = expr(material, unreal.MaterialExpressionWorldPosition, -1600, 0)
    tile = scalar(material, "TileCm", 300.0, -1600, 150)
    in_tiles = expr(material, unreal.MaterialExpressionDivide, -1400, 0)
    mel.connect_material_expressions(world, "", in_tiles, "A")
    mel.connect_material_expressions(tile, "", in_tiles, "B")
    per_wrap = expr(material, unreal.MaterialExpressionDivide, -1250, 0)
    per_wrap.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(in_tiles, "", per_wrap, "A")
    wrapped = expr(material, unreal.MaterialExpressionFrac, -1100, 0)
    mel.connect_material_expressions(per_wrap, "", wrapped, "")
    position = expr(material, unreal.MaterialExpressionMultiply, -950, 0)
    position.set_editor_property("const_b", 64.0)
    mel.connect_material_expressions(wrapped, "", position, "A")

    normal_ws = expr(material, unreal.MaterialExpressionVertexNormalWS, -950, 200)
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -950, 350)
    tint = scalar(material, "TintStrength", 0.0, -950, 500)
    rough_scale = scalar(material, "RoughnessScale", 1.0, -300, 700)
    weathering = scalar(material, "Weathering", 0.0, -950, 650)

    def texture_param(name, sampler, y):
        node = expr(material, unreal.MaterialExpressionTextureObjectParameter, -950, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler)
        return node

    colour_tex = texture_param("Colour", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600)
    normal_tex = texture_param("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -400)
    orm_tex = texture_param("ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -200)
    colour_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_D"))
    normal_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_N"))
    orm_tex.set_editor_property("texture", unreal.load_asset(TEXTURES + "/T_Concrete_ORM"))

    float3 = unreal.CustomMaterialOutputType.CMOT_FLOAT3
    colour = custom(material, COLOUR_CODE, float3, ["Tex", "P", "Nrm", "Tint", "TintStrength", "Mask", "Grime", "Wet", "Weathering"], -600, -600, "Triplanar colour")
    normal = custom(material, NORMAL_CODE, float3, ["Tex", "P", "Nrm", "Mask"], -600, -400, "Triplanar world normal")
    orm = custom(material, ORM_CODE, float3, ["Tex", "P", "Nrm", "Mask", "Grime", "Wet", "Weathering"], -600, -200, "Triplanar ORM")
    mask = world_noise(material, -1600, 700)
    for node, tex in ((colour, colour_tex), (normal, normal_tex), (orm, orm_tex)):
        mel.connect_material_expressions(tex, "", node, "Tex")
        mel.connect_material_expressions(position, "", node, "P")
        mel.connect_material_expressions(normal_ws, "", node, "Nrm")
        mel.connect_material_expressions(mask, "", node, "Mask")
    mel.connect_material_expressions(vertex_colour, "", colour, "Tint")
    # The vertex colour reaches a custom node as three channels; its alpha, the height above ground, goes in on its own.
    mel.connect_material_expressions(vertex_colour, "A", colour, "Grime")
    mel.connect_material_expressions(vertex_colour, "A", orm, "Grime")
    wet = collection_param(material, "Wet", -1600, 850)
    mel.connect_material_expressions(wet, "", colour, "Wet")
    mel.connect_material_expressions(wet, "", orm, "Wet")
    mel.connect_material_expressions(tint, "", colour, "TintStrength")
    mel.connect_material_expressions(weathering, "", colour, "Weathering")
    mel.connect_material_expressions(weathering, "", orm, "Weathering")

    def channel(r, g, b, y):
        mask = expr(material, unreal.MaterialExpressionComponentMask, -300, y)
        mask.set_editor_property("r", r)
        mask.set_editor_property("g", g)
        mask.set_editor_property("b", b)
        mask.set_editor_property("a", False)
        mel.connect_material_expressions(orm, "", mask, "")
        return mask

    ao = channel(True, False, False, -250)
    rough = channel(False, True, False, -150)
    metal = channel(False, False, True, -50)
    rough_scaled = expr(material, unreal.MaterialExpressionMultiply, -150, -150)
    mel.connect_material_expressions(rough, "", rough_scaled, "A")
    mel.connect_material_expressions(rough_scale, "", rough_scaled, "B")

    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(ao, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(rough_scaled, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    return material


def build_paint_master():
    # Default lit rather than clear coat: UE 5.8's Python API does not expose the clear-coat inputs
    # (MaterialProperty has no CustomData pins). Roughness and metallic per finish carry most of the
    # difference between gloss, metallic, matte, pearl and chrome; a hand-built clear coat can replace it.
    material = fresh_material("M_DTG_Paint")
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -600, 0)
    metallic = scalar(material, "Metallic", 0.1, -600, 200)
    roughness = scalar(material, "Roughness", 0.18, -600, 300)
    specular = scalar(material, "Specular", 0.6, -600, 400)
    mel.connect_material_property(vertex_colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(material)
    return material


def build_plain_master():
    material = fresh_material("M_DTG_Plain")
    vertex_colour = expr(material, unreal.MaterialExpressionVertexColor, -600, 0)
    colour_scale = scalar(material, "ColourScale", 1.0, -600, 150)
    scaled = expr(material, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(vertex_colour, "", scaled, "A")
    mel.connect_material_expressions(colour_scale, "", scaled, "B")
    roughness = scalar(material, "Roughness", 0.75, -600, 300)
    metallic = scalar(material, "Metallic", 0.0, -600, 400)
    mel.connect_material_property(scaled, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    mel.recompile_material(material)
    return material


def fresh_instance(name, parent):
    path = MATERIALS + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    instance = tools.create_asset(name, MATERIALS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    return instance


def build_instances(surface, plain, paint):
    for kind, (tile_cm, tint, rough_scale, weathering) in SURFACES.items():
        instance = fresh_instance("MI_" + kind, surface)
        for param, suffix in (("Colour", "D"), ("Normal", "N"), ("ORM", "ORM")):
            mel.set_material_instance_texture_parameter_value(instance, param, unreal.load_asset("%s/T_%s_%s" % (TEXTURES, kind, suffix)))
        mel.set_material_instance_scalar_parameter_value(instance, "TileCm", tile_cm)
        mel.set_material_instance_scalar_parameter_value(instance, "TintStrength", tint)
        mel.set_material_instance_scalar_parameter_value(instance, "RoughnessScale", rough_scale)
        mel.set_material_instance_scalar_parameter_value(instance, "Weathering", weathering)
    for kind, (roughness, metallic, colour_scale) in PLAIN.items():
        instance = fresh_instance("MI_" + kind, plain)
        mel.set_material_instance_scalar_parameter_value(instance, "Roughness", roughness)
        mel.set_material_instance_scalar_parameter_value(instance, "Metallic", metallic)
        mel.set_material_instance_scalar_parameter_value(instance, "ColourScale", colour_scale)
    fresh_instance("MI_Paint", paint)
    log("built %d surface, %d plain and 1 paint instances" % (len(SURFACES), len(PLAIN)))


def main():
    # DTG_MATERIAL_STAGES=window (or surfaces, or both, comma-separated) rebuilds only those; the
    # commandlet passes the environment through, which is simpler than script arguments.
    stages = os.environ.get("DTG_MATERIAL_STAGES", "surfaces,window").split(",")
    global COLLECTION
    COLLECTION = build_collection()
    if "surfaces" in stages:
        import_textures()
        surface = build_surface_master()
        paint = build_paint_master()
        plain = build_plain_master()
        build_instances(surface, plain, paint)
        fresh_instance("MI_Terrain", build_terrain_master())
    if "window" in stages:
        fresh_instance("MI_Window", build_window_master())
    unreal.EditorAssetLibrary.save_directory("/Game/Art", only_if_is_dirty=False, recursive=True)
    log("saved /Game/Art (%s)" % ", ".join(stages))


main()
