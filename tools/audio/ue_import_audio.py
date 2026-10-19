"""Imports the synthesised sounds into the Unreal project (run inside the editor):

  UnrealEditor-Cmd unreal/Sovereign.uproject -run=pythonscript -script="tools/audio/ue_import_audio.py"

Reads art/build/audio/*.wav (or $SOV_AUDIO_BUILD) and imports each as a sound wave under /Game/Audio/.
Ambience (AMB_) and music (MUS_) loop.
"""
import glob
import os

import unreal

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BUILD = os.environ.get("SOV_AUDIO_BUILD") or os.path.join(REPO, "art", "build", "audio")

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary

tasks = []
for wav in sorted(glob.glob(os.path.join(BUILD, "*.wav"))):
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", wav)
    task.set_editor_property("destination_path", "/Game/Audio")
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    tasks.append(task)
tools.import_asset_tasks(tasks)
for wav in sorted(glob.glob(os.path.join(BUILD, "*.wav"))):
    name = os.path.splitext(os.path.basename(wav))[0]
    path = "/Game/Audio/" + name
    sound = lib.load_asset(path)
    if not sound:
        print("SOVART missing after import: " + path)
        continue
    sound.set_editor_property("looping", name.startswith(("AMB_", "MUS_")))
    lib.save_asset(path)
    print("SOVART imported " + path)
