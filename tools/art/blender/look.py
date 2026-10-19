"""The game material's look in Blender (M_SovKit): vertex colour x tint x 2 x the detail cell (textures.py) the
vertex alpha picks, with the normal sheet's relief. Shared by the review renders (portrait.py, backdrop.py)."""
import bpy

import textures

G = textures.GRID
INNER = textures.PERIOD / float(textures.CELL)  # the share of a cell one period of its pattern fills
EDGE = textures.PAD / float(textures.CELL)


def material(name, tint, sheets):
    """As M_SovKit: vertex colour x tint x 2 x the detail sheet at the alpha's cell; `sheets` is
    (detail, normal) as textures.save returns them."""
    sheet, normal_sheet = sheets
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes, links = m.node_tree.nodes, m.node_tree.links
    bsdf = nodes.get("Principled BSDF")
    L = links.new

    def math_node(op, a, b=None):
        n = nodes.new("ShaderNodeMath")
        n.operation = op
        for i, v in enumerate((a, b)):
            if v is None:
                continue
            if isinstance(v, (int, float)):
                n.inputs[i].default_value = v
            else:
                L(v, n.inputs[i])
        return n.outputs[0]

    col = nodes.new("ShaderNodeVertexColor")
    col.layer_name = "Col"
    idx = math_node("ROUND", math_node("MULTIPLY", col.outputs["Alpha"], float(G * G - 1)))
    column = math_node("FLOORED_MODULO", idx, float(G))
    row = math_node("FLOOR", math_node("MULTIPLY", idx, 1.0 / G))
    uvmap = nodes.new("ShaderNodeUVMap")
    uvmap.uv_map = "UVMap"
    sep = nodes.new("ShaderNodeSeparateXYZ")
    L(uvmap.outputs["UV"], sep.inputs[0])
    fu = math_node("ADD", math_node("MULTIPLY", math_node("FRACT", sep.outputs["X"]), INNER), EDGE)
    fv = math_node("ADD", math_node("MULTIPLY", math_node("FRACT", sep.outputs["Y"]), INNER), EDGE)
    # Cell row 0 sits at the top of the sheet; Blender's V runs up from the bottom.
    au = math_node("MULTIPLY", math_node("ADD", column, fu), 1.0 / G)
    av = math_node("MULTIPLY", math_node("ADD", math_node("SUBTRACT", float(G - 1), row), fv), 1.0 / G)
    comb = nodes.new("ShaderNodeCombineXYZ")
    L(au, comb.inputs["X"])
    L(av, comb.inputs["Y"])
    img = nodes.new("ShaderNodeTexImage")
    img.image = sheet
    img.interpolation = "Linear"
    L(comb.outputs[0], img.inputs["Vector"])
    detail = math_node("MULTIPLY", img.outputs["Color"], 2.0)
    mix = nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    mix.inputs["Factor"].default_value = 1.0
    L(col.outputs["Color"], mix.inputs["A"])
    mix.inputs["B"].default_value = (tint[0], tint[1], tint[2], 1.0)
    mix2 = nodes.new("ShaderNodeMix")
    mix2.data_type = "RGBA"
    mix2.blend_type = "MULTIPLY"
    mix2.inputs["Factor"].default_value = 1.0
    L(mix.outputs["Result"], mix2.inputs["A"])
    L(detail, mix2.inputs["B"])
    L(mix2.outputs["Result"], bsdf.inputs["Base Color"])
    nimg = nodes.new("ShaderNodeTexImage")
    nimg.image = normal_sheet
    nimg.interpolation = "Linear"
    L(comb.outputs[0], nimg.inputs["Vector"])
    nmap = nodes.new("ShaderNodeNormalMap")
    nmap.uv_map = "UVMap"
    L(nimg.outputs["Color"], nmap.inputs["Color"])
    L(nmap.outputs["Normal"], bsdf.inputs["Normal"])
    bsdf.inputs["Roughness"].default_value = 0.75
    return m
