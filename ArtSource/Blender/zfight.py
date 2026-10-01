"""Z-fighting guard for the procedural props / architecture.

The builders stack boxes, bands, panels and trims as separate loose parts of one mesh. Wherever two parts end up with
faces in the same plane, facing the same way and overlapping (a strap flush with the crate side, a floor panel flush
with the plinth top, two wall boxes sharing a corner...), the GPU cannot tell which one is in front and the surface
flickers with the camera jitter. `fix_bmesh` finds those faces and pushes the smaller one (the trim / decoration;
the later part when both are the same size) out by a few millimeters, so it ends up visibly on top.
"""
import math
from collections import defaultdict
from mathutils import Vector

PLANE_EPS = 0.0015     # m: faces closer than this to the same plane count as coplanar
MIN_AREA = 0.00005     # m^2: ignore slivers below 0.5 cm^2
MIN_WIDTH = 0.002      # m: overlaps thinner than 2 mm are edge contacts
PUSH = 0.003           # m: largest step a part grows / moves by (0.3 cm in Unreal)


def _components(bm):
    parent = list(range(len(bm.verts)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for e in bm.edges:
        a, b = find(e.verts[0].index), find(e.verts[1].index)
        if a != b:
            parent[max(a, b)] = min(a, b)
    return [find(f.verts[0].index) for f in bm.faces]


def _basis(n):
    u = n.orthogonal().normalized()
    return u, n.cross(u).normalized()


def _area(poly):
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i]
        x1, y1 = poly[(i + 1) % len(poly)]
        a += x0 * y1 - x1 * y0
    return a * 0.5


def _convex_hull(pts):
    pts = sorted(set(pts))
    if len(pts) <= 2:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def _clip(subject, clip):
    """Sutherland-Hodgman: subject polygon clipped by a convex counter-clockwise polygon."""
    out = subject
    for i in range(len(clip)):
        if not out:
            break
        a, b = clip[i], clip[(i + 1) % len(clip)]
        inp, out = out, []

        def inside(p):
            return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= -1e-12

        def inter(p, q):
            x1, y1, x2, y2 = a[0], a[1], b[0], b[1]
            x3, y3, x4, y4 = p[0], p[1], q[0], q[1]
            den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
            if abs(den) < 1e-15:
                return q
            t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den
            return (x1 + t * (x2 - x1), y1 + t * (y2 - y1))

        for j in range(len(inp)):
            p, q = inp[j], inp[(j + 1) % len(inp)]
            if inside(q):
                if not inside(p):
                    out.append(inter(p, q))
                out.append(q)
            elif inside(p):
                out.append(inter(p, q))
    return out


def find_overlaps(bm):
    """([(face_a, face_b, area)], part of each face, creation order of each part) for same-facing coplanar
    overlapping faces of different parts."""
    bm.verts.index_update()
    bm.faces.index_update()
    bm.normal_update()
    comp = _components(bm)
    order = {}
    for f in bm.faces:
        order.setdefault(comp[f.index], f.index)	# creation order of each part = its first face

    buckets = defaultdict(list)
    info = {}
    for f in bm.faces:
        if f.calc_area() < MIN_AREA:
            continue
        n = f.normal.normalized()
        if n.length < 0.5:
            continue
        d = n.dot(f.verts[0].co)
        key = (round(n.x, 3), round(n.y, 3), round(n.z, 3))
        u, v = _basis(n)
        pts = [(vt.co.dot(u), vt.co.dot(v)) for vt in f.verts]
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        info[f.index] = (f, n, d, pts, (min(xs), min(ys), max(xs), max(ys)))
        buckets[(key, round(d / PLANE_EPS))].append(f.index)

    pairs = []
    seen = set()
    for (key, db), members in buckets.items():
        cands = list(members)
        for off in (-1, 1):
            cands += buckets.get((key, db + off), [])
        for i in members:
            fi, ni, di, pi, bi = info[i]
            for j in cands:
                if j == i or comp[i] == comp[j]:
                    continue
                pair = (min(i, j), max(i, j))
                if pair in seen:
                    continue
                fj, nj, dj, pj, bj = info[j]
                if abs(di - dj) > PLANE_EPS or ni.dot(nj) < 0.9999:
                    continue
                if bi[2] <= bj[0] or bj[2] <= bi[0] or bi[3] <= bj[1] or bj[3] <= bi[1]:
                    continue
                seen.add(pair)
                hull = _convex_hull(pj)
                if len(hull) < 3:
                    continue
                if _area(hull) < 0:
                    hull = hull[::-1]
                sub = pi if _area(pi) > 0 else pi[::-1]
                inter = _clip(sub, hull) if len(sub) >= 3 else []
                area = abs(_area(inter)) if len(inter) >= 3 else 0.0
                if area < MIN_AREA:
                    continue
                # Panels that only touch along an edge leave a numerical sliver: not an overlap.
                xs = [q[0] for q in inter]
                ys = [q[1] for q in inter]
                if min(max(xs) - min(xs), max(ys) - min(ys)) < MIN_WIDTH:
                    continue
                pairs.append((fi, fj, area))
    return pairs, comp, order


def _vertex_parts(bm):
    parent = list(range(len(bm.verts)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for e in bm.edges:
        a, b = find(e.verts[0].index), find(e.verts[1].index)
        if a != b:
            parent[max(a, b)] = min(a, b)
    return [find(v.index) for v in bm.verts]


def fix_bmesh(bm, push=PUSH, passes=10):
    """Lift overlapping coplanar parts apart without bending anything.

    Works per PART (loose piece): the biggest parts (base surfaces) stay; each overlapping smaller part gets the
    lowest layer not used by the parts it overlaps (a chain alternates 0 / 1 instead of stacking up) and grows by
    layer * step on every side around its own center (a strap flush with both crate sides sticks out on both), or,
    along an axis where it is (almost) flat, moves out along the faces it overlaps with. Returns parts moved."""
    moved = 0
    for _ in range(passes):
        pairs, comp, order = find_overlaps(bm)
        if not pairs:
            break
        bm.verts.ensure_lookup_table()
        vpart = _vertex_parts(bm)
        verts_of = defaultdict(list)
        for v in bm.verts:
            verts_of[vpart[v.index]].append(v)
        biggest = defaultdict(float)
        for f in bm.faces:
            biggest[comp[f.index]] = max(biggest[comp[f.index]], f.calc_area())
        adj = defaultdict(set)
        dirs = defaultdict(lambda: Vector((0.0, 0.0, 0.0)))
        for a, b, area in pairs:
            pa, pb = comp[a.index], comp[b.index]
            adj[pa].add(pb)
            adj[pb].add(pa)
            dirs[pa] += a.normal * area
            dirs[pb] += b.normal * area
        layer = {}
        for p in sorted(adj, key=lambda q: (-biggest[q], order[q], q)):
            used = {layer[o] for o in adj[p] if o in layer}
            k = 0
            while k in used:
                k += 1
            layer[p] = k
        for p, k in layer.items():
            if k == 0 or p not in verts_of:
                continue
            vs = verts_of[p]
            step = min(push, max(0.0008, 0.01 * math.sqrt(biggest[p])))
            g = step * k
            lo = Vector((min(v.co.x for v in vs), min(v.co.y for v in vs), min(v.co.z for v in vs)))
            hi = Vector((max(v.co.x for v in vs), max(v.co.y for v in vs), max(v.co.z for v in vs)))
            c = (lo + hi) * 0.5
            size = hi - lo
            d = dirs[p].normalized() if dirs[p].length > 1e-9 else Vector((0.0, 0.0, 1.0))
            for v in vs:
                co = v.co.copy()
                for ax in range(3):
                    if size[ax] > 4.0 * g:
                        co[ax] = c[ax] + (co[ax] - c[ax]) * (size[ax] + 2.0 * g) / size[ax]
                    else:
                        co[ax] += d[ax] * g
                v.co = co
            moved += 1
        bm.normal_update()
    return moved


def report(obj):
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    pairs, _, _ = find_overlaps(bm)
    total = sum(a for _, _, a in pairs)
    bm.free()
    return len(pairs), total
