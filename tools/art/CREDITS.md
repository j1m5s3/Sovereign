# Art credits

The detail sheets' building and paving surfaces come from photo scans on [Poly Haven](https://polyhaven.com),
released under [CC0 1.0](https://creativecommons.org/publicdomain/zero/1.0/) (no attribution required; given
here with thanks). The build downloads them (`fetch_textures.py`) and keeps only their light, shade and
relief (`blender/textures.py`); the list is `blender/sources.py`.

| Pattern | Scan | Author |
|---|---|---|
| masonry | [Sandstone Blocks 08](https://polyhaven.com/a/sandstone_blocks_08) | Rob Tuytel |
| brick | [Red Brick](https://polyhaven.com/a/red_brick) | Rob Tuytel |
| plaster | [White Plaster Rough 01](https://polyhaven.com/a/white_plaster_rough_01) | Rob Tuytel |
| rooftile | [Clay Roof Tiles 02](https://polyhaven.com/a/clay_roof_tiles_02) | Amal Kumar |
| thatch | [Thatch Roof Angled](https://polyhaven.com/a/thatch_roof_angled) | Rob Tuytel, Dimitrios Savva |
| adobe | [Clay Plaster](https://polyhaven.com/a/clay_plaster) | Amal Kumar |
| flagstones | [Cobblestone Floor 01](https://polyhaven.com/a/cobblestone_floor_01) | Rob Tuytel |
| cobbles | [Cobblestone Floor 08](https://polyhaven.com/a/cobblestone_floor_08) | Rob Tuytel |

## Motion capture

The rigged people's WalkSlow, Walk, Run and Idle are retargeted (`blender/mocap.py`) from the
[CMU Graphics Lab Motion Capture Database](http://mocap.cs.cmu.edu), in Bruce Hahne's BVH conversion: subject 16
trial 33 (slow walk), subject 39 trial 01 (walk), subject 35 trial 18 (run/jog) and subject 82 trial 08 (standing). The trimmed clips and what was
kept are in `mocap/` (its README). The data used in this project was obtained from mocap.cs.cmu.edu. The database
was created with funding from NSF EIA-0196217. CMU allows its use in commercial products but not reselling the data
itself, even converted.
