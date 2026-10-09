"""Imports the exported kit pieces into the Unreal project (run inside the editor):

  UnrealEditor-Cmd unreal/Sovereign.uproject -run=pythonscript -script="tools/art/ue_import.py"

Reads art/build/<Kit>/*.fbx (or $SOV_ART_BUILD), imports each as a static mesh under
/Game/Art/<Kit>/ with its vertex colours, and gives it the kit master material
/Game/Art/M_SovKit, rebuilt first: vertex colour x Tint x the painted detail tile that the vertex alpha picks
from /Game/Art/T_SovDetail (the sheet textures.py writes to <build>/Textures).
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BUILD = os.environ.get("SOV_ART_BUILD") or os.path.join(REPO, "art", "build")
MATERIAL = "/Game/Art/M_SovKit"

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


TEXTURE = "/Game/Art/T_SovDetail"


def detail_texture():
    """The painted detail sheet (textures.py): linear greyscale, tiling, with mips."""
    png = os.path.join(BUILD, "Textures", "T_SovDetail.png")
    if os.path.exists(png):
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", png)
        task.set_editor_property("destination_path", "/Game/Art")
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        tools.import_asset_tasks([task])
        tex = lib.load_asset(TEXTURE)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_GRAYSCALE)
        lib.save_asset(TEXTURE)
    return lib.load_asset(TEXTURE)


def _expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def master_material(texture):
    """Vertex colour x Tint x the painted detail tile the vertex alpha picks (kitlib PATTERNS).
    Built afresh each import, in place, so the meshes that use it stay linked."""
    if lib.does_asset_exist(MATERIAL):
        mat = lib.load_asset(MATERIAL)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_SovKit", "/Game/Art", unreal.Material, unreal.MaterialFactoryNew())
    c = mel.connect_material_expressions
    vc = _expr(mat, unreal.MaterialExpressionVertexColor, -1600, 0)
    # Vertex colours arrive as sRGB bytes: square them for roughly linear base colour.
    sq = _expr(mat, unreal.MaterialExpressionMultiply, -400, 0)
    c(vc, "", sq, "A")
    c(vc, "", sq, "B")
    tint = _expr(mat, unreal.MaterialExpressionVectorParameter, -400, 200, parameter_name="Tint", default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mul = _expr(mat, unreal.MaterialExpressionMultiply, -200, 80)
    c(sq, "", mul, "A")
    c(tint, "", mul, "B")
    # The tile: index = round(alpha * 15); column = index mod 4, row = floor(index / 4).
    a15 = _expr(mat, unreal.MaterialExpressionMultiply, -1400, 300, const_b=15.0)
    c(vc, "A", a15, "A")
    idx = _expr(mat, unreal.MaterialExpressionRound, -1250, 300)
    c(a15, "", idx, "")
    four = _expr(mat, unreal.MaterialExpressionConstant, -1250, 400, r=4.0)
    col = _expr(mat, unreal.MaterialExpressionFmod, -1100, 300)
    c(idx, "", col, "A")
    c(four, "", col, "B")
    quarter = _expr(mat, unreal.MaterialExpressionMultiply, -1100, 420, const_b=0.25)
    c(idx, "", quarter, "A")
    row = _expr(mat, unreal.MaterialExpressionFloor, -950, 420)
    c(quarter, "", row, "")
    off = _expr(mat, unreal.MaterialExpressionAppendVector, -800, 350)
    c(col, "", off, "A")
    c(row, "", off, "B")
    # Within the tile: the UVs repeat (frac), kept off the tile's edges.
    tc = _expr(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 600)
    fr = _expr(mat, unreal.MaterialExpressionFrac, -1250, 600)
    c(tc, "", fr, "")
    inner = _expr(mat, unreal.MaterialExpressionMultiply, -1100, 600, const_b=0.9375)
    c(fr, "", inner, "A")
    pad = _expr(mat, unreal.MaterialExpressionAdd, -950, 600, const_b=0.03125)
    c(inner, "", pad, "A")
    at = _expr(mat, unreal.MaterialExpressionAdd, -650, 450)
    c(pad, "", at, "A")
    c(off, "", at, "B")
    uv = _expr(mat, unreal.MaterialExpressionMultiply, -500, 450, const_b=0.25)
    c(at, "", uv, "A")
    # Mips from the unwrapped UVs, so the tile's seam does not flash the smallest mip.
    tc4 = _expr(mat, unreal.MaterialExpressionMultiply, -1100, 760, const_b=0.25)
    c(tc, "", tc4, "A")
    ddx = _expr(mat, unreal.MaterialExpressionDDX, -950, 760)
    ddy = _expr(mat, unreal.MaterialExpressionDDY, -950, 840)
    c(tc4, "", ddx, "")
    c(tc4, "", ddy, "")
    tex = _expr(mat, unreal.MaterialExpressionTextureSample, -350, 450, texture=texture,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE,
                mip_value_mode=unreal.TextureMipValueMode.TMVM_DERIVATIVE)
    c(uv, "", tex, "UVs")
    c(ddx, "", tex, "DDX(UVs)")
    c(ddy, "", tex, "DDY(UVs)")
    # Mid grey is no change: double it.
    detail = _expr(mat, unreal.MaterialExpressionMultiply, -200, 450, const_b=2.0)
    c(tex, "R", detail, "A")
    base = _expr(mat, unreal.MaterialExpressionMultiply, -50, 200)
    c(mul, "", base, "A")
    c(detail, "", base, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = _expr(mat, unreal.MaterialExpressionConstant, -200, 600, r=0.85)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(mat)
    lib.save_asset(MATERIAL)
    return mat


def import_kit(kit, files):
    tasks = []
    for path in files:
        # A fresh import every time: reimporting over an asset keeps its old material slots.
        asset = "/Game/Art/%s/%s" % (kit, os.path.splitext(os.path.basename(path))[0])
        if lib.does_asset_exist(asset):
            lib.delete_asset(asset)
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", False)
        options.set_editor_property("import_materials", False)  # slots are kept, filled with M_SovKit below
        options.set_editor_property("import_textures", False)
        options.set_editor_property("import_animations", False)
        data = options.get_editor_property("static_mesh_import_data")
        data.set_editor_property("combine_meshes", True)
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
        data.set_editor_property("auto_generate_collision", True)
        data.set_editor_property("generate_lightmap_u_vs", False)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", "/Game/Art/" + kit)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        task.set_editor_property("options", options)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    return ["/Game/Art/%s/%s" % (kit, os.path.splitext(os.path.basename(p))[0]) for p in files]


def main():
    # The classic FBX importer honours FbxImportUI (vertex colours, no materials).
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    mat = master_material(detail_texture())
    count = 0
    for kit_dir in sorted(glob.glob(os.path.join(BUILD, "*"))):
        if not os.path.isdir(kit_dir) or os.path.basename(kit_dir) == "Textures":
            continue
        kit = os.path.basename(kit_dir)
        files = sorted(glob.glob(os.path.join(kit_dir, "*.fbx")))
        for asset in import_kit(kit, files):
            mesh = lib.load_asset(asset)
            if not mesh:
                unreal.log_error("missing after import: " + asset)
                continue
            # Every slot gets the kit material; the game tints slot 1 (team colour) per instance.
            for slot in range(len(mesh.static_materials)):
                mesh.set_material(slot, mat)
            lib.save_asset(asset)
            count += 1
    unreal.log("SOVART imported %d pieces from %s" % (count, BUILD))


main()
