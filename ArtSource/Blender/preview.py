"""Render quick preview images of the current scene (Workbench, vertex colors)."""
import bpy, math, os
from mathutils import Vector

PREVIEW_DIR = r"D:\Game\CRANK\ArtSource\Preview"


def contact_sheet(paths, out_name, cols=3):
    """Combine PNGs (same size) into one grid image using numpy (Blender bundles it)."""
    import numpy as np
    imgs = []
    for p in paths:
        img = bpy.data.images.load(p, check_existing=False)
        w, h = img.size
        arr = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
        imgs.append(arr)
        bpy.data.images.remove(img)
    h, w = imgs[0].shape[:2]
    rows = (len(imgs) + cols - 1) // cols
    sheet = np.zeros((rows * h, cols * w, 4), dtype=np.float32)
    sheet[..., 3] = 1.0
    for i, arr in enumerate(imgs):
        r, c = divmod(i, cols)
        # images are stored bottom-up; place rows from the top
        y0 = (rows - 1 - r) * h
        sheet[y0:y0 + h, c * w:(c + 1) * w] = arr[:h, :w]
    out = bpy.data.images.new(out_name, cols * w, rows * h, alpha=True)
    out.pixels = sheet.ravel()
    path = os.path.join(PREVIEW_DIR, out_name + ".png")
    out.filepath_raw = path
    out.file_format = 'PNG'
    out.save()
    bpy.data.images.remove(out)
    return path


def render_preview(name, target=(0, 0, 0.5), distance=2.4, yaw=35.0, pitch=15.0, res=640, lens=50, ortho=False, ortho_scale=2.0, objects=None):
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    scene = bpy.context.scene
    cam_data = bpy.data.cameras.get("PreviewCam") or bpy.data.cameras.new("PreviewCam")
    cam = bpy.data.objects.get("PreviewCam")
    if not cam:
        cam = bpy.data.objects.new("PreviewCam", cam_data)
        scene.collection.objects.link(cam)
    cam_data.lens = lens
    cam_data.type = 'ORTHO' if ortho else 'PERSP'
    cam_data.ortho_scale = ortho_scale
    cam_data.clip_end = max(1000.0, distance * 6.0)
    cam_data.clip_start = max(0.01, distance * 0.001)
    t = Vector(target)
    y, p = math.radians(yaw), math.radians(pitch)
    pos = t + Vector((math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))) * distance
    cam.location = pos
    direction = t - pos
    cam.rotation_euler = direction.to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam

    scene.render.engine = 'BLENDER_WORKBENCH'
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.color_type = 'VERTEX'
    shading.show_shadows = True
    shading.show_cavity = True
    shading.cavity_type = 'BOTH'
    scene.render.resolution_x = res
    scene.render.resolution_y = res
    scene.render.film_transparent = False
    scene.display.render_aa = '8'
    hidden = []
    if objects is not None:
        for o in scene.objects:
            if o.type in ('MESH',) and o not in objects and not o.hide_render:
                o.hide_render = True
                hidden.append(o)
    path = os.path.join(PREVIEW_DIR, name + ".png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    for o in hidden:
        o.hide_render = False
    return path
