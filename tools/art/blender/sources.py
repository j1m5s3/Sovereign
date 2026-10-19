"""Photo sources for the detail sheet's building and paving patterns: CC0 scans from Poly Haven
(https://polyhaven.com, licence CC0 1.0). tools/art/fetch_textures.py downloads each one's colour and
OpenGL normal maps (2k JPG) to <build>/Sources/<id>/; textures.py turns them into greyscale detail and
relief, and falls back to its painted pattern when a download is missing.

Per pattern: the asset id, the metres one repeat spans (kitlib repeats the pattern over that many metres:
the scan's real size, so stones and tiles keep theirs; thatch is drawn twice its size so the straw reads), how strongly its light and dark show (`contrast`, 1 = as
photographed), how deep its relief reads (`relief`, 1 = as scanned), and who made it (for the credits).
"""

SOURCES = {
    "masonry": {"id": "sandstone_blocks_08", "size": 3.0, "contrast": 2.6, "relief": 2.2, "by": "Rob Tuytel"},
    "brick": {"id": "red_brick", "size": 1.4, "contrast": 1.5, "relief": 1.0, "by": "Rob Tuytel"},
    "plaster": {"id": "white_plaster_rough_01", "size": 1.0, "contrast": 1.4, "relief": 0.8, "by": "Rob Tuytel"},
    "rooftile": {"id": "clay_roof_tiles_02", "size": 2.5, "contrast": 1.0, "relief": 1.0, "by": "Amal Kumar"},
    "thatch": {"id": "thatch_roof_angled", "size": 1.1, "contrast": 1.0, "relief": 1.0, "by": "Rob Tuytel, Dimitrios Savva"},
    "adobe": {"id": "clay_plaster", "size": 2.0, "contrast": 1.8, "relief": 1.0, "by": "Amal Kumar"},
    # The street scene's ground repeats every tile over 1.6 m, so the paving is drawn to that size.
    "flagstones": {"id": "cobblestone_floor_01", "size": 1.6, "contrast": 1.0, "relief": 1.0, "by": "Rob Tuytel"},
    "cobbles": {"id": "cobblestone_floor_08", "size": 1.6, "contrast": 1.0, "relief": 1.0, "by": "Rob Tuytel"},
}

# The files fetched per asset: Poly Haven's map name and the local file it is saved as.
MAPS = {"Diffuse": "diff.jpg", "nor_gl": "nor_gl.jpg"}
RESOLUTION = "2k"
