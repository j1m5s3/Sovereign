# Motion capture

The clips the rigged people's WalkSlow, Walk, Run and Idle are retargeted from (`../blender/mocap.py`, run by
`../blender/rig_figures.py`). They come from the [CMU Graphics Lab Motion Capture Database](http://mocap.cs.cmu.edu),
in Bruce Hahne's 2010 MotionBuilder-friendly BVH conversion (cgspeed.com), as mirrored at
github.com/una-dinosauria/cmu-mocap (`data/<subject>/<subject>_<trial>.bvh`).

Each file here is its source trial cut down to the frames around one loop: the hierarchy and every kept frame are as
CMU's conversion has them, nothing re-posed or smoothed. The source is 120 fps.

| File | Action | CMU subject / trial (CMU's description) | Source frames kept | Loop (`mocap.CLIPS`, frames of this file) | Authored speed |
|---|---|---|---|---|---|
| `walkslow_16_33.bvh` | WalkSlow | 16 / 33 ("slow walk, stop": the walk before the stop) | 16-177 of 286, every frame | 12-149: one stride in 1.14 s, 27 frames | 100.4 cm/s |
| `walk_39_01.bvh` | Walk | 39 / 01 ("walk") | 218-366 of 379, every frame | 12-136: one stride (two steps) in 1.03 s, 25 frames at 24 fps | 152.5 cm/s |
| `run_35_18.bvh` | Run | 35 / 18 ("run/jog") | 54-171 of 176, every frame | 12-105: one stride in 0.78 s, 19 frames | 295.2 cm/s |
| `idle_82_08.bvh` | Idle | 82 / 08 ("stand still; casual walk forward": the standing part) | 24-856 of 1700, every 4th frame (30 fps) | 0-208: 6.9 s, 166 frames | 0 |

The speeds are the rig's, a figure about 1.75 m tall (legs 84 cm hip to ankle): the subject's own speed scaled by
our leg over theirs. The game plays a loop at its speed over this one, so planted feet hold still.

Chosen by measuring and rendering candidates: a natural, upright walk at an ordinary pace with an even arm swing
(39_01 over 02_01, 07_01 and 81_02, whose subjects look down or stride short), a steady jog with a clean loop, a
standing idle with weight shifts but no gestures, and a relaxed stroll for the slow end of the street's speeds (16_33
over 08_04, 16_34, 07_05, 37_01 and 120_19: 08_04 swings its arms like a march and its planted feet slid most, 16_34
is faster at 113 cm/s, 07_05 and 37_01 turn and close their loops badly, 120_19 ambles at 57 cm/s). To change one,
cut the new trial's frames the same way (header plus the frames, the frame time scaled if frames are skipped), set its
row in `mocap.CLIPS` (first and last frame of one loop: for a gait, the same foot at the same phase), run
`../blender/check_rigs.py` and update this table and the speeds in `../README.md` (and the game's).

## Credit and terms

The data used in this project was obtained from mocap.cs.cmu.edu. The database was created with funding from NSF
EIA-0196217.

CMU's terms: the data is free to use, including in commercially sold products, but may not be resold directly, even
in converted form. So these trimmed files ship in the repo as the pipeline's input, and the game ships only the
animations retargeted from them. The credit is repeated in `../CREDITS.md`.
