# Shared Blender helpers for Stage 3A.7 meshes. Geometry is authored in Unreal axes (X along, Y across, Z up, metres);
# the Blender vertex is (X, -Y, Z), which the FBX export / Unreal import maps back to (X, Y, Z).
import bpy, bmesh, os
from mathutils import Vector

ASSETS = r"C:\Users\niles\Documents\Blender Projects\P9_Assets"

def new_file(family, name):
    path = os.path.join(ASSETS, family, name)
    if os.path.exists(path): raise SystemExit("ABORT: exists " + path)
    if bpy.data.is_dirty: raise SystemExit("ABORT: current Blender file has unsaved changes")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=path)
    for coll in (bpy.data.objects, bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.lights, bpy.data.cameras):
        for b in list(coll): coll.remove(b)
    return path

class Builder:
    def __init__(self):
        self.bm = bmesh.new(); self.uv = self.bm.loops.layers.uv.new("UVMap"); self.vc = {}
    def V(self, x, y, z):
        k = (round(x, 5), round(y, 5), round(z, 5))
        if k not in self.vc: self.vc[k] = self.bm.verts.new((x, -y, z))
        return self.vc[k]
    def quad(self, pts, uvs, normal_ue, mi):
        """pts/normal in Unreal axes; uvs per point. Winding is fixed so the face normal matches normal_ue."""
        n = Vector((normal_ue[0], -normal_ue[1], normal_ue[2]))
        vs = [self.V(*p) for p in pts]
        f = self.bm.faces.new(vs); f.normal_update()
        if f.normal.dot(n) < 0:
            f.normal_flip()
        f.material_index = mi
        for l in f.loops: l[self.uv].uv = uvs[vs.index(l.vert)]
        return f
    def box(self, x0, x1, y0, y1, z0, z1, mi, skip=()):
        """Closed box with simple metre-scale UVs; 'skip' lists faces to leave out: '+x','-x','+y','-y','+z','-z'."""
        F = {"+x": ([(x1, y0, z0), (x1, y1, z0), (x1, y1, z1), (x1, y0, z1)], (1, 0, 0), lambda p: (p[1], p[2])),
             "-x": ([(x0, y0, z0), (x0, y1, z0), (x0, y1, z1), (x0, y0, z1)], (-1, 0, 0), lambda p: (p[1], p[2])),
             "+y": ([(x0, y1, z0), (x1, y1, z0), (x1, y1, z1), (x0, y1, z1)], (0, 1, 0), lambda p: (p[0], p[2])),
             "-y": ([(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)], (0, -1, 0), lambda p: (p[0], p[2])),
             "+z": ([(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], (0, 0, 1), lambda p: (p[0], p[1])),
             "-z": ([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)], (0, 0, -1), lambda p: (p[0], p[1]))}
        for k, (pts, n, f) in F.items():
            if k not in skip: self.quad(pts, [f(p) for p in pts], n, mi)
    def rod(self, x, y, z0, z1, r=0.006, mi=0):
        self.box(x - r, x + r, y - r, y + r, z0, z1, mi, skip=("+z",))
    def finish(self, name, mats):
        me = bpy.data.meshes.new(name); self.bm.to_mesh(me); self.bm.free()
        for m in mats: me.materials.append(m)
        ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
        me.calc_loop_triangles()
        bb = [Vector(c) for c in ob.bound_box]
        print(f"{name}: tris {len(me.loop_triangles)} verts {len(me.vertices)} slots {[m.name for m in me.materials]} "
              f"faces/slot {[sum(1 for p in me.polygons if p.material_index == i) for i in range(len(mats))]} | "
              f"UE bounds X {min(v.x for v in bb):.3f}..{max(v.x for v in bb):.3f} Y {-max(v.y for v in bb):.3f}..{-min(v.y for v in bb):.3f} "
              f"Z {min(v.z for v in bb):.3f}..{max(v.z for v in bb):.3f}")
        return ob

def mat(name, rgb=(0.5, 0.5, 0.5), tex=None):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    b.inputs["Base Color"].default_value = (*rgb, 1); m.diffuse_color = (*rgb, 1)
    if tex:
        t = m.node_tree.nodes.new("ShaderNodeTexImage"); t.image = bpy.data.images.load(tex)
        m.node_tree.links.new(t.outputs["Color"], b.inputs["Base Color"])
        if t.image.channels == 4:
            m.node_tree.links.new(t.outputs["Alpha"], b.inputs["Alpha"]); m.blend_method = "CLIP" if hasattr(m, "blend_method") else None
    return m

def export(family, objs, fbx_name):
    out = os.path.join(ASSETS, family, "Export", fbx_name)
    if os.path.exists(out): raise SystemExit("ABORT: exists " + out)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    for o in bpy.context.selected_objects: o.select_set(False)
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    r = bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"MESH"}, global_scale=1.0, apply_unit_scale=True,
                                 apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Z", axis_up="Y", mesh_smooth_type="FACE",
                                 use_tspace=False, add_leaf_bones=False, bake_anim=False, embed_textures=False, path_mode="AUTO",
                                 batch_mode="OFF")
    print("exported", out, r, os.path.getsize(out))
    return out

import math
def prism(b, p0, p1, r, mi, sides=6, caps=True):
    """Tube of radius r between two points given in Unreal axes (metres)."""
    p0, p1 = Vector(p0), Vector(p1); d = (p1 - p0); L = d.length; d.normalize()
    ref = Vector((0, 0, 1)) if abs(d.z) < 0.9 else Vector((1, 0, 0))
    u = d.cross(ref).normalized(); w = d.cross(u).normalized()
    ring = lambda c: [c + (u * math.cos(2 * math.pi * k / sides) + w * math.sin(2 * math.pi * k / sides)) * r for k in range(sides)]
    A, B = ring(p0), ring(p1)
    for k in range(sides):
        j = (k + 1) % sides
        n = (u * math.cos(2 * math.pi * (k + 0.5) / sides) + w * math.sin(2 * math.pi * (k + 0.5) / sides))
        b.quad([tuple(A[k]), tuple(A[j]), tuple(B[j]), tuple(B[k])], [(k / sides, 0), ((k + 1) / sides, 0), ((k + 1) / sides, L), (k / sides, L)], tuple(n), mi)
    if caps:
        for ring_pts, n in ((A, -d), (B, d)):
            c = sum(ring_pts, Vector()) / sides
            for k in range(sides):
                j = (k + 1) % sides
                vs = [b.V(*c), b.V(*ring_pts[k]), b.V(*ring_pts[j])]
                f = b.bm.faces.new(vs); f.normal_update()
                if f.normal.dot(Vector((n.x, -n.y, n.z))) < 0: f.normal_flip()
                f.material_index = mi
                for l in f.loops: l[b.uv].uv = (0.5, 0.5)

def ucx_box(name, x0, x1, y0, y1, z0, z1):
    """Convex collision proxy (Unreal UCX naming) in Unreal axes."""
    bb = Builder(); bb.box(x0, x1, y0, y1, z0, z1, 0)
    me = bpy.data.meshes.new(name); bb.bm.to_mesh(me); bb.bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); ob.display_type = "WIRE"
    return ob
