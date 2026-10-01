"""Props + gameplay meshes. Pivots are local (documented per mesh)."""
import bpy, math, os, sys
from mathutils import Vector
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Blender")
import importlib, crank_kit
importlib.reload(crank_kit)
from crank_kit import *
from arch import box_minmax

STEEL = "#c3c8cf"
BRASS = "#d8a446"
WOOD = "#9a6a43"


def gear_outline(r, teeth, depth=0.18):
    pts = []
    for i in range(teeth * 4):
        a = 2 * math.pi * i / (teeth * 4)
        phase = i % 4
        rr = r if phase in (1, 2) else r * (1 - depth)
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    return pts


def pot(name, r, h, body=STEEL, mat="Metal", accent="#8d939b"):
    """Upturned pot: pivot bottom center, top (pot base) at Z=h."""
    b = Builder(name)
    b.lathe([(0.0, 0.0), (r + 0.05, 0.0), (r + 0.05, 0.18), (r - 0.02, 0.24), (r - 0.02, h - 0.15), (r - 0.15, h), (0.0, h)], body, mat, seg=32)
    b.cylinder((0, 0, h - 0.01), r * 0.7, 0.03, accent, mat, verts=32)
    for s in (1, -1):
        b.torus((0, s * (r + 0.25), h * 0.7), 0.45, 0.09, "#3a3a3a", "Matte", seg=12, ring=6, rot=(90, 0, 0))
    return b.finish()


def build_props():
    out = []
    out.append(pot("SM_Pot_A", 2.6, 2.6))
    out.append(pot("SM_Pot_B", 2.6, 3.4, body="#d84a3a", mat="Gloss", accent="#f0e8dc"))
    out.append(pot("SM_Pot_C", 2.0, 4.5, body="#3d6fb6", mat="Gloss", accent="#f0e8dc"))

    # colander (upturned) - same footprint as a pot, with holes pattern
    b = Builder("SM_Colander")
    b.lathe([(0.0, 0.0), (2.45, 0.0), (2.45, 0.2), (2.3, 0.3), (2.0, 2.4), (1.6, 2.8), (0.0, 2.8)], "#e6a33e", "Gloss", seg=32)
    for i in range(24):
        a = 2 * math.pi * i / 24
        for z, rr in ((1.0, 2.15), (1.8, 1.95)):
            b.cylinder((math.cos(a) * rr, math.sin(a) * rr, z), 0.12, 0.05, "#5a3a10", "Matte", verts=8, rot=(0, 90, math.degrees(a)))
    out.append(b.finish())

    # book stack (P0): pivot bottom center, 7 x 6.8, top 1.0
    b = Builder("SM_BookStack")
    box_minmax(b, (-3.5, -3.4, 0.0), (3.5, 3.4, 0.5), "#7a2f2f", "Gloss", bevel=0.05)
    box_minmax(b, (-3.3, -3.2, 0.06), (3.6, 3.2, 0.44), "#efe6cf", "Matte")
    box_minmax(b, (-3.4, -3.35, 0.5), (3.45, 3.35, 1.0), "#2f5f7e", "Gloss", bevel=0.05)
    box_minmax(b, (-3.2, -3.15, 0.56), (3.55, 3.15, 0.94), "#efe6cf", "Matte")
    box_minmax(b, (-1.5, -3.4, 0.2), (1.5, -3.38, 0.3), BRASS, "Brass")
    out.append(b.finish())

    # spatula bridge: pivot at hinge (X=0, top surface Z=0), extends +X 6.6
    b = Builder("SM_Spatula")
    box_minmax(b, (0.0, -0.8, -0.15), (4.8, 0.8, 0.0), "#c79a63", "Wood", bevel=0.04)
    for i in range(3):
        box_minmax(b, (1.0 + i * 1.2, -0.08, -0.16), (1.8 + i * 1.2, 0.08, 0.01), "#8a6038", "Wood")
    box_minmax(b, (4.8, -0.35, -0.13), (6.8, 0.35, -0.02), "#8a6038", "Wood", bevel=0.03)
    out.append(b.finish())

    # tray ramp (detour): pivot at the low end bottom-center, rises to 1.0 at X=6.5
    b = Builder("SM_TrayRamp")
    b.prism([(0, 0), (6.5, 0), (6.5, 1.02)], 3.0, "#b7bcc3", "Metal", rot=(90, 0, 0))
    out.append(b.finish())

    # can fence module: 6 m long along +X, pivot at start bottom-center
    b = Builder("SM_CanFence")
    labels = [("#d23b2b", "#f2e2c4"), ("#2a6fd6", "#f2f2f2"), ("#3c8a4a", "#f2d64b"), ("#f2a33a", "#7a2f2f"), ("#7a3f8f", "#f2f2f2")]
    for i in range(5):
        x = 0.62 + i * 1.19
        c1, c2 = labels[i]
        b.cylinder((x, 0, 1.1), 0.62, 2.2, "#c9ced6", "Metal", verts=18)
        b.cylinder((x, 0, 1.05), 0.63, 1.3, c1, "Gloss", verts=18)
        b.cylinder((x, 0, 1.05), 0.635, 0.35, c2, "Gloss", verts=18)
        b.cylinder((x, 0, 2.21), 0.5, 0.04, "#9aa1aa", "Metal", verts=18)
    out.append(b.finish())

    # crate (3 m cube), pivot bottom center
    b = Builder("SM_Crate")
    box_minmax(b, (-1.5, -1.5, 0), (1.5, 1.5, 3.0), "#b48a5a", "Wood", bevel=0.08)
    for z in (0.5, 1.5, 2.5):
        box_minmax(b, (-1.55, -1.55, z - 0.06), (1.55, 1.55, z + 0.06), "#6e4b2c", "Wood")
    out.append(b.finish())

    # lever base + handle (handle pivot at its bottom, points +Z)
    b = Builder("SM_LeverBase")
    box_minmax(b, (-0.45, -0.45, 0), (0.45, 0.45, 0.35), "#4a4f57", "Metal", bevel=0.05)
    box_minmax(b, (-0.2, -0.2, 0.35), (0.2, 0.2, 0.5), "#2a2d33", "Metal", bevel=0.03)
    out.append(b.finish())
    b = Builder("SM_LeverHandle")
    b.cylinder((0, 0, 0.65), 0.08, 1.3, "#cfd3d9", "Metal", verts=10)
    b.sphere((0, 0, 1.35), 0.22, "#e63b2e", "Gloss", subdiv=2)
    out.append(b.finish())

    # delivery tube (brass funnel + glass tube): pivot bottom center
    b = Builder("SM_DeliveryTube")
    b.lathe([(0.0, 0.0), (1.0, 0.0), (1.05, 0.1), (1.35, 1.1), (1.5, 1.2), (1.45, 1.3), (1.15, 0.35), (0.0, 0.35)], BRASS, "Brass", seg=24)
    b.cylinder((-1.3, 0, 2.6), 0.35, 3.2, "#bfe3ff", "Glass", verts=16)
    b.tube([(-1.3, 0, 4.2), (-1.3, 0, 4.6), (-0.6, 0, 5.0)], 0.2, BRASS, "Brass", seg=10)
    b.torus((0, 0, 1.25), 1.42, 0.06, "#ffd36a", "Emissive", seg=32, ring=6)
    out.append(b.finish())

    # clock movement (main spring delivery spot): pivot bottom center
    b = Builder("SM_ClockMovement")
    b.cylinder((0, 0, 0.1), 1.6, 0.2, "#c99a3e", "Brass", verts=32)
    for (x, y) in [(1.2, 1.2), (-1.2, 1.2), (1.2, -1.2), (-1.2, -1.2)]:
        b.cylinder((x, y, 0.7), 0.1, 1.0, "#b8892f", "Brass", verts=8)
    b.cylinder((0, 0, 1.25), 1.6, 0.12, "#c99a3e", "Brass", verts=32)
    for (gx, gy, r, z) in [(0.6, 0.5, 0.55, 0.45), (-0.7, 0.2, 0.4, 0.6), (0.1, -0.8, 0.35, 0.8)]:
        b.prism(gear_outline(r, 12), 0.08, "#e3b257", "Brass", loc=(gx, gy, z))
    b.torus((0, 0, 1.33), 0.95, 0.06, "#ffd36a", "Emissive", seg=32, ring=6)
    out.append(b.finish())

    # music box station: body pivot bottom center (top at 0.9), dancer pivot base, crank pivot axle
    b = Builder("SM_MusicBox")
    box_minmax(b, (-1.2, -1.2, 0), (1.2, 1.2, 0.9), "#8a4f2e", "Wood", bevel=0.08)
    box_minmax(b, (-1.1, -1.1, 0.88), (1.1, 1.1, 0.92), "#3f6f5f", "Cloth")
    for (x, y) in [(-1.15, -1.15), (1.15, -1.15), (-1.15, 1.15), (1.15, 1.15)]:
        box_minmax(b, (x - 0.12, y - 0.12, 0.0), (x + 0.12, y + 0.12, 0.95), BRASS, "Brass", bevel=0.03)
    # open lid at the back
    box_minmax(b, (-1.25, 1.15, 0.9), (1.25, 1.3, 3.2), "#8a4f2e", "Wood", bevel=0.06)
    box_minmax(b, (-1.0, 1.1, 1.1), (1.0, 1.16, 3.0), "#dfe9f2", "Glass")
    # comb
    for i in range(10):
        box_minmax(b, (-0.8 + i * 0.16, -0.9, 0.92), (-0.72 + i * 0.16, -0.3, 0.97), "#d8dde3", "Metal")
    out.append(b.finish())
    b = Builder("SM_MusicBox_Dancer")
    b.cylinder((0, 0, 0.05), 0.5, 0.1, "#e8c46a", "Brass", verts=20)
    b.cylinder((0, 0, 0.3), 0.04, 0.4, "#d8dde3", "Metal", verts=8)
    b.cylinder((0, 0, 0.75), 0.42, 0.18, "#f4b8c8", "Gloss", verts=16, radius2=0.12)
    b.cylinder((0, 0, 1.0), 0.1, 0.4, "#f7e3e8", "Gloss", verts=10)
    b.sphere((0, 0, 1.3), 0.12, "#f7e3d6", "Gloss", subdiv=2)
    b.box((0, 0.18, 1.12), (0.04, 0.5, 0.04), "#f7e3d6", "Gloss", rot=(30, 0, 0))
    b.box((0, -0.18, 1.12), (0.04, 0.5, 0.04), "#f7e3d6", "Gloss", rot=(-30, 0, 0))
    out.append(b.finish())
    b = Builder("SM_MusicBox_Crank")
    b.cylinder((0, 0, 0), 0.08, 0.4, BRASS, "Brass", verts=8, rot=(90, 0, 0))
    b.box((0, -0.2, 0.3), (0.08, 0.06, 0.6), BRASS, "Brass")
    b.cylinder((0, -0.35, 0.6), 0.1, 0.3, "#7a3a20", "Wood", verts=8, rot=(90, 0, 0))
    out.append(b.finish())

    # charging dock: pivot bottom center, faces +X
    b = Builder("SM_ChargingDock")
    b.prism([(-1.1, 0), (0.6, 0), (1.1, 0.25), (0.2, 1.4), (-1.1, 1.4)], 2.6, "#2b2f36", "Gloss", rot=(90, 0, 0))
    box_minmax(b, (1.0, -0.9, 0.0), (1.4, 0.9, 0.08), "#c8ccd2", "Metal")
    for s in (1, -1):
        box_minmax(b, (0.5, s * 0.5 - 0.15, 0.25), (0.75, s * 0.5 + 0.15, 0.45), "#e8b21e", "Metal")
    box_minmax(b, (-0.6, -0.8, 1.38), (0.2, 0.8, 1.46), "#5cff8a", "Emissive")
    out.append(b.finish())

    # dishcloth: pivot center, 4 x 3 checkered (blue / white)
    b = Builder("SM_Dishcloth")
    for i in range(8):
        for j in range(6):
            col = "#3f6fd0" if (i + j) % 2 == 0 else "#f3f0ea"
            z = 0.04 * math.sin(i * 1.3 + j)
            box_minmax(b, (-2.0 + i * 0.5, -1.5 + j * 0.5, z), (-1.5 + i * 0.5, -1.0 + j * 0.5, z + 0.1), col, "Cloth")
    out.append(b.finish())

    # --- raid parts (pivot center)
    b = Builder("SM_Part_Gear")
    b.prism(gear_outline(0.5, 12, 0.22), 0.16, "#e0b04c", "Brass")
    b.cylinder((0, 0, 0), 0.16, 0.24, "#b8892f", "Brass", verts=16)
    for k in range(4):
        b.box((0, 0, 0), (0.7, 0.08, 0.17), "#b8892f", "Brass", rot=(0, 0, 45 * k))
    out.append(b.finish())

    b = Builder("SM_Part_SpringCoil")
    path = []
    for i in range(70):
        t = i / 69.0
        a = t * 2 * math.pi * 7
        path.append((-0.6 + 1.2 * t, math.cos(a) * 0.38, math.sin(a) * 0.38))
    b.tube(path, 0.07, "#b9c0c9", "Metal", seg=8)
    for x in (-0.62, 0.62):
        b.cylinder((x, 0, 0), 0.42, 0.06, "#8d949c", "Metal", verts=16, rot=(0, 90, 0))
    out.append(b.finish())

    b = Builder("SM_Part_JewelBearing")
    b.torus((0, 0, 0), 0.32, 0.08, "#e3b257", "Brass", seg=20, ring=8)
    b.sphere((0, 0, 0), 0.26, "#e0213a", "Gloss", subdiv=1, scale=(1, 1, 0.75))
    out.append(b.finish())

    b = Builder("SM_Part_MainSpring")
    path = []
    turns = 5
    for i in range(160):
        t = i / 159.0
        a = t * 2 * math.pi * turns
        r = 0.18 + 0.7 * t
        path.append((math.cos(a) * r, math.sin(a) * r))
    # spiral strip: build as thin boxes between consecutive points
    for i in range(len(path) - 1):
        p0, p1 = Vector((*path[i], 0)), Vector((*path[i + 1], 0))
        d = p1 - p0
        mid = (p0 + p1) / 2
        ang = math.degrees(math.atan2(d.y, d.x))
        b.box(tuple(mid), (d.length + 0.01, 0.045, 0.42), "#9fa6b0", "Metal", rot=(0, 0, ang))
    b.torus((0, 0, 0.0), 0.92, 0.07, "#e3b257", "Brass", seg=36, ring=8)
    b.cylinder((0, 0, 0), 0.18, 0.5, "#e3b257", "Brass", verts=16)
    out.append(b.finish())

    # --- grandfather clock (pivot bottom center, faces +X) + hands + pendulum
    b = Builder("SM_GrandfatherClock")
    DW = "#5a3320"
    box_minmax(b, (-1.6, -2.2, 0), (1.6, 2.2, 3.0), DW, "Wood", bevel=0.1)
    box_minmax(b, (-1.3, -1.6, 3.0), (1.3, 1.6, 15.5), "#6e4029", "Wood", bevel=0.08)
    box_minmax(b, (1.25, -1.0, 5.0), (1.35, 1.0, 14.0), "#cfe6f7", "Glass")
    box_minmax(b, (-1.8, -2.4, 15.5), (1.8, 2.4, 22.0), DW, "Wood", bevel=0.1)
    b.cylinder((1.8, 0, 18.8), 1.9, 0.12, "#f3ead2", "Gloss", verts=40, rot=(0, 90, 0))
    b.torus((1.86, 0, 18.8), 1.9, 0.1, "#d8a446", "Brass", seg=40, ring=6, rot=(0, 90, 0))
    for i in range(12):
        a = 2 * math.pi * i / 12
        b.box((1.9, math.sin(a) * 1.55, 18.8 + math.cos(a) * 1.55), (0.04, 0.12, 0.3 if i % 3 == 0 else 0.18), "#2a2a2a", "Matte", rot=(math.degrees(-a), 0, 0))
    b.lathe([(0.0, 0.0), (0.6, 0.0), (0.2, 0.8), (0.0, 1.2)], "#d8a446", "Brass", loc=(0, 0, 22.0), seg=12)
    out.append(b.finish())
    b = Builder("SM_ClockHand_Hour")
    b.box((0, 0, 0.5), (0.05, 0.16, 1.0), "#1e1e1e", "Matte")
    b.cylinder((0, 0, 0), 0.12, 0.08, "#d8a446", "Brass", verts=12, rot=(0, 90, 0))
    out.append(b.finish())
    b = Builder("SM_ClockHand_Minute")
    b.box((0, 0, 0.7), (0.05, 0.1, 1.45), "#1e1e1e", "Matte")
    out.append(b.finish())
    b = Builder("SM_ClockPendulum")
    b.box((0, 0, -3.0), (0.05, 0.12, 6.0), "#c9a24e", "Brass")
    b.cylinder((0, 0, -6.0), 0.7, 0.12, "#e3b257", "Brass", verts=24, rot=(0, 90, 0))
    out.append(b.finish())

    # ceiling lamp: pivot at the ceiling attachment (top)
    b = Builder("SM_CeilingLamp")
    b.cylinder((0, 0, -3.0), 0.06, 6.0, "#222222", "Matte", verts=6)
    b.lathe([(0.0, -6.0), (0.6, -6.0), (2.6, -8.2), (2.7, -8.4), (0.5, -6.2), (0.0, -6.2)], "#2f6b4c", "Gloss", seg=24, close_bottom=False)
    b.sphere((0, 0, -7.6), 0.7, "#fff1c9", "Emissive", subdiv=2)
    out.append(b.finish())

    # countertop items
    b = Builder("SM_Toaster")
    box_minmax(b, (-2.0, -1.3, 0.2), (2.0, 1.3, 2.8), "#d6dbe1", "Metal", bevel=0.4)
    for y in (-0.45, 0.45):
        box_minmax(b, (-1.5, y - 0.15, 2.6), (1.5, y + 0.15, 2.82), "#202020", "Matte")
    box_minmax(b, (1.9, -0.2, 1.4), (2.3, 0.2, 1.6), "#202020", "Matte", bevel=0.05)
    for (x, y) in [(-1.6, -1.0), (1.6, -1.0), (-1.6, 1.0), (1.6, 1.0)]:
        b.cylinder((x, y, 0.1), 0.18, 0.2, "#202020", "Matte", verts=8)
    out.append(b.finish())

    b = Builder("SM_CoolingRack")   # pivot bottom center, 8 x 6, top 0.62, clearance 0.55
    for (x, y) in [(-3.8, -2.8), (3.8, -2.8), (-3.8, 2.8), (3.8, 2.8)]:
        box_minmax(b, (x - 0.12, y - 0.12, 0), (x + 0.12, y + 0.12, 0.62), "#9aa1aa", "Metal")
    box_minmax(b, (-4.0, -3.0, 0.55), (4.0, 3.0, 0.62), "#b8bfc8", "Metal")
    out.append(b.finish())

    b = Builder("SM_BreadBox")      # pivot bottom center, 8 x 6 x 4
    box_minmax(b, (-4.0, -3.0, 0), (4.0, 3.0, 2.8), "#a8643c", "Wood", bevel=0.15)
    b.cylinder((0, 0, 2.8), 3.0, 8.0, "#a8643c", "Wood", verts=20, rot=(0, 90, 0))
    box_minmax(b, (-4.05, -3.05, 0), (4.05, 0.0, 2.8), "#a8643c", "Wood")
    box_minmax(b, (-1.0, -3.1, 1.4), (1.0, -3.0, 2.2), "#f2e2c4", "Matte")
    out.append(b.finish())

    for name, r, h, fill in [("SM_Jar_A", 1.2, 3.6, "#e8c76a"), ("SM_Jar_B", 1.0, 2.8, "#d97a5a")]:
        b = Builder(name)
        b.lathe([(0.0, 0.0), (r, 0.0), (r, h - 0.6), (r * 0.8, h - 0.3), (0.0, h - 0.3)], "#cfe6f7", "Glass", seg=20)
        b.lathe([(0.0, 0.05), (r - 0.1, 0.05), (r - 0.1, h * 0.6), (0.0, h * 0.62)], fill, "Matte", seg=16)
        b.cylinder((0, 0, h - 0.15), r * 0.85, 0.3, "#c43b2f", "Gloss", verts=20)
        out.append(b.finish())

    b = Builder("SM_Kettle")
    b.lathe([(0.0, 0.0), (1.8, 0.0), (2.0, 0.4), (1.9, 2.0), (1.2, 2.8), (0.4, 3.0), (0.0, 3.05)], "#c7362f", "Gloss", seg=24)
    b.sphere((0, 0, 3.2), 0.3, "#202020", "Matte", subdiv=1)
    b.tube([(1.7, 0, 1.0), (2.6, 0, 2.0), (3.0, 0, 2.6)], 0.22, "#c7362f", "Gloss", seg=10)
    b.torus((-1.6, 0, 2.4), 0.8, 0.12, "#202020", "Matte", seg=16, ring=6, rot=(90, 0, 0))
    out.append(b.finish())

    # long board ramp C -> D: pivot hinge end top (X=0, Z=0), extends +X 26 m
    b = Builder("SM_PlankRamp")
    box_minmax(b, (0.0, -1.2, -0.25), (26.0, 1.2, 0.0), "#b8834f", "Wood", bevel=0.05)
    for i in range(1, 13):
        box_minmax(b, (i * 2.0 - 0.05, -1.21, -0.26), (i * 2.0 + 0.05, 1.21, 0.005), "#8a5d34", "Wood")
    out.append(b.finish())

    # small glowing night light (cabinet interior), pivot bottom
    b = Builder("SM_Nightlight")
    b.cylinder((0, 0, 0.25), 0.3, 0.5, "#e8e2d4", "Gloss", verts=12)
    b.sphere((0, 0, 0.9), 0.55, "#ffd98a", "Emissive", subdiv=2, scale=(1, 1, 0.8))
    out.append(b.finish())

    # round rug for the nook (decor), pivot center
    b = Builder("SM_Rug")
    for k, (r, col) in enumerate([(5.0, "#7a2f2f"), (4.2, "#d8a446"), (3.4, "#2f5f7e"), (2.4, "#d8a446"), (1.4, "#7a2f2f")]):
        b.cylinder((0, 0, 0.02 + k * 0.004), r, 0.04, col, "Cloth", verts=40)
    out.append(b.finish())
    return out


if __name__ == "__main__":
    clear_scene()
    objs = build_props()
    for o in objs:
        export_static(o, o.name + ".fbx", sub="Props")
