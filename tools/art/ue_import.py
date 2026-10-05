"""Imports the exported kit pieces into the Unreal project (run inside the editor):

  UnrealEditor-Cmd unreal/Sovereign.uproject -run=pythonscript -script="tools/art/ue_import.py"

Reads art/build/<Kit>/*.fbx (or $SOV_ART_BUILD), imports each as a static mesh under
/Game/Art/<Kit>/ with its vertex colours, and gives it the kit master material
/Game/Art/M_SovKit (vertex colour x Tint), creating that material first.
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


def master_material():
    if lib.does_asset_exist(MATERIAL):
        return lib.load_asset(MATERIAL)
    mat = tools.create_asset("M_SovKit", "/Game/Art", unreal.Material, unreal.MaterialFactoryNew())
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    # Vertex colours arrive as sRGB bytes: square them for roughly linear base colour.
    sq = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(vc, "", sq, "A")
    mel.connect_material_expressions(vc, "", sq, "B")
    tint = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 200)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 80)
    mel.connect_material_expressions(sq, "", mul, "A")
    mel.connect_material_expressions(tint, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 260)
    rough.set_editor_property("r", 0.85)
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
    mat = master_material()
    count = 0
    for kit_dir in sorted(glob.glob(os.path.join(BUILD, "*"))):
        if not os.path.isdir(kit_dir):
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
