"""Builds every kit piece and exports it as FBX (run by tools/art/build_art.py):

  blender -b --python tools/art/blender/generate.py -- <out_dir> [kit ...]

Each kit module lists its piece functions in PIECES; a piece goes to <out_dir>/<Kit>/<Name>.fbx.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import kit_classical  # noqa: E402
import kit_nature  # noqa: E402
import kitlib  # noqa: E402

KITS = {"Nature": kit_nature, "Classical": kit_classical}
try:
    import kit_figures  # noqa: E402

    KITS["Figures"] = kit_figures
except ImportError:
    pass


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    out = os.path.abspath(args[0] if args else "art/build")
    wanted = args[1:] or list(KITS)
    count = 0
    for kit in wanted:
        os.makedirs(os.path.join(out, kit), exist_ok=True)
        for make in KITS[kit].PIECES:
            kitlib.reset_scene()
            piece = make()
            obj = piece.build()
            kitlib.export_fbx(obj, os.path.join(out, kit, piece.name + ".fbx"))
            count += 1
            print("piece %s/%s: %d faces, %.1f m tall" % (kit, piece.name, len(obj.data.polygons), piece.height))
    print("exported %d pieces to %s" % (count, out))


if __name__ == "__main__":
    main()
