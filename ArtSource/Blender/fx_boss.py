"""FX particle meshes (unit size: 1 m = 100 uu diameter) and the Dust Eater MK-II boss."""
import bpy, math, os, sys
from mathutils import Vector
sys.path.insert(0, r"D:\Game\CRANK\ArtSource\Blender")
import importlib, crank_kit
importlib.reload(crank_kit)
from crank_kit import *

W = "#ffffff"


def build_fx():
    out = []
    b = Builder("SM_FX_Puff"); b.sphere((0, 0, 0), 0.5, W, "Matte", subdiv=1); out.append(b.finish())
    b = Builder("SM_FX_Dot"); b.sphere((0, 0, 0), 0.5, W, "Matte", subdiv=1); out.append(b.finish())
    # 5 point star (flat, thick)
    pts = []
    for i in range(10):
        a = math.pi / 2 + i * math.pi / 5
        r = 0.5 if i % 2 == 0 else 0.22
        pts.append((math.cos(a) * r, math.sin(a) * r))
    b = Builder("SM_FX_Star"); b.prism(pts, 0.16, W, "Matte", rot=(90, 0, 0)); out.append(b.finish())
    # hex bolt
    b = Builder("SM_FX_Bolt")
    b.cylinder((0, 0, 0.18), 0.45, 0.28, W, "Matte", verts=6, smooth=False)
    b.cylinder((0, 0, -0.2), 0.2, 0.6, W, "Matte", verts=8)
    out.append(b.finish())
    # coil spring
    b = Builder("SM_FX_Spring")
    path = []
    for i in range(40):
        t = i / 39.0
        a = t * 2 * math.pi * 3.5
        path.append((math.cos(a) * 0.35, math.sin(a) * 0.35, -0.5 + t))
    b.tube(path, 0.07, W, "Matte", seg=6)
    out.append(b.finish())
    # water drop
    b = Builder("SM_FX_Drop")
    b.lathe([(0.0, -0.5), (0.3, -0.42), (0.42, -0.2), (0.35, 0.05), (0.18, 0.3), (0.0, 0.5)], W, "Matte", seg=10)
    out.append(b.finish())
    # music note (eighth note) in the X-Z plane
    b = Builder("SM_FX_Note")
    b.sphere((-0.15, 0, -0.3), 0.2, W, "Matte", subdiv=1, scale=(1.3, 0.5, 0.9))
    b.box((0.02, 0, 0.1), (0.07, 0.07, 0.8), W, "Matte")
    b.box((0.18, 0, 0.42), (0.3, 0.07, 0.12), W, "Matte", rot=(0, 25, 0))
    out.append(b.finish())
    b = Builder("SM_FX_Ring"); b.torus((0, 0, 0), 0.45, 0.05, W, "Matte", seg=32, ring=6); out.append(b.finish())
    for o in out:
        export_static(o, o.name + ".fbx", sub="FX")
    return out


# ------------------------------------------------------------------------------------------
# Boss: robot vacuum "Dust Eater MK-II" (radius 1.25 m = 125 uu, height 0.8 m), front = +X

BODY = "#3d4452"
BODY_TOP = "#566176"
TRIM = "#c9ced8"
BUMPER = "#1c1c1f"
ACCENT = "#e8b21e"


def build_boss():
    out = []
    # --- Body (pivot at floor center)
    b = Builder("SM_Boss_Body")
    b.lathe([(0.0, 0.03), (1.08, 0.03), (1.18, 0.08), (1.22, 0.2), (1.22, 0.58), (1.17, 0.7), (1.0, 0.77), (0.0, 0.79)],
            BODY, "Gloss", seg=40, sharp_angle=50)
    # top plate ring + hatch recess frame
    b.cylinder((0, 0, 0.785), 0.95, 0.03, BODY_TOP, "Metal", verts=40)
    b.box((0.0, 0, 0.80), (0.82, 0.74, 0.02), "#20242c", "Matte", bevel=0.02)
    # trim stripe
    b.torus((0, 0, 0.45), 1.225, 0.02, ACCENT, "Gloss", seg=48, ring=6)
    # floor skirt
    b.cylinder((0, 0, 0.035), 1.12, 0.06, "#111114", "Matte", verts=40)
    # "dog" ear sensors
    for s in (1, -1):
        b.box((0.35, 0.55 * s, 0.92), (0.22, 0.12, 0.3), BODY_TOP, "Gloss", bevel=0.04, rot=(12 * s, -10, 0))
        b.box((0.36, 0.55 * s, 0.94), (0.14, 0.13, 0.2), "#e3a0a0", "Matte", bevel=0.03, rot=(12 * s, -10, 0))
    # front face plate with eyes (eyes are a separate material slot for the eye light)
    b.box((1.12, 0, 0.56), (0.12, 0.95, 0.3), "#0f1013", "Gloss", bevel=0.05, rot=(0, 0, 0))
    for s in (1, -1):
        b.sphere((1.19, 0.22 * s, 0.58), 0.12, W, "Eyes", subdiv=2, scale=(0.45, 1, 1))
        b.sphere((1.215, 0.2 * s, 0.56), 0.05, "#000000", "Gloss", subdiv=1, scale=(0.3, 1, 1))
        # angry brows
        b.box((1.2, 0.22 * s, 0.73), (0.05, 0.22, 0.04), ACCENT, "Gloss", rot=(-18 * s, 0, 0))
    # intake mouth (dark slot under the bumper) with teeth bristles
    b.box((1.1, 0, 0.12), (0.18, 1.0, 0.12), "#050505", "Matte", bevel=0.03)
    for i in range(9):
        y = -0.4 + i * 0.1
        b.box((1.17, y, 0.16), (0.04, 0.03, 0.06), "#e9e5da", "Gloss", bevel=0.008, segments=1)
    # wheels
    for s in (1, -1):
        b.cylinder((0.0, 0.98 * s, 0.12), 0.13, 0.12, "#151515", "Matte", verts=16, rot=(90, 0, 0))
    # rear dust bin housing
    b.box((-1.06, 0, 0.36), (0.26, 0.9, 0.46), "#6a7383", "Glass", bevel=0.05)
    # antenna
    b.cylinder((-0.55, 0.0, 0.9), 0.02, 0.22, TRIM, "Metal", verts=8)
    b.sphere((-0.55, 0.0, 1.02), 0.05, "#ff3b2f", "Emissive", subdiv=1)
    o = b.finish(); new_material_slots_order(o, ["Gloss", "Metal", "Matte", "Eyes", "Glass", "Emissive"]); out.append(o)

    # --- Bumper (front arc)
    b = Builder("SM_Boss_Bumper")
    seg = 24
    for i in range(seg):
        a0 = math.radians(-75 + 150 * i / seg)
        a1 = math.radians(-75 + 150 * (i + 1) / seg)
        mid = (a0 + a1) / 2
        r = 1.25
        b.box((math.cos(mid) * r, math.sin(mid) * r, 0.32), (0.09, 2 * r * math.sin((a1 - a0) / 2) + 0.01, 0.28), BUMPER, "Matte", bevel=0.03, rot=(0, 0, math.degrees(mid)))
    out.append(b.finish())

    # --- Hatch (pivot at rear edge, extends +X)
    b = Builder("SM_Boss_Hatch")
    b.box((0.4, 0, 0.02), (0.8, 0.74, 0.04), BODY_TOP, "Metal", bevel=0.015)
    b.box((0.72, 0, 0.06), (0.08, 0.3, 0.05), ACCENT, "Gloss", bevel=0.01)
    out.append(b.finish())

    # --- Ramp (pivot at hinge, extends +X, 2.3 m)
    b = Builder("SM_Boss_Ramp")
    b.box((1.15, 0, -0.02), (2.3, 0.62, 0.05), "#8c8f96", "Metal", bevel=0.01)
    for i in range(9):
        b.box((0.2 + i * 0.24, 0, 0.012), (0.04, 0.56, 0.02), "#30333a", "Matte")
    out.append(b.finish())

    # --- Side brush (3 bristle arms), pivot at center
    b = Builder("SM_Boss_Brush")
    b.cylinder((0, 0, 0.03), 0.08, 0.06, "#2a2a2a", "Matte", verts=12)
    for i in range(3):
        a = i * 120
        b.box((0.22 * math.cos(math.radians(a)), 0.22 * math.sin(math.radians(a)), 0.02), (0.36, 0.05, 0.02), "#e0d7c8", "Matte", rot=(0, 0, a))
        for k in range(5):
            r = 0.1 + k * 0.07
            b.box((r * math.cos(math.radians(a)), r * math.sin(math.radians(a)), 0.0), (0.02, 0.02, 0.05), "#f2ebe0", "Matte", rot=(0, 0, a))
    out.append(b.finish())

    # --- Battery cell (AA style, 0.18 tall, pivot at bottom)
    b = Builder("SM_Boss_Cell")
    b.cylinder((0, 0, 0.09), 0.055, 0.18, "#2f8f45", "Gloss", verts=16, bevel=0.008)
    b.cylinder((0, 0, 0.14), 0.056, 0.05, "#d8d8d8", "Metal", verts=16)
    b.cylinder((0, 0, 0.19), 0.022, 0.02, "#d8d8d8", "Metal", verts=10)
    out.append(b.finish())

    # --- Dust bin lever (pivot at base)
    b = Builder("SM_Boss_Lever")
    b.box((0, 0, 0.0), (0.08, 0.12, 0.08), "#2a2a2a", "Matte", bevel=0.01)
    b.cylinder((-0.08, 0, 0.12), 0.025, 0.24, "#c33a2a", "Gloss", verts=10, rot=(0, 30, 0))
    b.sphere((-0.14, 0, 0.23), 0.05, "#e6492f", "Gloss", subdiv=1)
    out.append(b.finish())

    # --- Rear bin door
    b = Builder("SM_Boss_BinDoor")
    b.box((0, 0, 0), (0.06, 0.86, 0.42), "#8a93a3", "Metal", bevel=0.02)
    b.box((-0.04, 0, 0.1), (0.03, 0.4, 0.06), "#222", "Matte")
    out.append(b.finish())

    for o in out:
        export_static(o, o.name + ".fbx", sub="Boss")
    return out


if __name__ == "__main__":
    clear_scene()
    build_fx()
    build_boss()
