#!/usr/bin/env python3
"""Downloads the CC0 photo sources of the detail sheet (tools/art/blender/sources.py) from Poly Haven to
art/build/Sources/<id>/ (or $SOV_ART_BUILD/Sources), skipping files already there. Run by build_art.py;
textures.py falls back to its painted patterns for anything missing, so the build works offline.

  python tools/art/fetch_textures.py
"""
import json
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "blender"))

import sources  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
BUILD = os.environ.get("SOV_ART_BUILD") or os.path.join(REPO, "art", "build")
AGENT = {"User-Agent": "Sovereign-art-build"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=AGENT), timeout=120) as r:
        return r.read()


def main():
    fetched = 0
    for pattern, src in sorted(sources.SOURCES.items()):
        folder = os.path.join(BUILD, "Sources", src["id"])
        wanted = {m: os.path.join(folder, f) for m, f in sources.MAPS.items()}
        if all(os.path.exists(p) for p in wanted.values()):
            continue
        os.makedirs(folder, exist_ok=True)
        try:
            files = json.loads(get("https://api.polyhaven.com/files/" + src["id"]))
            for m, path in wanted.items():
                if os.path.exists(path):
                    continue
                url = files[m][sources.RESOLUTION]["jpg"]["url"]
                data = get(url)
                with open(path + ".part", "wb") as f:
                    f.write(data)
                os.replace(path + ".part", path)
                fetched += 1
                print("fetched %s %s (%d KB)" % (pattern, os.path.basename(path), len(data) // 1024))
        except Exception as e:  # offline or moved: textures.py paints this pattern instead
            print("could not fetch %s (%s): %s" % (pattern, src["id"], e))
    print("photo sources: %d files fetched to %s" % (fetched, os.path.join(BUILD, "Sources")))


if __name__ == "__main__":
    main()
