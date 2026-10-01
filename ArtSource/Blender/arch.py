"""Kitchen architecture + furniture.  World-space pivots (placed at UE origin) unless noted.

Layout (Blender meters = UE uu / 100):
  room X[0,100] Y[0,80], walls 32 high
  nook (start)   X[0,16]  Y[34,80]     workbench X[1.5,12.5] Y[41,51] top 3
  A sink cabinet X[16,56] Y[63,80]     floor 1.0, interior Z[1,5.6], top 6.2
  B canyon       X[46,78] Y[48,63]
  C counter      X[78,100] Y[63,80]    drawers X[80,88] tops .95..4.95, top 6.0
  D fridge front X[56,100] Y[8,48]     fridge X[90,100] Y[8,28]
  E table        X[18,43] Y[5,30]      top 6.5 (slab 6.0-6.5)
"""
import bpy, math, os, sys
from mathutils import Vector
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Blender")
import importlib, crank_kit
importlib.reload(crank_kit)
from crank_kit import *

WOOD = "#9a6a43"
WOOD_D = "#6e4529"
WOOD_L = "#c2925f"
CREAM = "#f1e6cc"
WHITE = "#f4f1ea"
STEEL = "#b8bdc4"
MINT = "#8fb8a8"


def box_minmax(b, mn, mx, color, mat="Matte", bevel=0.0, rot=None, segments=2):
    c = [(mn[i] + mx[i]) / 2 for i in range(3)]
    s = [abs(mx[i] - mn[i]) for i in range(3)]
    return b.box(c, s, color, mat, bevel=bevel, rot=rot, segments=segments)


# ------------------------------------------------------------------------------------------
def floor():
    b = Builder("SM_Floor")
    box_minmax(b, (-1, -1, -0.4), (101, 81, -0.05), "#4a3a30", "Matte")
    t = 3.0
    nx, ny = 34, 27
    for i in range(nx):
        for j in range(ny):
            x0, y0 = i * t, j * t
            if x0 >= 100 or y0 >= 80:
                continue
            x1, y1 = min(x0 + t, 100), min(y0 + t, 80)
            col = "#efe3c8" if (i + j) % 2 == 0 else "#a9483a"
            box_minmax(b, (x0 + 0.04, y0 + 0.04, -0.12), (x1 - 0.04, y1 - 0.04, 0.0), col, "Gloss", bevel=0.03, segments=1)
    return b.finish()


def walls():
    objs = []
    H = 32.0
    # North + East + West: wainscot + striped wallpaper + baseboard + chair rail
    def wall_run(b, start, end, normal_axis, inner, outward):
        # start/end along the run axis, inner = coordinate of the inner face, outward = +1/-1 thickness dir
        ax = 0 if normal_axis == 1 else 1   # run axis
        def mk(a0, a1, n0, n1, z0, z1, col, mat="Matte", bevel=0.0):
            mn = [0, 0, z0]; mx = [0, 0, z1]
            mn[ax], mx[ax] = a0, a1
            mn[normal_axis], mx[normal_axis] = min(n0, n1), max(n0, n1)
            box_minmax(b, mn, mx, col, mat, bevel=bevel, segments=1)
        # structural wall
        mk(start, end, inner, inner + outward * 1.0, 0, H, "#d9c9a8")
        # wainscot panels
        n = inner - outward * 0.12
        a = start
        while a < end - 0.01:
            a1 = min(a + 3.0, end)
            mk(a + 0.08, a1 - 0.08, inner, n, 1.2, 8.0, MINT, "Gloss", bevel=0.04)
            a = a1
        mk(start, end, inner, inner - outward * 0.25, 7.9, 8.4, WHITE, "Gloss", bevel=0.05)    # chair rail
        mk(start, end, inner, inner - outward * 0.2, 0, 1.2, WHITE, "Gloss", bevel=0.04)       # baseboard
        # wallpaper stripes
        a = start
        k = 0
        while a < end - 0.01:
            a1 = min(a + 1.6, end)
            mk(a, a1, inner, inner - outward * 0.04, 8.4, H, "#f3e5c8" if k % 2 == 0 else "#e6cfa6")
            a = a1
            k += 1
        # crown molding
        mk(start, end, inner, inner - outward * 0.35, H - 0.6, H, WHITE, "Gloss", bevel=0.08)

    b = Builder("SM_Wall_North"); wall_run(b, -1, 101, 1, 80.0, 1); objs.append(b.finish())
    b = Builder("SM_Wall_East"); wall_run(b, -1, 81, 0, 100.0, 1); objs.append(b.finish())
    b = Builder("SM_Wall_West"); wall_run(b, -1, 81, 0, 0.0, -1); objs.append(b.finish())

    # South wall with a big window above the table
    b = Builder("SM_Wall_South")
    wx0, wx1, wz0, wz1 = 20.0, 44.0, 10.0, 27.0
    for (x0, x1, z0, z1) in [(-1, wx0, 0, 32), (wx1, 101, 0, 32), (wx0, wx1, 0, wz0), (wx0, wx1, wz1, 32)]:
        box_minmax(b, (x0, -1.0, z0), (x1, 0.0, z1), "#d9c9a8")
    # wainscot / stripes on the inner face (skip the window opening)
    a = -1.0; k = 0
    while a < 101:
        a1 = min(a + 1.6, 101)
        if a1 <= wx0 or a >= wx1:
            box_minmax(b, (a, 0.0, 8.4), (a1, 0.04, 32), "#f3e5c8" if k % 2 == 0 else "#e6cfa6")
        else:
            box_minmax(b, (a, 0.0, 8.4), (a1, 0.04, wz0), "#f3e5c8" if k % 2 == 0 else "#e6cfa6")
            box_minmax(b, (a, 0.0, wz1), (a1, 0.04, 32), "#f3e5c8" if k % 2 == 0 else "#e6cfa6")
        a = a1; k += 1
    a = -1.0
    while a < 101:
        a1 = min(a + 3.0, 101)
        box_minmax(b, (a + 0.08, 0.0, 1.2), (a1 - 0.08, 0.12, 8.0), MINT, "Gloss", bevel=0.04, segments=1)
        a = a1
    box_minmax(b, (-1, 0, 7.9), (101, 0.25, 8.4), WHITE, "Gloss", bevel=0.05, segments=1)
    box_minmax(b, (-1, 0, 0), (101, 0.2, 1.2), WHITE, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (-1, 0, 31.4), (101, 0.35, 32), WHITE, "Gloss", bevel=0.08, segments=1)
    # window frame + mullions + sill
    fr = 0.7
    box_minmax(b, (wx0 - fr, -0.2, wz0 - fr), (wx1 + fr, 0.5, wz0), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx0 - fr, -0.2, wz1), (wx1 + fr, 0.5, wz1 + fr), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx0 - fr, -0.2, wz0), (wx0, 0.5, wz1), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, (wx1, -0.2, wz0), (wx1 + fr, 0.5, wz1), WHITE, "Gloss", bevel=0.1)
    box_minmax(b, ((wx0 + wx1) / 2 - 0.25, -0.15, wz0), ((wx0 + wx1) / 2 + 0.25, 0.3, wz1), WHITE, "Gloss", bevel=0.06)
    box_minmax(b, (wx0, -0.15, (wz0 + wz1) / 2 - 0.25), (wx1, 0.3, (wz0 + wz1) / 2 + 0.25), WHITE, "Gloss", bevel=0.06)
    box_minmax(b, (wx0 - 1.2, -0.3, wz0 - 1.0), (wx1 + 1.2, 1.4, wz0 - 0.6), WHITE, "Gloss", bevel=0.1)  # sill
    # glass
    box_minmax(b, (wx0, -0.55, wz0), (wx1, -0.5, wz1), "#9cc4ff", "Glass")
    # night sky backdrop + moon (emissive) behind the window
    box_minmax(b, (wx0 - 8, -12, wz0 - 8), (wx1 + 8, -11.5, wz1 + 8), "#0b1530", "Emissive")
    b.cylinder((37.0, -11.3, 22.5), 3.0, 0.2, "#fff4d6", "Emissive", verts=32, rot=(90, 0, 0))
    for (sx, sz) in [(24, 24), (28, 20), (31, 26), (41, 15), (26, 14), (43, 25), (34, 13)]:
        b.sphere((sx, -11.2, sz), 0.15, "#ffffff", "Emissive", subdiv=1)
    # curtains
    for cx in (wx0 - 2.2, wx1 + 2.2):
        for i in range(6):
            b.cylinder((cx - 1.0 + i * 0.4, 0.8, (wz0 + wz1) / 2 + 1), 0.35, wz1 - wz0 + 4, "#b8434a" if i % 2 == 0 else "#a3363d", "Cloth", verts=8)
    box_minmax(b, (wx0 - 4, 0.5, wz1 + 2), (wx1 + 4, 1.0, wz1 + 2.4), WOOD_D, "Wood", bevel=0.05)
    objs.append(b.finish())

    # Ceiling with beams
    b = Builder("SM_Ceiling")
    box_minmax(b, (-1, -1, 32), (101, 81, 33), "#efe6d3")
    for x in range(10, 100, 15):
        box_minmax(b, (x - 0.6, 0, 30.8), (x + 0.6, 80, 32), WOOD_D, "Wood", bevel=0.08, segments=1)
    objs.append(b.finish())
    return objs


# ------------------------------------------------------------------------------------------
def bookcase():
    """Start nook south barrier with a mouse hole at X[5.5, 9.5]."""
    b = Builder("SM_Bookcase")
    y0, y1 = 31.0, 34.0
    H = 14.0
    # base plinth with the mouse hole opening
    box_minmax(b, (0, y0, 0), (5.5, y1, 1.8), WOOD_D, "Wood", bevel=0.05)
    box_minmax(b, (9.5, y0, 0), (16, y1, 1.8), WOOD_D, "Wood", bevel=0.05)
    box_minmax(b, (5.5, y0, 1.6), (9.5, y1, 1.8), WOOD_D, "Wood")
    # arch trim around the hole
    for i in range(9):
        a = math.pi * i / 8
        b.box((7.5 + math.cos(a) * 2.1, y0 - 0.05, 0.0 + math.sin(a) * 1.75), (0.5, 0.25, 0.5), "#3b281b", "Wood", bevel=0.05)
    # shelves / sides / back
    box_minmax(b, (0, y1 - 0.3, 1.8), (16, y1, H), WOOD, "Wood")
    for x in (0.0, 15.6):
        box_minmax(b, (x, y0, 1.8), (x + 0.4, y1, H), WOOD_D, "Wood", bevel=0.05)
    for z in (1.8, 5.8, 9.8, H - 0.4):
        box_minmax(b, (0, y0, z), (16, y1, z + 0.4), WOOD_L, "Wood", bevel=0.05, segments=1)
    # books
    import random
    rnd = random.Random(7)
    cols = ["#b23a3a", "#2f5f9e", "#3c8a4a", "#d8a03a", "#7a3f8f", "#c25b2a", "#e0d6b8", "#305060"]
    for z in (2.2, 6.2, 10.2):
        x = 0.6
        while x < 15.2:
            w = rnd.uniform(0.35, 0.8)
            h = rnd.uniform(2.6, 3.4)
            tilt = 0 if rnd.random() > 0.15 else rnd.uniform(-12, 12)
            box_minmax(b, (x, y0 + 0.4, z), (x + w, y1 - 0.4, z + h), rnd.choice(cols), "Gloss", bevel=0.03, segments=1)
            x += w + rnd.uniform(0.0, 0.12)
    return b.finish()


def crates():
    """Barrier between the nook and the B area: X[16,19] Y[34,63]."""
    b = Builder("SM_CrateStack")
    y = 34.0
    k = 0
    while y < 63.0:
        y1 = min(y + 3.0, 63.0)
        for lvl, z in enumerate((0.0, 3.0, 6.0)):
            if lvl == 2 and k % 2 == 1:
                continue
            col = "#b48a5a" if (k + lvl) % 2 == 0 else "#a07446"
            box_minmax(b, (16.0, y + 0.05, z), (19.0, y1 - 0.05, z + 2.95), col, "Wood", bevel=0.08, segments=1)
            for s in (0.5, 1.5, 2.5):
                box_minmax(b, (15.95, y + 0.05, z + s - 0.06), (19.05, y1 - 0.05, z + s + 0.06), "#6e4b2c", "Wood")
        y = y1
        k += 1
    return b.finish()


def workbench():
    b = Builder("SM_Workbench")
    x0, x1, y0, y1, top = 1.5, 12.5, 41.0, 51.0, 3.0
    box_minmax(b, (x0, y0, top - 0.4), (x1, y1, top), WOOD, "Wood", bevel=0.06)
    for (x, y) in [(x0 + 0.3, y0 + 0.3), (x1 - 0.8, y0 + 0.3), (x0 + 0.3, y1 - 0.8), (x1 - 0.8, y1 - 0.8)]:
        box_minmax(b, (x, y, 0), (x + 0.5, y + 0.5, top - 0.4), WOOD_D, "Wood", bevel=0.05)
    box_minmax(b, (x0 + 0.4, y0 + 0.4, 0.8), (x1 - 0.4, y1 - 0.4, 1.0), WOOD_D, "Wood")
    # green felt work mat
    box_minmax(b, (x0 + 1.0, y0 + 1.0, top), (x1 - 4.5, y1 - 1.0, top + 0.05), "#2f6b4c", "Cloth", bevel=0.02, segments=1)
    # screwdriver
    b.cylinder((10.5, 44.0, top + 0.18), 0.18, 2.4, "#c43b2f", "Gloss", verts=8, rot=(0, 90, 30))
    b.cylinder((9.0, 46.6, top + 0.08), 0.06, 2.6, STEEL, "Metal", verts=8, rot=(0, 90, 30))
    # loupe
    b.lathe([(0.0, 0.0), (0.45, 0.0), (0.5, 0.1), (0.38, 0.6), (0.0, 0.62)], "#1d1d1d", "Gloss", loc=(11.0, 49.0, top))
    # tweezers
    b.box((9.6, 49.6, top + 0.05), (2.0, 0.08, 0.06), STEEL, "Metal", rot=(0, 0, 18))
    b.box((9.6, 49.8, top + 0.05), (2.0, 0.08, 0.06), STEEL, "Metal", rot=(0, 0, 12))
    # little gear scatter
    for (gx, gy, r) in [(10.5, 42.5, 0.35), (11.6, 43.0, 0.22), (11.2, 47.5, 0.28)]:
        pts = []
        teeth = 10
        for i in range(teeth * 2):
            a = 2 * math.pi * i / (teeth * 2)
            rr = r if i % 2 == 0 else r * 0.8
            pts.append((math.cos(a) * rr, math.sin(a) * rr))
        b.prism(pts, 0.06, "#d6a548", "Brass", loc=(gx, gy, top + 0.03))
    return b.finish()


def ruler():
    """Ramp from the workbench north edge (7, 51, 3) down to (7, 63, 0)."""
    b = Builder("SM_RulerRamp")
    start = Vector((7.0, 51.0, 3.0))
    end = Vector((7.0, 63.2, 0.0))
    d = end - start
    L = d.length
    rot = d.to_track_quat('X', 'Z')
    mid = (start + end) / 2 + Vector((0, 0, -0.08))
    b.box(tuple(mid), (L + 0.6, 1.8, 0.16), "#e9d8a6", "Wood", rot=rot, bevel=0.03, segments=1)
    # tick marks
    n = 30
    for i in range(1, n):
        t = i / n
        p = start + d * t + Vector((0, 0, 0.01))
        length = 0.7 if i % 5 == 0 else 0.35
        b.box(tuple(p + Vector((-0.9 + length / 2 + 0.05, 0, 0))), (length, 0.05, 0.02), "#2a2a2a", "Matte", rot=rot)
    return b.finish()


def entry_wedge():
    """Little ramp from the floor into the sink cabinet's west door (X 13.5 -> 16, up to 1.0)."""
    b = Builder("SM_EntryWedge")
    pts = [(0, 0), (4.0, 0), (4.0, 1.02)]
    b.prism(pts, 5.0, "#f2d64b", "Gloss", rot=(90, 0, 0), loc=(12.0, 69.5, 0))
    return b.finish()


# ------------------------------------------------------------------------------------------
def sink_cabinet():
    b = Builder("SM_SinkCabinet")
    X0, X1, Y0, Y1 = 16.0, 56.0, 63.0, 80.0
    FL, CEIL, TOP = 1.0, 5.6, 6.2
    PAINT = "#5f8f86"
    # plinth + floor
    box_minmax(b, (X0, Y0 + 0.3, 0), (X1, Y1, FL), "#3c3a36", "Matte")
    box_minmax(b, (X0 + 0.5, Y0 + 0.5, FL - 0.05), (X1 - 0.5, Y1 - 0.5, FL), "#d8cbb0", "Matte")
    # west face with door opening Y[67,72]
    box_minmax(b, (X0, Y0, FL), (X0 + 0.5, 67.0, CEIL), PAINT, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (X0, 72.0, FL), (X0 + 0.5, Y1, CEIL), PAINT, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (X0, 67.0, 5.4), (X0 + 0.5, 72.0, CEIL), PAINT, "Gloss")
    # front (south) face with exit opening X[50,54.5]
    box_minmax(b, (X0, Y0, FL), (50.0, Y0 + 0.5, CEIL), PAINT, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (54.5, Y0, FL), (X1, Y0 + 0.5, CEIL), PAINT, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (50.0, Y0, 5.4), (54.5, Y0 + 0.5, CEIL), PAINT, "Gloss")
    # door panel decoration on the front face
    x = X0 + 0.6
    while x < 49.5:
        x1 = min(x + 4.4, 49.5)
        box_minmax(b, (x + 0.3, Y0 - 0.12, FL + 0.4), (x1 - 0.3, Y0, CEIL - 0.4), "#6fa196", "Gloss", bevel=0.06, segments=1)
        b.sphere(((x + x1) / 2 + 1.4, Y0 - 0.25, 3.4), 0.22, "#d8b45a", "Brass", subdiv=1)
        x = x1
    # open door leaves next to the openings
    box_minmax(b, (50.0 - 4.2, Y0 - 4.2, FL), (50.0 - 3.9, Y0, CEIL - 0.1), "#6fa196", "Gloss", rot=None)
    box_minmax(b, (X0 - 4.4, 72.0, FL), (X0, 72.3, CEIL - 0.1), "#6fa196", "Gloss")
    # east + back walls
    box_minmax(b, (X1 - 0.5, Y0, FL), (X1, Y1, CEIL), PAINT, "Gloss")
    box_minmax(b, (X0, Y1 - 0.5, FL), (X1, Y1, CEIL), "#cfc2a5", "Matte")
    # ceiling (underside of the countertop) + countertop with overhang
    box_minmax(b, (X0, Y0, CEIL), (X1, Y1, CEIL + 0.2), "#cfc2a5", "Matte")
    box_minmax(b, (X0 - 0.3, Y0 - 0.6, CEIL + 0.2), (X1 + 0.3, Y1, TOP), "#e7e1d6", "Gloss", bevel=0.06)
    # sink basin + faucet on top
    box_minmax(b, (28.0, 66.0, TOP), (40.0, 76.0, TOP + 0.15), STEEL, "Metal", bevel=0.1)
    box_minmax(b, (28.6, 66.6, TOP + 0.15), (39.4, 75.4, TOP + 0.2), "#7d838b", "Metal")
    b.cylinder((34.0, 77.5, TOP + 1.8), 0.35, 3.6, STEEL, "Metal", verts=16)
    b.cylinder((34.0, 76.2, TOP + 3.5), 0.25, 2.8, STEEL, "Metal", verts=12, rot=(90, 0, 0))
    # interior dividers (maze) with stacked boxes look
    divs = [(26.0, 64.0, 74.0), (36.0, 68.0, 79.0), (46.0, 64.0, 74.0)]
    for (dx, dy0, dy1) in divs:
        box_minmax(b, (dx, dy0, FL), (dx + 0.6, dy1, CEIL), "#b99a73", "Wood", bevel=0.05, segments=1)
        for z in (2.2, 3.6):
            box_minmax(b, (dx - 0.05, dy0, z), (dx + 0.65, dy1, z + 0.1), "#8a6a46", "Wood")
    # pipes: drain + P-trap running to the back wall
    b.tube([(34.0, 71.0, CEIL), (34.0, 71.0, 3.2), (34.0, 73.5, 2.2), (34.0, 76.0, 3.2), (34.0, 79.4, 3.2)], 0.45, "#c9ccd2", "Metal", seg=12)
    b.tube([(30.0, 79.4, 2.0), (30.0, 76.0, 2.0)], 0.3, "#c08a3e", "Brass", seg=10)
    return b.finish()


def cabinet_props():
    """Clutter inside the sink cabinet: bottles, bucket, sponges, nightlight."""
    b = Builder("SM_CabinetClutter")
    FL = 1.0
    # spray bottles along dividers
    for (x, y, col) in [(24.6, 75.5, "#3aa0d8"), (25.2, 77.8, "#e45a3a"), (38.2, 64.8, "#7cc04a"), (44.5, 77.0, "#f2c12e")]:
        b.lathe([(0.0, 0.0), (0.7, 0.0), (0.75, 0.2), (0.7, 2.0), (0.4, 2.4), (0.25, 2.8), (0.0, 2.85)], col, "Gloss", loc=(x, y, FL), seg=14)
        b.box((x + 0.3, y, FL + 3.0), (0.8, 0.35, 0.35), "#efefef", "Gloss")
    # bucket
    b.lathe([(0.0, 0.0), (1.2, 0.0), (1.45, 2.2), (1.35, 2.2), (1.1, 0.15), (0.0, 0.15)], "#d84a3a", "Gloss", loc=(52.0, 77.0, FL), seg=20)
    # sponges
    for (x, y) in [(20.0, 77.5), (41.0, 65.5)]:
        box_minmax(b, (x - 0.9, y - 0.6, FL), (x + 0.9, y + 0.6, FL + 0.5), "#f6d64a", "Matte", bevel=0.08)
        box_minmax(b, (x - 0.9, y - 0.6, FL + 0.5), (x + 0.9, y + 0.6, FL + 0.7), "#3f8f4f", "Matte", bevel=0.05)
    return b.finish()


# ------------------------------------------------------------------------------------------
def counter():
    b = Builder("SM_Counter")
    X0, X1, Y0, Y1 = 78.0, 100.0, 63.0, 80.0
    TOP = 6.0
    PAINT = "#e7d3a7"
    # left / right blocks
    box_minmax(b, (X0, Y0, 0), (80.0, Y1, 5.6), PAINT, "Gloss", bevel=0.04, segments=1)
    box_minmax(b, (88.0, Y0, 0), (X1, Y1, 5.6), PAINT, "Gloss", bevel=0.04, segments=1)
    # drawer column back + side walls of the drawer bay
    box_minmax(b, (80.0, 72.3, 0), (88.0, Y1, 5.6), "#cdb88d", "Matte")
    # rails between drawers (front frame), drawers occupy Z[k+0.05, k+0.95]
    for k in range(5):
        box_minmax(b, (80.0, Y0, k + 0.95), (88.0, Y0 + 0.3, k + 1.05), PAINT, "Gloss")
    box_minmax(b, (80.0, Y0, 4.95), (88.0, 72.3, 5.6), "#cdb88d", "Matte")
    box_minmax(b, (80.0, Y0, 0), (88.0, Y0 + 0.3, 0.05), PAINT, "Gloss")
    # countertop slab (butcher block)
    box_minmax(b, (X0 - 0.3, Y0 - 0.6, 5.6), (X1, Y1, TOP), "#c48a52", "Wood", bevel=0.06)
    for i in range(12):
        x = X0 + i * 1.85
        box_minmax(b, (x, Y0 - 0.62, 5.62), (x + 0.06, Y1, TOP + 0.005), "#a8713f", "Wood")
    # cabinet doors on the right block
    for x in (88.6, 94.3):
        box_minmax(b, (x, Y0 - 0.12, 0.5), (x + 5.4, Y0, 5.2), "#f0e0b8", "Gloss", bevel=0.08, segments=1)
        b.sphere((x + (4.8 if x < 90 else 0.6), Y0 - 0.3, 3.0), 0.22, "#d8b45a", "Brass", subdiv=1)
    # backsplash tiles
    for i in range(22):
        for j in range(4):
            box_minmax(b, (X0 + i * 1.0 + 0.04, Y1 - 0.15, TOP + j * 1.0 + 0.04), (X0 + i * 1.0 + 0.96, Y1, TOP + j * 1.0 + 0.96),
                       "#dfeef0" if (i + j) % 2 == 0 else "#bcd9dd", "Gloss")
    return b.finish()


def drawer():
    """Pull drawer, pivot at front-bottom-center, body extends to local -X, solid top (towels)."""
    b = Builder("SM_Drawer")
    W, D, H = 8.0, 9.0, 0.9
    box_minmax(b, (-D, -W / 2 + 0.1, 0), (0, W / 2 - 0.1, H - 0.08), "#d9c39a", "Wood", bevel=0.03, segments=1)
    # folded towels on top (walkable surface)
    cols = ["#f0f0f0", "#8ec5e8", "#f3b2b2", "#f0f0f0"]
    for i in range(4):
        box_minmax(b, (-D + 0.2 + i * 2.2, -W / 2 + 0.3, H - 0.1), (-D + 2.2 + i * 2.2, W / 2 - 0.3, H), cols[i], "Cloth", bevel=0.05, segments=1)
    # front panel + handle
    box_minmax(b, (0, -W / 2, 0), (0.18, W / 2, H), "#e7d3a7", "Gloss", bevel=0.05, segments=1)
    box_minmax(b, (0.18, -1.3, 0.35), (0.45, -1.1, 0.55), "#c9a24e", "Brass", bevel=0.03, segments=1)
    box_minmax(b, (0.18, 1.1, 0.35), (0.45, 1.3, 0.55), "#c9a24e", "Brass", bevel=0.03, segments=1)
    box_minmax(b, (0.38, -1.3, 0.35), (0.5, 1.3, 0.55), "#e0b85c", "Brass", bevel=0.04, segments=1)
    return b.finish()


def fridge():
    b = Builder("SM_Fridge")
    X0, X1, Y0, Y1, H = 90.0, 100.0, 8.0, 28.0, 19.0
    E = "#f1ede2"
    # shell (open front at X0): back, sides, top, bottom
    box_minmax(b, (X1 - 0.6, Y0, 0.6), (X1, Y1, H), E, "Gloss", bevel=0.1)
    box_minmax(b, (X0, Y0, 0.6), (X1, Y0 + 0.6, H), E, "Gloss", bevel=0.1)
    box_minmax(b, (X0, Y1 - 0.6, 0.6), (X1, Y1, H), E, "Gloss", bevel=0.1)
    box_minmax(b, (X0, Y0, H - 0.8), (X1, Y1, H), E, "Gloss", bevel=0.25)
    box_minmax(b, (X0, Y0, 0.6), (X1, Y1, 1.0), "#e2ddd0", "Gloss", bevel=0.05)
    # feet + kick plate
    box_minmax(b, (X0 + 0.2, Y0 + 0.3, 0), (X1 - 0.3, Y1 - 0.3, 0.6), "#7d7f84", "Metal")
    # interior liner + light panel
    box_minmax(b, (X1 - 0.7, Y0 + 0.6, 1.0), (X1 - 0.6, Y1 - 0.6, H - 0.8), "#dfe9f2", "Matte")
    box_minmax(b, (X0 + 2.0, Y0 + 6.0, H - 0.85), (X1 - 2.0, Y1 - 6.0, H - 0.8), "#eaf6ff", "Emissive")
    # shelves (glass) and food
    for z in (6.0, 10.5, 14.5):
        box_minmax(b, (X0 + 0.3, Y0 + 0.6, z), (X1 - 0.7, Y1 - 0.6, z + 0.1), "#cfe6f7", "Glass")
    b.lathe([(0.0, 0.0), (1.0, 0.0), (1.0, 3.2), (0.4, 4.0), (0.3, 4.6), (0.0, 4.6)], "#e9f2ff", "Gloss", loc=(97.0, 12.0, 6.1), seg=14)  # milk
    b.lathe([(0.0, 0.0), (0.9, 0.0), (0.95, 1.4), (0.0, 1.4)], "#c6392f", "Gloss", loc=(96.0, 22.0, 10.6), seg=14)  # jam
    box_minmax(b, (94.0, 16.0, 10.6), (97.0, 20.0, 11.8), "#f2c94a", "Matte", bevel=0.1)  # cheese
    b.sphere((95.5, 24.5, 15.3), 0.9, "#d93a2c", "Gloss", subdiv=2)  # apple
    b.sphere((95.5, 13.0, 15.0), 0.6, "#f2a33a", "Gloss", subdiv=2)
    # bottom crisper drawers (low so dolls can hop in at Z 1.0)
    box_minmax(b, (93.0, Y0 + 1.0, 1.0), (X1 - 1.0, 17.0, 3.0), "#d7ebf5", "Glass", bevel=0.1)
    # handle on the body
    return b.finish()


def fridge_door(name, width, hinge_side):
    """Pivot at the hinge (bottom). Door extends along +Y (hinge_side=1) or -Y. Closed = facing -X."""
    b = Builder(name)
    s = 1 if hinge_side > 0 else -1
    E = "#f1ede2"
    H = 19.0
    y0, y1 = (0.0, width) if s > 0 else (-width, 0.0)
    box_minmax(b, (-1.2, y0, 0.6), (0.0, y1, H), E, "Gloss", bevel=0.2)
    # door shelves inside (+X face)
    for z in (4.0, 9.0, 14.0):
        box_minmax(b, (0.0, y0 + 0.4, z), (1.2, y1 - 0.4, z + 0.15), "#d9e8f2", "Glass")
        box_minmax(b, (1.0, y0 + 0.4, z), (1.2, y1 - 0.4, z + 1.2), "#d9e8f2", "Glass")
    # chrome handle (outer face -X) far from the hinge
    hy = y1 - 1.0 if s > 0 else y0 + 1.0
    b.cylinder((-1.6, hy, 10.0), 0.18, 5.0, "#d9dde3", "Metal", verts=10)
    box_minmax(b, (-1.6, hy - 0.15, 7.4), (-1.2, hy + 0.15, 7.7), "#d9dde3", "Metal")
    box_minmax(b, (-1.6, hy - 0.15, 12.3), (-1.2, hy + 0.15, 12.6), "#d9dde3", "Metal")
    # retro badge + magnets
    box_minmax(b, (-1.25, (y0 + y1) / 2 - 1.2, 16.5), (-1.2, (y0 + y1) / 2 + 1.2, 17.2), "#c9a24e", "Brass")
    for (oy, oz, col) in [(0.3, 12.0, "#e63946"), (0.55, 9.0, "#457b9d"), (0.25, 6.5, "#f4a261")]:
        b.cylinder((-1.3, y0 + (y1 - y0) * oy, oz), 0.35, 0.15, col, "Gloss", verts=12, rot=(0, 90, 0))
    return b.finish()


# ------------------------------------------------------------------------------------------
def table():
    b = Builder("SM_TableTop")
    X0, X1, Y0, Y1 = 18.0, 43.0, 5.0, 30.0
    box_minmax(b, (X0, Y0, 6.0), (X1, Y1, 6.5), "#8a5a34", "Wood", bevel=0.1)
    # apron
    box_minmax(b, (X0 + 0.6, Y0 + 0.6, 5.3), (X1 - 0.6, Y0 + 1.0, 6.0), "#6e4529", "Wood")
    box_minmax(b, (X0 + 0.6, Y1 - 1.0, 5.3), (X1 - 0.6, Y1 - 0.6, 6.0), "#6e4529", "Wood")
    box_minmax(b, (X0 + 0.6, Y0 + 0.6, 5.3), (X0 + 1.0, Y1 - 0.6, 6.0), "#6e4529", "Wood")
    box_minmax(b, (X1 - 1.0, Y0 + 0.6, 5.3), (X1 - 0.6, Y1 - 0.6, 6.0), "#6e4529", "Wood")
    # cloth on top with short gingham overhang on N/S/E (west side drape is a separate actor)
    box_minmax(b, (X0 - 0.2, Y0 - 0.2, 6.5), (X1 + 0.2, Y1 + 0.2, 6.56), "#f3f0ea", "Cloth")
    for side in range(3):
        for i in range(25):
            col = "#c8383a" if i % 2 == 0 else "#f3f0ea"
            if side == 0:   # south
                box_minmax(b, (X0 + i, Y0 - 0.3, 5.2), (X0 + i + 1, Y0 - 0.2, 6.56), col, "Cloth")
            elif side == 1:  # north
                box_minmax(b, (X0 + i, Y1 + 0.2, 5.2), (X0 + i + 1, Y1 + 0.3, 6.56), col, "Cloth")
            else:  # east
                box_minmax(b, (X1 + 0.2, Y0 + i, 5.2), (X1 + 0.3, Y0 + i + 1, 6.56), col, "Cloth")
    # dinner things on the table top (out of reach, for silhouette)
    b.lathe([(0.0, 0.0), (2.4, 0.0), (2.6, 0.25), (2.2, 0.3), (0.0, 0.3)], "#f7f3ea", "Gloss", loc=(26.0, 14.0, 6.56), seg=24)
    b.lathe([(0.0, 0.0), (0.6, 0.0), (0.7, 3.5), (0.5, 4.2), (0.0, 4.2)], "#4b7f5a", "Glass", loc=(33.0, 22.0, 6.56), seg=16)
    b.lathe([(0.0, 0.0), (1.4, 0.0), (1.6, 1.6), (1.2, 2.8), (0.0, 2.8)], "#c78f5a", "Gloss", loc=(36.0, 11.0, 6.56), seg=18)
    return b.finish()


def table_leg():
    b = Builder("SM_TableLeg")
    prof = [(0.0, 0.0), (0.55, 0.0), (0.55, 0.3), (0.42, 0.5), (0.5, 1.2), (0.36, 2.4), (0.6, 3.0), (0.6, 3.4), (0.42, 3.6),
            (0.55, 4.2), (0.55, 4.6), (0.7, 4.8), (0.7, 6.0), (0.0, 6.0)]
    b.lathe(prof, "#7a4c2c", "Wood", seg=16)
    return b.finish()


def chair():
    """Pivot bottom center, faces +X (backrest at -X)."""
    b = Builder("SM_Chair")
    S = 4.6
    for (x, y) in [(-S / 2 + 0.4, -S / 2 + 0.4), (S / 2 - 0.4, -S / 2 + 0.4), (-S / 2 + 0.4, S / 2 - 0.4), (S / 2 - 0.4, S / 2 - 0.4)]:
        b.cylinder((x, y, 2.1), 0.28, 4.2, "#7a4c2c", "Wood", verts=10, radius2=0.22)
    box_minmax(b, (-S / 2, -S / 2, 4.2), (S / 2, S / 2, 4.6), "#9b6a40", "Wood", bevel=0.1)
    box_minmax(b, (-S / 2 + 0.15, -S / 2 + 0.3, 4.6), (S / 2 - 0.3, S / 2 - 0.3, 4.75), "#b23a3a", "Cloth", bevel=0.08)
    # rungs
    for y in (-S / 2 + 0.4, S / 2 - 0.4):
        box_minmax(b, (-S / 2 + 0.4, y - 0.08, 1.3), (S / 2 - 0.4, y + 0.08, 1.45), "#6e4529", "Wood")
    # backrest
    for y in (-S / 2 + 0.4, S / 2 - 0.4):
        b.cylinder((-S / 2 + 0.35, y, 6.9), 0.2, 4.6, "#7a4c2c", "Wood", verts=10)
    for z in (6.2, 7.6, 9.0):
        box_minmax(b, (-S / 2 + 0.15, -S / 2 + 0.4, z), (-S / 2 + 0.55, S / 2 - 0.4, z + 0.5), "#9b6a40", "Wood", bevel=0.08)
    return b.finish()


def tablecloth_curtain():
    """Hanging drape (west side of the table). Pivot: top edge center, hangs down to Z=0, spans Y +-12.5.
    Local X = outward normal."""
    b = Builder("SM_Tablecloth_Curtain")
    import bmesh as _bm
    nY, nZ = 50, 14
    W, H = 25.0, 6.6
    for j in range(nY):
        for k in range(nZ):
            y0 = -W / 2 + W * j / nY
            y1 = -W / 2 + W * (j + 1) / nY
            z0 = -H * k / nZ
            z1 = -H * (k + 1) / nZ
            amp0 = 0.15 + 0.35 * (k / nZ)
            amp1 = 0.15 + 0.35 * ((k + 1) / nZ)
            x00 = math.sin(y0 * 1.3) * amp0
            x10 = math.sin(y1 * 1.3) * amp0
            x01 = math.sin(y0 * 1.3) * amp1
            x11 = math.sin(y1 * 1.3) * amp1
            col = "#c8383a" if ((j // 2) + (k // 1)) % 2 == 0 else "#f3f0ea"
            before = set(b.bm.faces)
            v = [b.bm.verts.new((x00, y0, z0)), b.bm.verts.new((x10, y1, z0)), b.bm.verts.new((x11, y1, z1)), b.bm.verts.new((x01, y0, z1))]
            b.bm.faces.new(v)
            # thickness back face so collision works from both sides
            vb = [b.bm.verts.new((x00 - 0.15, y0, z0)), b.bm.verts.new((x01 - 0.15, y0, z1)), b.bm.verts.new((x11 - 0.15, y1, z1)), b.bm.verts.new((x10 - 0.15, y1, z0))]
            b.bm.faces.new(vb)
            faces = [f for f in b.bm.faces if f not in before]
            b._finish_geom(faces, col, "Cloth", smooth=True, sharp_angle=80)
    _bm.ops.remove_doubles(b.bm, verts=b.bm.verts, dist=1e-4)
    return b.finish()


def tablecloth_fallen():
    """Crumpled cloth lying on the floor, pivot center, ~26 x 9 m, low bumps (walkable slide)."""
    b = Builder("SM_Tablecloth_Fallen")
    import bmesh as _bm
    nx, ny = 52, 18
    Lx, Ly = 9.0, 26.0
    grid = {}
    for i in range(nx + 1):
        for j in range(ny + 1):
            x = -Lx / 2 + Lx * i / nx
            y = -Ly / 2 + Ly * j / ny
            z = 0.06 + 0.14 * (0.5 + 0.5 * math.sin(x * 1.7 + y * 0.6)) * (0.5 + 0.5 * math.cos(y * 0.9))
            grid[(i, j)] = b.bm.verts.new((x, y, z))
    before = set(b.bm.faces)
    for i in range(nx):
        for j in range(ny):
            f = b.bm.faces.new((grid[(i, j)], grid[(i + 1, j)], grid[(i + 1, j + 1)], grid[(i, j + 1)]))
    faces = [f for f in b.bm.faces if f not in before]
    b._finish_geom(faces, "#f3f0ea", "Cloth", smooth=True, sharp_angle=80)
    for f in faces:
        c = f.calc_center_median()
        if (int((c.x + 50) / 1.0) + int((c.y + 50) / 1.0)) % 2 == 0:
            for loop in f.loops:
                loop[b.col] = srgb("#c8383a")
    return b.finish()


ALL = [floor, walls, bookcase, crates, workbench, ruler, entry_wedge, sink_cabinet, cabinet_props, counter, drawer, fridge,
       lambda: fridge_door("SM_FridgeDoor_L", 10.0, 1), lambda: fridge_door("SM_FridgeDoor_R", 10.0, -1),
       table, table_leg, chair, tablecloth_curtain, tablecloth_fallen]


def build_all(export=True):
    objs = []
    for fn in ALL:
        r = fn()
        objs += r if isinstance(r, list) else [r]
    if export:
        for o in objs:
            export_static(o, o.name + ".fbx", sub="Arch")
    return objs
