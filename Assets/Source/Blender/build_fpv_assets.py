"""Baut alle 3D-Assets der FPV-Drohne prozedural, exportiert FBX und rendert Icons.

Aufruf (Blender als Python-Modul):  python3 Assets/Source/Blender/build_fpv_assets.py
Aufruf (Blender-Programm):          blender -b -P Assets/Source/Blender/build_fpv_assets.py

Konventionen (passend zum Unreal-Import):
- 1 Blender-Einheit = 1 m, Export mit FBX_SCALE_UNITS -> in Unreal 1 UU = 1 cm
- Vorne = Blender +X, Oben = +Z (landet in Unreal ebenfalls auf +X / +Z)
- Sockets sind Empties mit Praefix SOCKET_, Kollision sind Meshes mit Praefix UCX_
"""
import math
import os
import sys

import bpy  # muss vor bmesh importiert werden (bpy-Modul)
import bmesh
from mathutils import Euler, Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TEX = os.path.join(ROOT, "Export", "Textures")
OUT_MESH = os.path.join(ROOT, "Export", "Meshes")
OUT_ICON = os.path.join(ROOT, "Export", "Icons")
OUT_PREVIEW = os.path.join(ROOT, "Docs", "img")

# Rahmenmasse eines 5-Zoll-Freestyle-Quads
MOTOR_DIAG = 0.225          # Motor-zu-Motor diagonal
MOTOR_OFF = MOTOR_DIAG / 2 / math.sqrt(2)
PROP_R = 0.0635             # 5 Zoll Durchmesser
PROP_PITCH = 0.109          # 4.3 Zoll Steigung
ARM_T = 0.005               # Armdicke
MOTOR_H = 0.014
PROP_Z = ARM_T + MOTOR_H + 0.002
COG_Z = 0.028               # Pivot des Rahmens ~ Schwerpunkt (mit Akku)
CAM_UPTILT = 30.0


# --------------------------------------------------------------------------- Szene

def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.unit_settings.system = "METRIC"
    sc.unit_settings.scale_length = 1.0


def link(obj):
    bpy.context.scene.collection.objects.link(obj)
    return obj


def apply_modifiers(obj):
    dg = bpy.context.evaluated_depsgraph_get()
    mesh = bpy.data.meshes.new_from_object(obj.evaluated_get(dg))
    old = obj.data
    obj.modifiers.clear()
    obj.data = mesh
    bpy.data.meshes.remove(old)


def apply_transform(obj):
    obj.data.transform(obj.matrix_basis)
    obj.matrix_basis = Matrix.Identity(4)


# --------------------------------------------------------------------------- Materialien

def _img(name, non_color=False):
    path = os.path.join(TEX, name)
    if not os.path.exists(path):
        return None
    img = bpy.data.images.load(path, check_existing=True)
    if non_color:
        img.colorspace_settings.name = "Non-Color"
    return img


def make_material(name, color=(0.5, 0.5, 0.5), rough=0.5, metal=0.0, bc_tex=None,
                  orm_tex=None, emission=None, transmission=0.0, alpha_tex=None):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*color, 1)
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    if transmission:
        bsdf.inputs["Transmission Weight"].default_value = transmission
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*emission[0], 1)
        bsdf.inputs["Emission Strength"].default_value = emission[1]
    img = _img(bc_tex) if bc_tex else None
    if img:
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = img
        if color != (1, 1, 1):
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 1.0
            nt.links.new(n.outputs["Color"], mix.inputs[6])
            mix.inputs[7].default_value = (*color, 1)
            nt.links.new(mix.outputs[2], bsdf.inputs["Base Color"])
        else:
            nt.links.new(n.outputs["Color"], bsdf.inputs["Base Color"])
    img = _img(orm_tex, True) if orm_tex else None
    if img:
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = img
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(n.outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
    img = _img(alpha_tex) if alpha_tex else None
    if img:
        n = nt.nodes.new("ShaderNodeTexImage")
        n.image = img
        nt.links.new(n.outputs["Alpha"], bsdf.inputs["Alpha"])
        mat.surface_render_method = "BLENDED"
    return mat


def build_materials():
    return {
        "carbon": make_material("M_FPV_Carbon", (1, 1, 1), 0.3, 0.0,
                                "T_FPV_Carbon_BC.png", "T_FPV_Carbon_ORM.png"),
        "paint": make_material("M_FPV_Paint", (1, 1, 1), 0.45, 0.0,
                               "T_FPV_Paint_BC.png", "T_FPV_Paint_ORM.png"),
        "metal": make_material("M_FPV_MotorBell", (0.95, 0.42, 0.08), 0.3, 1.0,
                               "T_FPV_Brushed_BC.png", "T_FPV_Brushed_ORM.png"),
        "steel": make_material("M_FPV_Steel", (0.62, 0.62, 0.64), 0.25, 1.0),
        "plastic": make_material("M_FPV_Plastic", (0.025, 0.025, 0.028), 0.55),
        "battery": make_material("M_FPV_Battery", (0.018, 0.02, 0.025), 0.4),
        "prop": make_material("M_FPV_Prop", (0.75, 0.16, 0.01), 0.35, transmission=0.1),
        "lens": make_material("M_FPV_Lens", (0.01, 0.012, 0.02), 0.03, 0.0),
        "led": make_material("M_FPV_LED", (0.2, 0.6, 1.0), 0.3,
                             emission=((0.2, 0.6, 1.0), 25.0)),
        "screen": make_material("M_FPV_Screen", (0.02, 0.05, 0.08), 0.1,
                                emission=((0.25, 0.55, 0.9), 3.0)),
        "foam": make_material("M_FPV_Foam", (0.03, 0.03, 0.03), 0.95),
        "blur": make_material("M_FPV_PropBlur", (0.12, 0.12, 0.12), 0.6,
                              alpha_tex="T_FPV_PropBlur.png"),
    }


# --------------------------------------------------------------------------- Geometrie

def box_uv(obj, tile=0.05):
    """Box-Projektion in Objektkoordinaten, damit kachelnde Texturen gleichmaessig sitzen."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        for loop in f.loops:
            co = loop.vert.co
            if ax == 0:
                u, v = co.y, co.z
            elif ax == 1:
                u, v = co.x, co.z
            else:
                u, v = co.x, co.y
            loop[uv].uv = (u / tile, v / tile)
    bm.to_mesh(obj.data)
    bm.free()


def finish(obj, mat, tile=0.05, smooth_angle=35.0):
    apply_transform(obj)
    obj.data.materials.clear()
    obj.data.materials.append(mat)
    if not obj.data.uv_layers:
        box_uv(obj, tile)
    else:
        box_uv(obj, tile)
    obj.data.shade_smooth()
    obj.data.set_sharp_from_angle(angle=math.radians(smooth_angle))
    return obj


def rbox(name, size, loc, mat, bevel=0.0015, segs=2, rot=(0, 0, 0), tile=0.05):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    if bevel > 0:
        apply_transform(obj)
        m = obj.modifiers.new("bevel", "BEVEL")
        m.width = bevel
        m.segments = segs
        m.limit_method = "ANGLE"
        apply_modifiers(obj)
    return finish(obj, mat, tile)


def cyl(name, r, h, loc, mat, rot=(0, 0, 0), verts=24, bevel=0.0, tile=0.05):
    bpy.ops.mesh.primitive_cylinder_add(vertices=verts, radius=r, depth=h, location=loc,
                                        rotation=rot)
    obj = bpy.context.active_object
    obj.name = name
    if bevel > 0:
        apply_transform(obj)
        m = obj.modifiers.new("bevel", "BEVEL")
        m.width = bevel
        m.segments = 2
        m.limit_method = "ANGLE"
        apply_modifiers(obj)
    return finish(obj, mat, tile)


def sphere(name, r, loc, mat, scale=(1, 1, 1)):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=20, ring_count=10, radius=r, location=loc)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = scale
    return finish(obj, mat)


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = name
    obj.data.name = name
    return obj


def add_socket(name, loc, parent, rot=(0, 0, 0)):
    # Unreal entfernt das Praefix SOCKET_, der Socket heisst danach z. B. Socket_Prop_FL
    e = bpy.data.objects.new("SOCKET_Socket_" + name, None)
    e.empty_display_type = "ARROWS"
    e.empty_display_size = 0.02
    e.location = loc
    e.rotation_euler = Euler([math.radians(a) for a in rot])
    link(e)
    e.parent = parent
    return e


def add_ucx(mesh_name, idx, size, loc, rot_z=0.0):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=(0, 0, math.radians(rot_z)))
    o = bpy.context.active_object
    o.scale = size
    apply_transform(o)
    o.name = "UCX_%s_%02d" % (mesh_name, idx)
    o.display_type = "WIRE"
    o.hide_render = True
    return o


# --------------------------------------------------------------------------- Propeller

def blade_section(r, chord, thick, pitch_angle, n=10):
    """Gewoelbtes Profil; Vorderkante bei +Y (Drehrichtung CCW von oben)."""
    pts_top, pts_bot = [], []
    for i in range(n + 1):
        t = i / n                      # 0 = Hinterkante, 1 = Vorderkante
        y = (t - 0.35) * chord
        camber = 0.04 * chord * math.sin(math.pi * t)
        th = thick * (math.sqrt(max(t, 0.0)) * (1 - t) * 2.6 + 0.08)
        pts_top.append((y, camber + th / 2))
        pts_bot.append((y, camber - th / 2))
    ring = pts_top + list(reversed(pts_bot[1:-1]))
    ca, sa = math.cos(pitch_angle), math.sin(pitch_angle)
    return [Vector((r, y * ca - z * sa, y * sa + z * ca)) for (y, z) in ring]


def build_prop(name, mats, ccw=True, blades=3):
    bm = bmesh.new()
    sections = 14
    r0, r1 = 0.006, PROP_R
    for b in range(blades):
        rings = []
        for i in range(sections + 1):
            t = i / sections
            r = r0 + (r1 - r0) * t
            chord = (0.010 + 0.011 * math.sin(math.pi * min(t * 1.25, 1.0))) * \
                (math.sqrt(max(0.0, 1 - ((t - 0.75) / 0.25) ** 2)) if t > 0.75 else 1.0)
            chord = max(chord, 0.0025)
            thick = 0.0022 * (1 - 0.6 * t)
            ang = math.atan(PROP_PITCH / (2 * math.pi * max(r, 0.012)))
            ring = blade_section(r, chord, thick, ang)
            rings.append([bm.verts.new(v) for v in ring])
        for a, b2 in zip(rings[:-1], rings[1:]):
            k = len(a)
            for j in range(k):
                bm.faces.new((a[j], a[(j + 1) % k], b2[(j + 1) % k], b2[j]))
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
        rot = Matrix.Rotation(2 * math.pi * b / blades, 4, "Z")
        verts = [v for ring in rings for v in ring]
        bmesh.ops.transform(bm, matrix=rot, verts=verts)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name + "_blades")
    bm.to_mesh(mesh)
    bm.free()
    blades_obj = link(bpy.data.objects.new(name + "_blades", mesh))
    blades_obj.location.z = 0.0045
    finish(blades_obj, mats["prop"], tile=0.03, smooth_angle=60)
    hub = cyl(name + "_hub", 0.0065, 0.008, (0, 0, 0.004), mats["prop"], verts=24, bevel=0.0008)
    nut = cyl(name + "_nut", 0.0042, 0.005, (0, 0, 0.0105), mats["steel"], verts=6, bevel=0.0004)
    prop = join([blades_obj, hub, nut], name)
    if not ccw:
        prop.scale.y = -1
        apply_transform(prop)
        prop.data.flip_normals()
    return prop


def build_prop_blur(mats):
    bpy.ops.mesh.primitive_circle_add(vertices=48, radius=PROP_R, fill_type="NGON",
                                      location=(0, 0, 0.0045))
    o = bpy.context.active_object
    o.name = "SM_FPVDrone_PropBlur"
    apply_transform(o)
    # planare UV 0..1 ueber den Kreis
    bm = bmesh.new()
    bm.from_mesh(o.data)
    uv = bm.loops.layers.uv.verify()
    for f in bm.faces:
        for lp in f.loops:
            lp[uv].uv = (lp.vert.co.x / (2 * PROP_R) + 0.5, lp.vert.co.y / (2 * PROP_R) + 0.5)
    bm.to_mesh(o.data)
    bm.free()
    o.data.materials.append(mats["blur"])
    return o


# --------------------------------------------------------------------------- Drohne

def motor_positions():
    # FL, FR, RL, RR (vorne = +X, links = +Y)
    return {
        "FL": Vector((MOTOR_OFF, MOTOR_OFF, 0)),
        "FR": Vector((MOTOR_OFF, -MOTOR_OFF, 0)),
        "RL": Vector((-MOTOR_OFF, MOTOR_OFF, 0)),
        "RR": Vector((-MOTOR_OFF, -MOTOR_OFF, 0)),
    }


def build_frame(mats):
    parts = []
    m = mats
    arm_len = MOTOR_DIAG / 2 + 0.006
    for key, p in motor_positions().items():
        ang = math.degrees(math.atan2(p.y, p.x))
        mid = p * 0.5
        parts.append(rbox("arm_" + key, (arm_len, 0.022, ARM_T), (mid.x, mid.y, ARM_T / 2), m["carbon"],
                          bevel=0.001, rot=(0, 0, math.radians(ang))))
        parts.append(cyl("pad_" + key, 0.0145, ARM_T, (p.x, p.y, ARM_T / 2), m["carbon"], verts=32,
                         bevel=0.0008))
        # Motor: Stator-Fuss, Glocke, Kuehlrippen-Ring, Welle
        z0 = ARM_T
        parts.append(cyl("mbase_" + key, 0.0135, 0.004, (p.x, p.y, z0 + 0.002), m["plastic"], verts=32))
        parts.append(cyl("mbell_" + key, 0.0138, MOTOR_H - 0.004, (p.x, p.y, z0 + 0.004 + (MOTOR_H - 0.004) / 2),
                         m["metal"], verts=40, bevel=0.0012, tile=0.02))
        parts.append(cyl("mshaft_" + key, 0.0025, 0.004, (p.x, p.y, z0 + MOTOR_H + 0.002), m["steel"], verts=12))
        # Armschutz / TPU-Bumper unter dem Motor
        parts.append(rbox("bumper_" + key, (0.012, 0.016, 0.004), (p.x * 1.12, p.y * 1.12, -0.001), m["paint"],
                          bevel=0.0015, rot=(0, 0, math.radians(ang))))

    # Grundplatte und Top-Platte
    parts.append(rbox("bottom_plate", (0.12, 0.042, ARM_T), (0.005, 0, ARM_T / 2), m["carbon"], bevel=0.002))
    parts.append(rbox("top_plate", (0.085, 0.038, 0.002), (-0.0075, 0, 0.031), m["carbon"], bevel=0.0008))
    for x in (-0.04, 0.025):
        for y in (-0.014, 0.014):
            parts.append(cyl("standoff", 0.0025, 0.026, (x, y, 0.018), m["steel"], verts=6))
    # Elektronik-Stack (FC + ESC) und Kondensator
    parts.append(rbox("esc", (0.032, 0.032, 0.005), (-0.008, 0, 0.009), m["plastic"], bevel=0.0005))
    parts.append(rbox("fc", (0.030, 0.030, 0.003), (-0.008, 0, 0.016), m["battery"], bevel=0.0005))
    parts.append(cyl("cap", 0.005, 0.016, (-0.028, 0, 0.012), m["plastic"], rot=(math.radians(90), 0, 0)))
    # Kamera-Seitenteile (TPU)
    for y in (-0.0115, 0.0115):
        parts.append(rbox("cam_side", (0.026, 0.0025, 0.026), (0.05, y, 0.018), m["paint"], bevel=0.002))
    # Akku mit Straps
    parts.append(rbox("battery", (0.072, 0.036, 0.032), (-0.008, 0, 0.032 + 0.016), m["battery"], bevel=0.004,
                      segs=3))
    for x in (-0.03, 0.014):
        parts.append(rbox("strap", (0.012, 0.0375, 0.0335), (x, 0, 0.032 + 0.0165), m["paint"], bevel=0.003,
                          segs=2))
    # XT60-Stecker hinten
    parts.append(rbox("xt60", (0.016, 0.008, 0.016), (-0.052, 0, 0.042), m["paint"], bevel=0.001))
    # Antenne: TPU-Halter, Rohr, Kappe
    ant_base = Vector((-0.058, 0, 0.026))
    ant_dir = Vector((-math.sin(math.radians(35)), 0, math.cos(math.radians(35))))
    ant_len = 0.05
    parts.append(rbox("ant_mount", (0.014, 0.02, 0.01), (-0.054, 0, 0.026), m["paint"], bevel=0.002))
    c = ant_base + ant_dir * ant_len / 2
    parts.append(cyl("ant_tube", 0.0022, ant_len, tuple(c), m["plastic"], verts=12,
                     rot=(0, -math.radians(35), 0)))
    tip = ant_base + ant_dir * ant_len
    parts.append(sphere("ant_cap", 0.0055, tuple(tip), m["plastic"], scale=(1, 1, 1.4)))
    # LED-Streifen unten hinten
    parts.append(rbox("led", (0.03, 0.006, 0.002), (-0.045, 0, -0.001), m["led"], bevel=0.0004))

    frame = join(parts, "SM_FPVDrone_Frame")
    frame.data.transform(Matrix.Translation((0, 0, -COG_Z)))

    # Sockets (relativ zum Pivot)
    for key, p in motor_positions().items():
        add_socket("Prop_" + key, (p.x, p.y, PROP_Z - COG_Z), frame)
    add_socket("FPVCam", (0.05, 0, 0.018 - COG_Z), frame)
    add_socket("Antenna", tuple(tip - Vector((0, 0, COG_Z))), frame)
    add_socket("LED", (-0.045, 0, -0.002 - COG_Z), frame)
    add_socket("Battery", (-0.008, 0, 0.048 - COG_Z), frame)
    add_socket("Bottom", (0, 0, -0.002 - COG_Z), frame)

    # Kollision: Mittelteil + zwei Diagonalen
    ucx = [
        add_ucx("SM_FPVDrone_Frame", 0, (0.13, 0.045, 0.07), (-0.002, 0, 0.032 - COG_Z)),
        add_ucx("SM_FPVDrone_Frame", 1, (MOTOR_DIAG + 0.03, 0.03, 0.026), (0, 0, 0.011 - COG_Z), 45),
        add_ucx("SM_FPVDrone_Frame", 2, (MOTOR_DIAG + 0.03, 0.03, 0.026), (0, 0, 0.011 - COG_Z), -45),
    ]
    return frame, ucx


def build_camera(mats):
    """Separate FPV-Kamera, Pivot = Neigeachse, damit der Code den Uptilt setzen kann."""
    m = mats
    parts = [
        rbox("cam_body", (0.019, 0.019, 0.019), (0, 0, 0), m["plastic"], bevel=0.002),
        cyl("cam_barrel", 0.007, 0.008, (0.0125, 0, 0), m["plastic"], rot=(0, math.radians(90), 0), verts=24,
            bevel=0.0006),
        cyl("cam_lens", 0.0052, 0.002, (0.0172, 0, 0), m["lens"], rot=(0, math.radians(90), 0), verts=24),
        cyl("cam_ring", 0.0074, 0.0015, (0.0155, 0, 0), m["paint"], rot=(0, math.radians(90), 0), verts=24),
    ]
    cam = join(parts, "SM_FPVDrone_Camera")
    add_socket("Lens", (0.019, 0, 0), cam)
    return cam


# --------------------------------------------------------------------------- Ausruestung

def build_remote(mats):
    """Generischer Gamepad-Style-Sender (Pocket-Format). Vorne = Antenne/Schalter (+X), Oben = Sticks."""
    m = mats
    parts = [
        rbox("body", (0.07, 0.15, 0.028), (0, 0, 0), m["plastic"], bevel=0.008, segs=4),
        rbox("faceplate", (0.06, 0.13, 0.002), (0, 0, 0.0145), m["paint"], bevel=0.0008),
        rbox("screen", (0.026, 0.036, 0.0012), (0.012, 0, 0.0158), m["screen"], bevel=0.0003),
    ]
    for y in (-0.042, 0.042):
        parts.append(cyl("gimbal_ring", 0.016, 0.003, (-0.004, y, 0.0165), m["plastic"], verts=32, bevel=0.0008))
        parts.append(cyl("stick", 0.002, 0.016, (-0.004, y, 0.025), m["steel"], verts=12))
        parts.append(cyl("stick_end", 0.0055, 0.006, (-0.004, y, 0.034), m["plastic"], verts=20, bevel=0.0012))
        # Griffe
        parts.append(cyl("grip", 0.016, 0.05, (-0.03, y * 1.55, -0.004), m["plastic"],
                         rot=(0, math.radians(70), 0), verts=24, bevel=0.004))
    for y in (-0.05, -0.03, 0.03, 0.05):
        parts.append(cyl("switch", 0.0018, 0.01, (0.036, y, 0.008), m["steel"], rot=(0, math.radians(70), 0),
                         verts=8))
    parts.append(cyl("antenna", 0.003, 0.03, (0.04, 0, 0.004), m["plastic"], rot=(0, math.radians(90), 0),
                     verts=12))
    for y in (-0.014, 0.014):
        parts.append(rbox("button", (0.006, 0.008, 0.002), (-0.012, y, 0.0158), m["steel"], bevel=0.0006))
    rem = join(parts, "SM_FPV_Remote")
    add_socket("Grip_L", (-0.03, 0.065, -0.004), rem)
    add_socket("Grip_R", (-0.03, -0.065, -0.004), rem)
    add_socket("Screen", (0.012, 0, 0.0165), rem)
    return rem


def build_goggles(mats):
    """FPV-Brille. Vorne = Blickrichtung (+X), Kopfmitte liegt bei -X."""
    m = mats
    parts = [
        rbox("shell", (0.06, 0.17, 0.065), (0.03, 0, 0), m["plastic"], bevel=0.014, segs=4),
        rbox("visor", (0.004, 0.15, 0.045), (0.0605, 0, 0.002), m["paint"], bevel=0.002),
        rbox("foam", (0.018, 0.16, 0.058), (-0.006, 0, -0.002), m["foam"], bevel=0.008, segs=3),
        rbox("strap_l", (0.09, 0.006, 0.03), (-0.05, 0.088, 0), m["foam"], bevel=0.002),
        rbox("strap_r", (0.09, 0.006, 0.03), (-0.05, -0.088, 0), m["foam"], bevel=0.002),
        rbox("rx_module", (0.03, 0.05, 0.012), (0.03, 0, 0.038), m["battery"], bevel=0.003),
    ]
    for y in (-0.03, 0.03):
        parts.append(cyl("patch", 0.008, 0.014, (0.03, y, 0.048), m["plastic"], verts=20, bevel=0.002))
        parts.append(cyl("dial", 0.004, 0.006, (0.04, y * 2.6, -0.028), m["steel"], verts=16))
    gog = join(parts, "SM_FPV_Goggles")
    add_socket("Head", (-0.09, 0, 0), gog)
    return gog


# --------------------------------------------------------------------------- Export

def export_fbx(objs, filename):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.hide_set(False)
        o.select_set(True)
        for c in o.children:
            c.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    path = os.path.join(OUT_MESH, filename)
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"MESH", "EMPTY"},
        apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Z", axis_up="Y",
        mesh_smooth_type="FACE", use_mesh_modifiers=True, add_leaf_bones=False,
        bake_anim=False, path_mode="STRIP", embed_textures=False,
    )
    print("FBX:", filename)


def place(obj, loc, rot=(0, 0, 0)):
    """Kopie eines Objekts fuer Vorschau/Icon-Szenen."""
    c = obj.copy()
    c.data = obj.data
    c.location = loc
    c.rotation_euler = Euler([math.radians(a) for a in rot])
    link(c)
    return c


# --------------------------------------------------------------------------- Rendern

def setup_render(res):
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = 96
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.resolution_x = res[0]
    sc.render.resolution_y = res[1]
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "AgX"
    sc.view_settings.look = "AgX - Medium High Contrast"
    world = bpy.data.worlds.new("World")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.6, 0.7, 1)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.3
    sc.world = world


def setup_lights(scale):
    def area(name, loc, energy, size, color=(1, 1, 1)):
        d = bpy.data.lights.new(name, "AREA")
        d.energy = energy * scale ** 2
        d.size = size * scale
        d.color = color
        o = link(bpy.data.objects.new(name, d))
        o.location = Vector(loc) * scale
        o.rotation_euler = (Vector((0, 0, 0)) - o.location).to_track_quat("-Z", "Y").to_euler()
        return o
    return [
        area("key", (2.0, -1.5, 2.5), 650, 2.0, (1.0, 0.95, 0.88)),
        area("fill", (-1.5, -2.0, 1.0), 250, 3.0, (0.8, 0.88, 1.0)),
        area("rim", (-1.5, 2.0, 2.0), 500, 1.5, (1.0, 0.75, 0.45)),
    ]


def render_objects(objs, out_path, res=(512, 512), view_dir=(1.0, -1.25, 0.8), pad=1.15):
    """Rendert die Objekte mit orthografischer Kamera, automatisch eingepasst."""
    sc = bpy.context.scene
    for o in sc.objects:
        o.hide_render = o not in objs and o.type == "MESH"
    pts = []
    dg = bpy.context.evaluated_depsgraph_get()
    for o in objs:
        for v in o.evaluated_get(dg).data.vertices:
            pts.append(o.matrix_world @ v.co)
    center = sum(pts, Vector()) / len(pts)
    d = Vector(view_dir).normalized()
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam = link(bpy.data.objects.new("cam", cam_data))
    rot = (-d).to_track_quat("-Z", "Y")
    cam.rotation_euler = rot.to_euler()
    inv = rot.to_matrix().inverted()
    local = [inv @ (p - center) for p in pts]
    w = max(abs(p.x) for p in local) * 2
    h = max(abs(p.y) for p in local) * 2
    aspect = res[0] / res[1]
    cam_data.ortho_scale = max(w, h * aspect) * pad
    radius = max((p - center).length for p in pts)
    cam.location = center + d * radius * 4
    cam_data.clip_end = radius * 10
    sc.camera = cam
    lights = setup_lights(radius * 4)
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.filepath = out_path
    bpy.ops.render.render(write_still=True)
    for l in lights:
        bpy.data.objects.remove(l)
    bpy.data.objects.remove(cam)
    print("Render:", out_path)


def assembled_drone(frame, cam, prop_cw, prop_ccw, offset=(0, 0, 0)):
    """Setzt eine vollstaendige Drohne aus den Einzelteilen fuer Renderings zusammen."""
    off = Vector(offset)
    objs = [place(frame, off)]
    for key, p in motor_positions().items():
        prop = prop_ccw if key in ("FL", "RR") else prop_cw
        objs.append(place(prop, off + Vector((p.x, p.y, PROP_Z - COG_Z)), (0, 0, 37 * hash(key) % 120)))
    objs.append(place(cam, off + Vector((0.05, 0, 0.018 - COG_Z)), (0, -CAM_UPTILT, 0)))
    return objs


def main():
    os.makedirs(OUT_MESH, exist_ok=True)
    os.makedirs(OUT_ICON, exist_ok=True)
    os.makedirs(OUT_PREVIEW, exist_ok=True)
    reset_scene()
    mats = build_materials()

    frame, ucx = build_frame(mats)
    cam = build_camera(mats)
    prop_ccw = build_prop("SM_FPVDrone_Prop_CCW", mats, ccw=True)
    prop_cw = build_prop("SM_FPVDrone_Prop_CW", mats, ccw=False)
    blur = build_prop_blur(mats)
    remote = build_remote(mats)
    goggles = build_goggles(mats)

    export_fbx([frame] + ucx, "SM_FPVDrone_Frame.fbx")
    for o in (cam, prop_ccw, prop_cw, blur, remote, goggles):
        export_fbx([o], o.name + ".fbx")

    # Quelldatei zum Weiterbearbeiten
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "FPVDrone.blend"), compress=True,
                                relative_remap=True)
    bpy.ops.file.make_paths_relative()
    bpy.ops.wm.save_mainfile(compress=True)

    if "--no-render" in sys.argv:
        return
    setup_render((512, 512))
    for o in ucx:
        o.hide_render = True
    # Exportierte Einzelteile aus Renderings ausblenden, nur Kopien rendern
    for o in (frame, cam, prop_ccw, prop_cw, blur, remote, goggles):
        o.location.x += 100
    drone = assembled_drone(frame, cam, prop_cw, prop_ccw)
    render_objects(drone, os.path.join(OUT_ICON, "IconFPVDrone_512.png"))
    render_objects([place(remote, (0, 0, 0))], os.path.join(OUT_ICON, "IconFPVRemote_512.png"),
                   view_dir=(-0.9, -1.0, 1.4))
    render_objects([place(goggles, (0, 3, 0))], os.path.join(OUT_ICON, "IconFPVGoggles_512.png"),
                   view_dir=(1.3, -1.0, 0.6))
    render_objects([place(prop_ccw, (0, 6, 0))], os.path.join(OUT_ICON, "IconFPVProp_512.png"),
                   view_dir=(0.6, -0.6, 1.2))
    bpy.context.scene.cycles.samples = 160
    render_objects(drone, os.path.join(OUT_PREVIEW, "preview_drone.png"), res=(1280, 720),
                   view_dir=(1.0, -0.8, 0.45), pad=1.08)


if __name__ == "__main__":
    main()
