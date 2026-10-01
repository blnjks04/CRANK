"""Crank low-poly tin-toy modeling kit for Blender (5.x).

Units: meters (1 m in Blender = 100 uu in Unreal).  Facing: +X is "front" (Unreal +X).
Left side of characters is +Y in Blender (becomes -Y = left in Unreal after FBX conversion).

Materials are slot *names* that Unreal maps to its own materials:
  Paint (player color tin), Metal (vertex-colored metal), Matte (vertex-colored diffuse),
  Gloss (vertex-colored glossy enamel / plastic), Brass, Glass, Emissive, Eyes, Cloth, Wood
"""
import bpy
import bmesh
import math
import os
from mathutils import Vector, Matrix, Quaternion, Euler

EXPORT_ROOT = r"D:\Game\CRANK\ArtSource\Export"

MAT_COLORS = {
    "Paint": (0.8, 0.15, 0.1, 1),
    "Metal": (0.75, 0.75, 0.78, 1),
    "Matte": (0.6, 0.6, 0.6, 1),
    "Gloss": (0.9, 0.9, 0.9, 1),
    "Brass": (0.85, 0.65, 0.25, 1),
    "Glass": (0.6, 0.8, 1.0, 0.3),
    "Emissive": (1.0, 0.9, 0.5, 1),
    "Eyes": (0.2, 1.0, 0.3, 1),
    "Cloth": (0.9, 0.9, 0.9, 1),
    "Wood": (0.55, 0.35, 0.2, 1),
}


def srgb(hex_or_tuple):
    """'#rrggbb' -> linear-ish RGBA tuple for vertex colors (byte colors are sRGB)."""
    if isinstance(hex_or_tuple, str):
        h = hex_or_tuple.lstrip("#")
        if len(h) == 3:
            h = "".join(c * 2 for c in h)
        return (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0, 1.0)
    if len(hex_or_tuple) == 3:
        return (hex_or_tuple[0], hex_or_tuple[1], hex_or_tuple[2], 1.0)
    return tuple(hex_or_tuple)


def get_material(name):
    m = bpy.data.materials.get(name)
    if not m:
        m = bpy.data.materials.new(name)
        m.diffuse_color = MAT_COLORS.get(name, (0.8, 0.8, 0.8, 1))
    return m


def clear_scene():
    if bpy.context.object and bpy.context.object.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for coll in (bpy.data.meshes, bpy.data.armatures, bpy.data.actions, bpy.data.curves):
        for block in list(coll):
            if block.users == 0:
                coll.remove(block)


class Builder:
    """Accumulates primitives into one bmesh with per-corner colors and material slots."""

    def __init__(self, name):
        self.name = name
        self.bm = bmesh.new()
        self.col = self.bm.loops.layers.color.new("Col")
        self.uv = self.bm.loops.layers.uv.new("UVMap")
        self.mats = []
        self.group_ids = {}  # vertex group name -> id (stored in a persistent int layer)
        self.glayer = self.bm.verts.layers.int.new("crank_group")

    # ---------------------------------------------------------------- helpers
    def mat_index(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def _finish_geom(self, geom_faces, color, mat, group=None, smooth=True, sharp_angle=38.0):
        idx = self.mat_index(mat)
        c = srgb(color)
        verts = set()
        for f in geom_faces:
            f.material_index = idx
            f.smooth = smooth
            for loop in f.loops:
                loop[self.col] = c
            verts.update(f.verts)
        # sharp edges by angle
        for f in geom_faces:
            for e in f.edges:
                if len(e.link_faces) == 2:
                    a = e.link_faces[0].normal.angle(e.link_faces[1].normal, 0.0)
                    if math.degrees(a) > sharp_angle:
                        e.smooth = False
        if group:
            gid = self.group_ids.setdefault(group, len(self.group_ids) + 1)
            for v in verts:
                v[self.glayer] = gid
        return geom_faces

    @staticmethod
    def _transform(bm, verts, loc, rot):
        m = Matrix.Translation(Vector(loc))
        if rot is not None:
            if isinstance(rot, (tuple, list)) and len(rot) == 3:
                m = m @ Euler([math.radians(a) for a in rot], 'XYZ').to_matrix().to_4x4()
            else:
                m = m @ rot.to_matrix().to_4x4()
        bmesh.ops.transform(bm, matrix=m, verts=list(verts))

    def _new_faces_since(self, before):
        self.bm.faces.ensure_lookup_table()
        return [f for f in self.bm.faces if f.index >= before or f.index == -1]

    def _mark(self):
        self.bm.faces.index_update()
        return len(self.bm.faces)

    # ---------------------------------------------------------------- primitives
    def box(self, loc, size, color, mat="Matte", bevel=0.0, rot=None, group=None, segments=2):
        """Axis aligned (then rotated) box. size = full extents (x, y, z)."""
        before = set(self.bm.faces)
        res = bmesh.ops.create_cube(self.bm, size=1.0)
        verts = res["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(size), verts=verts)
        faces = [f for f in self.bm.faces if f not in before]
        if bevel > 0:
            edges = list({e for f in faces for e in f.edges})
            bmesh.ops.bevel(self.bm, geom=edges + verts, offset=bevel, segments=segments, profile=0.5, affect='EDGES', clamp_overlap=True)
            faces = [f for f in self.bm.faces if f not in before]
            verts = list({v for f in faces for v in f.verts})
        self._transform(self.bm, verts, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=bevel > 0)

    def cylinder(self, loc, radius, depth, color, mat="Matte", verts=16, radius2=None, rot=None, group=None, bevel=0.0, smooth=True, caps=True):
        """Cylinder along Z centered at loc (rotate with rot)."""
        before = set(self.bm.faces)
        r2 = radius if radius2 is None else radius2
        res = bmesh.ops.create_cone(self.bm, cap_ends=caps, cap_tris=False, segments=verts, radius1=radius, radius2=r2, depth=depth)
        vs = res["verts"]
        faces = [f for f in self.bm.faces if f not in before]
        if bevel > 0:
            cap_edges = [e for e in {e for f in faces for e in f.edges} if len(e.link_faces) == 2 and abs(e.link_faces[0].normal.dot(e.link_faces[1].normal)) < 0.3]
            bmesh.ops.bevel(self.bm, geom=cap_edges, offset=bevel, segments=2, profile=0.5, affect='EDGES', clamp_overlap=True)
            faces = [f for f in self.bm.faces if f not in before]
            vs = list({v for f in faces for v in f.verts})
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=smooth)

    def sphere(self, loc, radius, color, mat="Matte", subdiv=2, scale=(1, 1, 1), rot=None, group=None, smooth=True):
        before = set(self.bm.faces)
        res = bmesh.ops.create_icosphere(self.bm, subdivisions=subdiv, radius=radius)
        vs = res["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(scale), verts=vs)
        faces = [f for f in self.bm.faces if f not in before]
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=smooth, sharp_angle=60)

    def uvsphere(self, loc, radius, color, mat="Matte", segs=16, rings=8, scale=(1, 1, 1), rot=None, group=None):
        before = set(self.bm.faces)
        res = bmesh.ops.create_uvsphere(self.bm, u_segments=segs, v_segments=rings, radius=radius)
        vs = res["verts"]
        bmesh.ops.scale(self.bm, vec=Vector(scale), verts=vs)
        faces = [f for f in self.bm.faces if f not in before]
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=True, sharp_angle=70)

    def torus(self, loc, major, minor, color, mat="Matte", seg=24, ring=8, rot=None, group=None):
        before = set(self.bm.faces)
        verts_grid = []
        for i in range(seg):
            a = 2 * math.pi * i / seg
            row = []
            for j in range(ring):
                b = 2 * math.pi * j / ring
                r = major + minor * math.cos(b)
                row.append(self.bm.verts.new((r * math.cos(a), r * math.sin(a), minor * math.sin(b))))
            verts_grid.append(row)
        for i in range(seg):
            for j in range(ring):
                v1 = verts_grid[i][j]
                v2 = verts_grid[(i + 1) % seg][j]
                v3 = verts_grid[(i + 1) % seg][(j + 1) % ring]
                v4 = verts_grid[i][(j + 1) % ring]
                self.bm.faces.new((v1, v2, v3, v4))
        faces = [f for f in self.bm.faces if f not in before]
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        vs = [v for row in verts_grid for v in row]
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=True, sharp_angle=80)

    def lathe(self, profile, color, mat="Matte", seg=20, loc=(0, 0, 0), rot=None, group=None, close_top=True, close_bottom=True, sharp_angle=38.0):
        """Revolve a (radius, z) profile around Z."""
        before = set(self.bm.faces)
        rings = []
        for (r, z) in profile:
            ring = []
            for i in range(seg):
                a = 2 * math.pi * i / seg
                ring.append(self.bm.verts.new((r * math.cos(a), r * math.sin(a), z)))
            rings.append(ring)
        for k in range(len(rings) - 1):
            for i in range(seg):
                a, b = rings[k][i], rings[k][(i + 1) % seg]
                c, d = rings[k + 1][(i + 1) % seg], rings[k + 1][i]
                if a.co == d.co and b.co == c.co:
                    continue
                try:
                    self.bm.faces.new((a, b, c, d))
                except ValueError:
                    pass
        if close_bottom and profile[0][0] > 1e-4:
            self.bm.faces.new(list(reversed(rings[0])))
        if close_top and profile[-1][0] > 1e-4:
            self.bm.faces.new(rings[-1])
        faces = [f for f in self.bm.faces if f not in before]
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        vs = [v for ring in rings for v in ring]
        bmesh.ops.remove_doubles(self.bm, verts=vs, dist=1e-5)
        faces = [f for f in self.bm.faces if f not in before]
        vs = list({v for f in faces for v in f.verts})
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=True, sharp_angle=sharp_angle)

    def prism(self, points2d, depth, color, mat="Matte", loc=(0, 0, 0), rot=None, group=None, bevel=0.0, smooth=False):
        """Extrude a 2D polygon (x, y) along Z by depth (centered)."""
        before = set(self.bm.faces)
        bottom = [self.bm.verts.new((x, y, -depth / 2)) for (x, y) in points2d]
        top = [self.bm.verts.new((x, y, depth / 2)) for (x, y) in points2d]
        n = len(points2d)
        self.bm.faces.new(list(reversed(bottom)))
        self.bm.faces.new(top)
        for i in range(n):
            self.bm.faces.new((bottom[i], bottom[(i + 1) % n], top[(i + 1) % n], top[i]))
        faces = [f for f in self.bm.faces if f not in before]
        bmesh.ops.recalc_face_normals(self.bm, faces=faces)
        vs = bottom + top
        if bevel > 0:
            edges = list({e for f in faces for e in f.edges})
            bmesh.ops.bevel(self.bm, geom=edges, offset=bevel, segments=1, profile=0.5, affect='EDGES', clamp_overlap=True)
            faces = [f for f in self.bm.faces if f not in before]
            vs = list({v for f in faces for v in f.verts})
        self._transform(self.bm, vs, loc, rot)
        return self._finish_geom(faces, color, mat, group, smooth=smooth)

    def tube(self, path, radius, color, mat="Metal", seg=10, group=None):
        """Pipe along a polyline of points."""
        faces_all = []
        for i in range(len(path) - 1):
            a, b = Vector(path[i]), Vector(path[i + 1])
            d = b - a
            length = d.length
            rot = d.to_track_quat('Z', 'Y')
            mid = (a + b) / 2
            faces_all += self.cylinder(tuple(mid), radius, length, color, mat, verts=seg, rot=rot, group=group)
            faces_all += self.sphere(tuple(b), radius, color, mat, subdiv=1, group=group)
        return faces_all

    # ---------------------------------------------------------------- output
    def uv_project(self, scale=1.0):
        for f in self.bm.faces:
            n = f.normal
            ax = max(range(3), key=lambda i: abs(n[i]))
            for loop in f.loops:
                co = loop.vert.co
                if ax == 0:
                    uv = (co.y, co.z)
                elif ax == 1:
                    uv = (co.x, co.z)
                else:
                    uv = (co.x, co.y)
                loop[self.uv].uv = (uv[0] * scale, uv[1] * scale)

    def finish(self, collection=None):
        # Coplanar trims / panels flicker (z-fighting): push them a few millimeters out of their base.
        import zfight
        zfight.fix_bmesh(self.bm)
        self.uv_project()
        mesh = bpy.data.meshes.new(self.name)
        self.bm.normal_update()
        self.bm.verts.index_update()
        id_to_name = {gid: name for name, gid in self.group_ids.items()}
        group_indices = {name: [] for name in self.group_ids}
        for v in self.bm.verts:
            gid = v[self.glayer]
            if gid in id_to_name:
                group_indices[id_to_name[gid]].append(v.index)
        self.bm.verts.layers.int.remove(self.glayer)
        for m in self.mats:
            mesh.materials.append(get_material(m))
        self.bm.to_mesh(mesh)
        self.bm.free()
        if "Col" in mesh.color_attributes:
            attr = mesh.color_attributes["Col"]
            mesh.color_attributes.active_color = attr
            try:
                mesh.color_attributes.render_color_index = list(mesh.color_attributes).index(attr)
            except Exception:
                pass
        obj = bpy.data.objects.new(self.name, mesh)
        (collection or bpy.context.scene.collection).objects.link(obj)
        for gname, indices in group_indices.items():
            vg = obj.vertex_groups.new(name=gname)
            vg.add(indices, 1.0, 'REPLACE')
        return obj


def select_only(objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs:
        o.select_set(True)
    if objs:
        bpy.context.view_layer.objects.active = objs[0]


def export_static(obj_or_objs, filename, sub="Props", mirror_y=True):
    """Export static meshes. Unreal's FBX import flips Y (Blender +Y -> UE -Y); mirror_y pre-flips the
    geometry so Unreal ends up with exactly the Blender coordinates (layout authored in UE axes)."""
    objs = obj_or_objs if isinstance(obj_or_objs, (list, tuple)) else [obj_or_objs]
    path = os.path.join(EXPORT_ROOT, sub, filename)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temps = []
    if mirror_y:
        for o in objs:
            c = o.copy()
            c.data = o.data.copy()
            bpy.context.scene.collection.objects.link(c)
            c.location = (o.location.x, -o.location.y, o.location.z)
            c.scale = (o.scale.x, -o.scale.y, o.scale.z)
            temps.append(c)
        select_only(temps)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        export_objs = temps
    else:
        export_objs = objs
    select_only(export_objs)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={'MESH'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_ALL', axis_forward='-Z', axis_up='Y',
        mesh_smooth_type='FACE', use_mesh_modifiers=True, add_leaf_bones=False, bake_anim=False,
        colors_type='SRGB', use_triangles=False)
    for c in temps:
        mesh = c.data
        bpy.data.objects.remove(c, do_unlink=True)
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    return path


def new_material_slots_order(obj, order):
    """Reorder material slots so that 'order' names come first (slot 0 = order[0])."""
    mesh = obj.data
    names = [m.name for m in mesh.materials]
    final = [n for n in order if n in names] + [n for n in names if n not in order]
    remap = {names.index(n): final.index(n) for n in names}
    indices = [remap[p.material_index] for p in mesh.polygons]
    # Assign slots in place (clearing the slot list would reset polygon indices).
    for i, n in enumerate(final):
        mesh.materials[i] = get_material(n)
    for p, idx in zip(mesh.polygons, indices):
        p.material_index = idx
