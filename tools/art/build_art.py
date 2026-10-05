#!/usr/bin/env python3
"""Regenerates every art asset from the scripts (see tools/art/README.md).

  python tools/art/build_art.py            # all kits
  python tools/art/build_art.py Classical  # some kits

1. Blender (headless) builds each kit piece and exports FBX to art/build/<Kit>/.
2. The Unreal editor (commandlet) imports them into unreal/Content/Art/<Kit>/ with the
   kit material. The .uasset files that come out are committed.
Set BLENDER / UNREAL_EDITOR_CMD to override the default install paths.
"""
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BLENDER = os.environ.get("BLENDER", r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe")
EDITOR = os.environ.get("UNREAL_EDITOR_CMD", r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe")
BUILD = os.path.join(REPO, "art", "build")


def run(cmd, env=None):
    print("+", " ".join('"%s"' % c if " " in c else c for c in cmd))
    result = subprocess.run(cmd, env=env, capture_output=True, text=True, errors="replace")
    for line in result.stdout.splitlines():
        if any(k in line for k in ("piece ", "exported ", "SOVART", "Error", "Traceback")):
            print("  " + line.strip())
    if result.returncode != 0:
        print(result.stdout[-4000:], result.stderr[-4000:])
        sys.exit("failed: %s" % cmd[0])


def main():
    kits = sys.argv[1:]
    os.makedirs(BUILD, exist_ok=True)
    run([BLENDER, "-b", "--factory-startup", "--python", os.path.join(REPO, "tools", "art", "blender", "generate.py"), "--", BUILD] + kits)
    env = dict(os.environ, SOV_ART_BUILD=BUILD)
    run([EDITOR, os.path.join(REPO, "unreal", "Sovereign.uproject"), "-run=pythonscript",
         "-script=" + os.path.join(REPO, "tools", "art", "ue_import.py"), "-unattended", "-nosplash", "-nopause", "-stdout"], env=env)
    print("done: assets in unreal/Content/Art")


if __name__ == "__main__":
    main()
