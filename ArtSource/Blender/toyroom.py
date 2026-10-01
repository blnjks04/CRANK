"""Zone F: the toy room behind the kitchen's north wall (giant cymbal monkey arena).

Blender meters = UE uu / 100. World-space pivots (placed at the UE origin) unless noted.
  kitchen north wall  Y[80,81], mouse hole X[4.5,7.5] Z[0,3.2] (arched trim on both faces)
  toy room            X[0,60] Y[81,141] height 32, window in the north wall
"""
import bpy, math, os, sys
from mathutils import Vector
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Blender")
import importlib, crank_kit
importlib.reload(crank_kit)
from crank_kit import *

WHITE = "#f4f1ea"
MINT = "#8fb8a8"
WOOD = "#9a6a43"
WOOD_D = "#6e4529"
HOLE = (4.5, 7.5, 3.2)          # x0, x1, top
ROOM = (0.0, 60.0, 81.0, 141.0)  # x0, x1, y0, y1
H = 32.0


def box_minmax(b, mn, mx, color, mat="Matte", bevel=0.0, rot=None, segments=2):
    c = [(mn[i] + mx[i]) / 2 for i in range(3)]
    s = [abs(mx[i] - mn[i]) for i in range(3)]
    if min(s) <= 1e-4:
        return None
    return b.box(c, s, color, mat, bevel=bevel, rot=rot, segments=segments)


def cut_x(b, x0, x1, y0, y1, z0, z1, color, mat="Matte", bevel=0.0, hole=HOLE):
    """Box spanning X[x0,x1] that leaves the mouse hole open."""
    h0, h1, top = hole
    if x1 <= h0 or x0 >= h1 or z0 >= top:
        box_minmax(b, (x0, y0, z0), (x1, y1, z1), color, mat, bevel=bevel, segments=1)
        return
    if x0 < h0:
        box_minmax(b, (x0, y0, z0), (h0, y1, z1), color, mat, bevel=bevel, segments=1)
    if x1 > h1:
        box_minmax(b, (h1, y0, z0), (x1, y1, z1), color, mat, bevel=bevel, segments=1)
    if z1 > top:
        box_minmax(b, (max(x0, h0), y0, top), (min(x1, h1), y1, z1), color, mat, bevel=bevel, segments=1)


def hole_trim(b, y_face, outward):
    """Arched wooden trim around the mouse hole on one face (outward = direction away from the wall)."""
    h0, h1, top = HOLE
    t = 0.35
    y0, y1 = sorted((y_face, y_face + outward * 0.3))
    box_minmax(b, (h0 - t, y0, 0), (h0, y1, top), WOOD_D, "Wood", bevel=0.05)
    box_minmax(b, (h1, y0, 0), (h1 + t, y1, top), WOOD_D, "Wood", bevel=0.05)
    # rounded top: corner fillers + an arch of small blocks
    r = (h1 - h0) / 2
    cx = (h0 + h1) / 2
    cz = top - r * 0.55
    steps = 9
    for i in range(steps):
        a0 = math.pi * i / steps
        a1 = math.pi * (i + 1) / steps
        am = (a0 + a1) / 2
        px, pz = cx + math.cos(am) * r, cz + math.sin(am) * r * 0.55 + r * 0.25
        b.box((px, (y0 + y1) / 2, max(pz, top - 0.6)), (0.5, y1 - y0, 0.45), WOOD_D, "Wood", rot=(0, -math.degrees(am) + 90, 0), segments=1)
    for side in (-1, 1):
        # fill the upper corners so the opening reads as an arch
        x_in = cx + side * r
        for k in range(4):
            z = top - 0.15 - k * 0.22
            w = 0.25 + 0.22 * (3 - k) ** 1.3 / 2.0
            xa, xb = sorted((x_in, x_in - side * w))
            box_minmax(b, (xa, y0, z - 0.11), (xb, y1, z + 0.11), "#d9c9a8")
    # a little brass plate over the hole
    box_minmax(b, (cx - 0.6, y0 - outward * 0.02 if outward < 0 else y0, top + 0.5), (cx + 0.6, y1 + 0.02, top + 0.9), "#d6a548", "Brass", bevel=0.03)


def kitchen_north_wall():
    """SM_Wall_North again (same look as arch.walls), now with the mouse hole cut through."""
    b = Builder("SM_Wall_North")
    inner, outward = 80.0, 1
    cut_x(b, -1, 101, inner, inner + outward * 1.0, 0, H, "#d9c9a8")
    a = -1
    while a < 101 - 0.01:
        a1 = min(a + 3.0, 101)
        cut_x(b, a + 0.08, a1 - 0.08, inner - 0.12, inner, 1.2, 8.0, MINT, "Gloss", bevel=0.04)
        a = a1
    cut_x(b, -1, 101, inner - 0.25, inner, 7.9, 8.4, WHITE, "Gloss", bevel=0.05)
    cut_x(b, -1, 101, inner - 0.2, inner, 0, 1.2, WHITE, "Gloss", bevel=0.04)
    a = -1; k = 0
    while a < 101 - 0.01:
        a1 = min(a + 1.6, 101)
        cut_x(b, a, a1, inner - 0.04, inner, 8.4, H, "#f3e5c8" if k % 2 == 0 else "#e6cfa6")
        a = a1; k += 1
    cut_x(b, -1, 101, inner - 0.35, inner, H - 0.6, H, WHITE, "Gloss", bevel=0.08)
    hole_trim(b, inner, -1)
    return b.finish()


def toy_wallpaper(b, axis, a0, a1, face, inward, hole=None):
    """Blue nursery wallpaper with a white wainscot. axis = run axis (0 = X, 1 = Y); face = inner face coord;
    inward = direction into the room along the normal axis."""
    nax = 1 - axis

    def mk(r0, r1, n0, n1, z0, z1, color, mat="Matte", bevel=0.0):
        if hole and axis == 0:
            cut_x(b, r0, r1, min(n0, n1), max(n0, n1), z0, z1, color, mat, bevel=bevel, hole=hole)
            return
        mn = [0, 0, z0]; mx = [0, 0, z1]
        mn[axis], mx[axis] = r0, r1
        mn[nax], mx[nax] = min(n0, n1), max(n0, n1)
        box_minmax(b, mn, mx, color, mat, bevel=bevel, segments=1)

    a = a0; k = 0
    while a < a1 - 0.01:
        e = min(a + 2.0, a1)
        mk(a, e, face, face + inward * 0.04, 7.4, H, "#a9cbe8" if k % 2 == 0 else "#95bbdf")
        a = e; k += 1
    a = a0
    while a < a1 - 0.01:
        e = min(a + 3.0, a1)
        mk(a + 0.08, e - 0.08, face, face + inward * 0.12, 1.0, 7.2, "#fbf7ee", "Gloss", bevel=0.04)
        a = e
    mk(a0, a1, face, face + inward * 0.25, 7.1, 7.6, "#e9b949", "Gloss", bevel=0.05)    # yellow chair rail
    mk(a0, a1, face, face + inward * 0.2, 0, 1.0, WHITE, "Gloss", bevel=0.04)          # baseboard
    mk(a0, a1, face, face + inward * 0.35, H - 0.6, H, WHITE, "Gloss", bevel=0.08)     # crown
    # little yellow stars on the paper
    import random
    r = random.Random(int(a0 * 7 + face))
    pts = []
    for i in range(10):
        ang = math.pi * 2 * i / 10 - math.pi / 2
        rad = 0.45 if i % 2 == 0 else 0.2
        pts.append((math.cos(ang) * rad, math.sin(ang) * rad))
    count = int((a1 - a0) / 3.2)
    for i in range(count):
        along = a0 + 1.6 + i * 3.2 + r.uniform(-0.6, 0.6)
        z = r.uniform(10.0, 29.0)
        loc = [0, 0, z]
        loc[axis] = along
        loc[nax] = face + inward * 0.06
        rot = (90, 0, 0) if nax == 1 else (90, 0, 90)
        b.prism(pts, 0.06, "#ffd85a", "Emissive" if i % 5 == 0 else "Gloss", loc=tuple(loc), rot=rot)


def toy_room_shell():
    x0, x1, y0, y1 = ROOM
    objs = []
    # floor: warm planks
    b = Builder("SM_Toy_Floor")
    box_minmax(b, (x0 - 1, y0 - 0.2, -0.4), (x1 + 1, y1 + 1, -0.06), "#4a3428")
    y = y0
    k = 0
    while y < y1 - 0.01:
        e = min(y + 1.2, y1)
        x = x0 - (k % 3) * 2.0
        while x < x1 - 0.01:
            xe = min(x + 9.0, x1)
            xs = max(x, x0)
            if xe - xs > 0.05:
                col = ("#b9824f", "#a8723f", "#c08a55")[(k + int(x)) % 3]
                box_minmax(b, (xs + 0.03, y + 0.03, -0.1), (xe - 0.03, e - 0.03, 0.0), col, "Wood", bevel=0.02, segments=1)
            x = xe
        y = e; k += 1
    objs.append(b.finish())

    # walls: west, east, north (with a window) and the room side of the kitchen wall
    b = Builder("SM_Toy_Walls")
    box_minmax(b, (x0 - 1, y0, 0), (x0, y1 + 1, H), "#d9c9a8")
    toy_wallpaper(b, 1, y0, y1, x0, 1)
    box_minmax(b, (x1, y0, 0), (x1 + 1, y1 + 1, H), "#d9c9a8")
    toy_wallpaper(b, 1, y0, y1, x1, -1)
    wx0, wx1, wz0, wz1 = 22.0, 38.0, 11.0, 25.0
    for (a, c, z0, z1) in [(x0 - 1, wx0, 0, H), (wx1, x1 + 1, 0, H), (wx0, wx1, 0, wz0), (wx0, wx1, wz1, H)]:
        box_minmax(b, (a, y1, z0), (c, y1 + 1, z1), "#d9c9a8")
    # paper on the north face, skipping the window
    for (a, c) in [(x0, wx0), (wx1, x1)]:
        toy_wallpaper(b, 0, a, c, y1, -1)
    box_minmax(b, (wx0, y1 - 0.04, 7.4), (wx1, y1, wz0), "#a9cbe8")
    box_minmax(b, (wx0, y1 - 0.04, wz1), (wx1, y1, H), "#95bbdf")
    box_minmax(b, (wx0, y1 - 0.12, 1.0), (wx1, y1, 7.2), "#fbf7ee", "Gloss")
    box_minmax(b, (wx0, y1 - 0.25, 7.1), (wx1, y1, 7.6), "#e9b949", "Gloss")
    box_minmax(b, (wx0, y1 - 0.2, 0), (wx1, y1, 1.0), WHITE, "Gloss")
    fr = 0.6
    box_minmax(b, (wx0 - fr, y1 - 0.5, wz0 - fr), (wx1 + fr, y1 + 0.2, wz0), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx0 - fr, y1 - 0.5, wz1), (wx1 + fr, y1 + 0.2, wz1 + fr), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx0 - fr, y1 - 0.5, wz0), (wx0, y1 + 0.2, wz1), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx1, y1 - 0.5, wz0), (wx1 + fr, y1 + 0.2, wz1), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, ((wx0 + wx1) / 2 - 0.22, y1 - 0.3, wz0), ((wx0 + wx1) / 2 + 0.22, y1 + 0.1, wz1), WHITE, "Gloss", bevel=0.05)
    box_minmax(b, (wx0, y1 - 0.3, (wz0 + wz1) / 2 - 0.22), (wx1, y1 + 0.1, (wz0 + wz1) / 2 + 0.22), WHITE, "Gloss", bevel=0.05)
    box_minmax(b, (wx0 - 1.0, y1 - 1.2, wz0 - 0.9), (wx1 + 1.0, y1 + 0.2, wz0 - 0.5), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx0, y1 + 0.5, wz0), (wx1, y1 + 0.55, wz1), "#9cc4ff", "Glass")
    box_minmax(b, (wx0 - 8, y1 + 11.5, wz0 - 8), (wx1 + 8, y1 + 12, wz1 + 8), "#0b1530", "Emissive")
    b.cylinder((26.0, y1 + 11.3, 21.0), 2.4, 0.2, "#fff4d6", "Emissive", verts=32, rot=(90, 0, 0))
    for (sx, sz) in [(24, 14), (31, 23), (35, 16), (29, 12), (36, 22)]:
        b.sphere((sx, y1 + 11.2, sz), 0.14, "#ffffff", "Emissive", subdiv=1)
    # curtains (blue)
    for cx in (wx0 - 2.0, wx1 + 2.0):
        for i in range(5):
            b.cylinder((cx - 0.8 + i * 0.4, y1 - 0.8, (wz0 + wz1) / 2 + 1), 0.35, wz1 - wz0 + 4, "#3d5fa8" if i % 2 == 0 else "#324f8d", "Cloth", verts=8)
    box_minmax(b, (wx0 - 4, y1 - 1.0, wz1 + 2), (wx1 + 4, y1 - 0.5, wz1 + 2.4), WOOD_D, "Wood", bevel=0.05)
    # room side of the kitchen wall (with the hole) + its trim
    toy_wallpaper(b, 0, x0, x1, 81.0, 1, hole=HOLE)
    hole_trim(b, 81.0, 1)
    objs.append(b.finish())

    b = Builder("SM_Toy_Ceiling")
    box_minmax(b, (x0 - 1, y0, H), (x1 + 1, y1 + 1, H + 1), "#eef3f8")
    for x in range(8, 60, 13):
        box_minmax(b, (x - 0.5, y0, H - 1.0), (x + 0.5, y1, H), "#e1e8f0", "Matte", bevel=0.06, segments=1)
    objs.append(b.finish())
    return objs


# ------------------------------------------------------------------------------------------
# props (pivot bottom center unless noted)

def rug():
    b = Builder("SM_Toy_Rug")
    cols = ["#d9534f", "#f0ad4e", "#5bc0de", "#5cb85c", "#9b59b6", "#f7f1e3"]
    rings = [(22.0, 0), (19.0, 1), (16.0, 2), (13.0, 3), (10.0, 4), (7.0, 0), (4.0, 5)]
    for i, (rad, c) in enumerate(rings):
        b.cylinder((0, 0, 0.02 + i * 0.004), rad, 0.04, cols[c], "Cloth", verts=64, smooth=False)
    return b.finish()


def letter_block(name, size, colors, letter_color):
    """Alphabet block: colored faces with a raised square 'letter tile'."""
    b = Builder(name)
    s = size
    b.box((0, 0, s / 2), (s, s, s), colors[0], "Wood", bevel=s * 0.06)
    inset = s * 0.62
    d = s * 0.03
    for ax, sign, col in [(0, 1, colors[1]), (0, -1, colors[2]), (1, 1, colors[3]), (1, -1, colors[1]), (2, 1, colors[2])]:
        loc = [0, 0, s / 2]
        size3 = [inset, inset, inset]
        loc[ax] += sign * (s / 2 + d / 2)
        size3[ax] = d
        b.box(tuple(loc), tuple(size3), col, "Gloss", bevel=d * 0.4, segments=1)
        # a chunky glyph bar on the tile
        loc2 = list(loc)
        loc2[ax] += sign * d
        g = [inset * 0.18, inset * 0.18, inset * 0.18]
        g[ax] = d
        bar = list(g)
        other = [i for i in range(3) if i != ax]
        bar[other[0]] = inset * 0.6
        b.box(tuple(loc2), tuple(bar), letter_color, "Gloss", segments=1)
        bar2 = list(g)
        bar2[other[1]] = inset * 0.6
        b.box(tuple(loc2), tuple(bar2), letter_color, "Gloss", segments=1)
    return b.finish()


def toy_chest():
    b = Builder("SM_Toy_Chest")
    box_minmax(b, (-4.5, -2.6, 0), (4.5, 2.6, 4.6), "#c0392b", "Wood", bevel=0.15)
    box_minmax(b, (-4.7, -2.8, 4.6), (4.7, 2.8, 5.3), "#e67e22", "Wood", bevel=0.15)
    for x in (-3.0, 0.0, 3.0):
        box_minmax(b, (x - 0.15, -2.75, 0.3), (x + 0.15, 2.75, 4.5), "#f1c40f", "Gloss", segments=1)
    box_minmax(b, (-0.8, -2.85, 3.4), (0.8, -2.6, 4.3), "#d6a548", "Brass", bevel=0.05)
    # teddy ear + toys peeking out of the lid
    b.sphere((2.6, 0.8, 5.6), 0.9, "#a0522d", "Cloth", subdiv=2)
    b.cylinder((-2.5, -0.5, 6.0), 0.18, 2.4, "#3498db", "Gloss", verts=8, rot=(0, 25, 10))
    return b.finish()


def toy_ball():
    b = Builder("SM_Toy_Ball")
    b.sphere((0, 0, 2.4), 2.4, "#e74c3c", "Gloss", subdiv=3)
    b.torus((0, 0, 2.4), 2.42, 0.25, "#f7f1e3", "Gloss", seg=32, ring=6)
    b.torus((0, 0, 2.4), 2.42, 0.25, "#2e86de", "Gloss", seg=32, ring=6, rot=(90, 0, 0))
    return b.finish()


def picture_books():
    b = Builder("SM_Toy_Books")
    cols = ["#16a085", "#8e44ad", "#d35400", "#2980b9"]
    z = 0.0
    for i, (w, d, h) in enumerate([(5.0, 3.6, 0.6), (4.6, 3.4, 0.5), (4.8, 3.2, 0.7), (4.2, 3.0, 0.45)]):
        rot = (0, 0, (i * 9) % 23 - 11)
        b.box((0, 0, z + h / 2), (w, d, h), cols[i], "Matte", bevel=0.05, rot=rot)
        b.box((0.05 * (i % 2), 0, z + h / 2), (w - 0.2, d + 0.02, h * 0.8), "#fbf7ee", "Matte", rot=rot)
        z += h
    return b.finish()


ALL_PROPS = [rug, toy_chest, toy_ball, picture_books]


def build_all(export=True):
    objs = [kitchen_north_wall()]
    objs += toy_room_shell()
    props = [fn() for fn in ALL_PROPS]
    props.append(letter_block("SM_Toy_BlockSmall", 1.0, ["#f6e3b4", "#e74c3c", "#27ae60", "#2980b9"], "#fdfdfd"))
    props.append(letter_block("SM_Toy_BlockBig", 2.0, ["#f6e3b4", "#f39c12", "#8e44ad", "#16a085"], "#fdfdfd"))
    if export:
        export_static(objs[0], "SM_Wall_North.fbx", sub="Arch")
        for o in objs[1:]:
            export_static(o, o.name + ".fbx", sub="Arch")
        for o in props:
            export_static(o, o.name + ".fbx", sub="Props")
    return objs + props
