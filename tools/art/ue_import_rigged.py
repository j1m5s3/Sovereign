"""Imports the rigged people (tools/art/blender/rig_figures.py) into the Unreal project (run inside the editor):

  UnrealEditor-Cmd unreal/Sovereign.uproject -run=pythonscript -script="tools/art/ue_import_rigged.py"

Reads art/build/Rigged/SK_<Name>.fbx (or $SOV_ART_BUILD/Rigged) and imports each as a skeletal mesh with its own
skeleton and its Walk and Idle animations under /Game/Art/Rigged/, the kit material on every slot (marked for
skeletal meshes, so it renders on them).
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BUILD = os.path.join(os.environ.get("SOV_ART_BUILD") or os.path.join(REPO, "art", "build"), "Rigged")
DEST = "/Game/Art/Rigged"
MATERIAL = "/Game/Art/M_SovKit"
ANIM_SCALE = 100.0  # the FBX takes carry bone translations in metres while the skin and skeleton are in centimetres

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary


def main():
    unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX false")
    mat = lib.load_asset(MATERIAL)
    if not mat.get_editor_property("used_with_skeletal_mesh"):
        mat.set_editor_property("used_with_skeletal_mesh", True)
        unreal.MaterialEditingLibrary.recompile_material(mat)
        lib.save_asset(MATERIAL)
    tasks = []
    for path in sorted(glob.glob(os.path.join(BUILD, "SK_*.fbx"))):
        # A fresh import: importing over the old assets keeps the old animations.
        name = os.path.splitext(os.path.basename(path))[0]
        for suffix in ("_Anim_Armature_Walk", "_Anim_Armature_Idle", "", "_Skeleton"):  # what uses the skeleton first
            if lib.does_asset_exist("%s/%s%s" % (DEST, name, suffix)):
                lib.delete_asset("%s/%s%s" % (DEST, name, suffix))
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("import_animations", True)
        options.set_editor_property("create_physics_asset", False)
        data = options.get_editor_property("skeletal_mesh_import_data")
        data.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
        data.set_editor_property("import_morph_targets", False)
        anim = options.get_editor_property("anim_sequence_import_data")
        anim.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        anim.set_editor_property("import_uniform_scale", ANIM_SCALE)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", path)
        task.set_editor_property("destination_path", DEST)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        task.set_editor_property("options", options)
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    for asset in lib.list_assets(DEST, recursive=False):
        obj = lib.load_asset(asset)
        if isinstance(obj, unreal.SkeletalMesh):
            mats = obj.get_editor_property("materials")
            for m in mats:
                m.set_editor_property("material_interface", mat)
            obj.set_editor_property("materials", mats)
        lib.save_asset(asset)
        unreal.log("SOVART rigged asset " + asset)


main()
