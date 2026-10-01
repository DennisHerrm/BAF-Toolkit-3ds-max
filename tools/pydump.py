# ============================================================
#  pydump.py - die Referenz, gegen die der C++-Parser antritt.
#
#  Benutzt den Codec des Blender-Addons io_scene_baf (die
#  Dateien baf/codec.py und baf/lab.py, beide ohne bpy). Die
#  Ausgabe ist zeichengleich mit der von bafdump - Zeile fuer
#  Zeile, Zahl fuer Zahl. Ein schlichtes diff genuegt.
#
#  Aufruf:
#     python pydump.py <addon-ordner> big.lab anakin.baf out.txt [--all]
#     python pydump.py <addon-ordner> big.lab --alle out/ [--all]
#
#  <addon-ordner> ist der Ordner, in dem "baf/codec.py" liegt.
#
#  Gleitkommazahlen: "%.6f" auf beiden Seiten. Python formatiert
#  locale-unabhaengig, C++ benutzt dafuer std::to_chars mit
#  chars_format::fixed - ein deutsches Max wuerde mit printf
#  sonst Dezimalkommata schreiben.
# ============================================================
import math
import os
import struct
import sys

FNV_OFFSET = 1469598103934665603
FNV_PRIME = 1099511628211
MASK64 = (1 << 64) - 1


def fnv1a(data):
    """FNV-1a, 64 Bit. Dieselbe Zahl wie Fnv1a() in baf_reader.cpp."""
    h = FNV_OFFSET
    for byte in data:
        h = ((h ^ byte) * FNV_PRIME) & MASK64
    return '%016x' % h


def float32(value):
    """Auf float32 runden. Die C++-Seite legt die aufbereiteten Positionen
    und UVs in float ab; ohne diesen Schritt weicht die Referenz in den
    letzten Stellen ab."""
    return struct.unpack('<f', struct.pack('<f', value))[0]


def f(value, decimals=6):
    """Muss Zeichen fuer Zeichen dasselbe liefern wie FormatFloat()
    in baf_reader.cpp - dieselbe Rechnung, dieselbe Tie-Regel.

    Bewusst NICHT '%.6f': weder printf noch std::to_chars legen
    fest, wohin ein exakter Rundungs-Gleichstand faellt. In den
    Retail-Daten gibt es 473 Werte, die genau auf der Grenze der
    6. Nachkommastelle liegen; dort duerften UCRT und glibc
    verschieden runden, und der Vergleich wuerde einen Fehler
    melden, den es nicht gibt.

    Also: skalieren (gewoehnliche IEEE-754-Multiplikation, auf
    jeder Plattform bitgleich), Betrag, +0.5, abrunden. Die
    Tie-Regel ist damit unsere eigene und steht hier im Quelltext.
    """
    if math.isnan(value):
        return 'nan'
    if math.isinf(value):
        return '-inf' if value < 0.0 else 'inf'

    scale = 10 ** decimals
    units = math.floor(abs(value * float(scale)) + 0.5)
    whole, fraction = divmod(units, scale)
    # Das Vorzeichen haengt am Eingabewert, nicht am Ergebnis:
    # -0.0000001 ergibt "-0.000000", genau wie in C++.
    sign = '-' if math.copysign(1.0, value) < 0.0 else ''
    if decimals == 0:
        return '%s%d' % (sign, whole)
    return '%s%d.%0*d' % (sign, whole, decimals, fraction)


def v3(vec):
    return '%s %s %s' % (f(vec[0]), f(vec[1]), f(vec[2]))


def node_local(node, scale, euler_basis):
    """Die lokale Matrix eines Knotens, als m[0..3] = right, forward, up,
    Translation. Genau die Anordnung von Matrix34 in baf_reader.h."""
    right, forward, up = euler_basis(*node.bind_euler)
    return [list(right), list(forward), list(up),
            [component * scale for component in node.bind_pos]]


def concat(parent, local):
    """parent @ local in der Konvention der Engine, wo right/forward/up die
    Spalten sind: v' = right*v.x + forward*v.y + up*v.z."""
    def apply(v):
        return [parent[0][axis] * v[0] + parent[1][axis] * v[1] + parent[2][axis] * v[2]
                for axis in range(3)]
    result = [apply(local[0]), apply(local[1]), apply(local[2]), apply(local[3])]
    result[3] = [result[3][axis] + parent[3][axis] for axis in range(3)]
    return result


def rest_world(baf, scale, euler_basis):
    world = []
    for i, node in enumerate(baf.nodes):
        local = node_local(node, scale, euler_basis)
        if 0 <= node.parent < i:
            world.append(concat(world[node.parent], local))
        else:
            world.append(local)
    return world


def write_rest(baf, out, scale, euler_basis):
    for i, tm in enumerate(rest_world(baf, scale, euler_basis)):
        out.write('rest %d %s r=%s %s %s f=%s %s %s u=%s %s %s t=%s %s %s\n'
                  % (i, baf.nodes[i].name,
                     f(tm[0][0]), f(tm[0][1]), f(tm[0][2]),
                     f(tm[1][0]), f(tm[1][1]), f(tm[1][2]),
                     f(tm[2][0]), f(tm[2][1]), f(tm[2][2]),
                     f(tm[3][0]), f(tm[3][1]), f(tm[3][2])))


FNV_LOCAL = None


def face_render_class(face):
    """Dieselbe Ableitung wie FaceRenderClass() in baf_reader.cpp, gelesen
    NACH den beiden Ladedurchgaengen."""
    v0 = face.v0_loaded
    base = v0 if (0.0 < v0 < 1.0) else 1.0
    alpha255 = int(base * 1.0 * 255.0)
    if alpha255 == 255:
        blend = 0                      # Opaque
    elif face.flags_loaded & 0x0010:   # additiv
        blend = 2
    else:
        blend = 1                      # translucent
    return {'blend': blend,
            'alpha': alpha255 / 255.0,
            'two_sided': bool(face.flags_loaded & 0x0001),
            'env_map': bool(face.flags_loaded & 0x0008),
            'fullbright': face.light_mode_loaded == 0}


def material_slots(baf):
    """Die Materialplaetze in Datei-Reihenfolge, wie BuildMaterialSlots()."""
    slots = []
    for mesh in baf.meshes:
        for face in mesh.faces:
            rc = face_render_class(face)
            key = (face.material, rc['blend'], rc['two_sided'], rc['fullbright'])
            if any(s[0] == key for s in slots):
                continue
            slots.append((key, rc))
    return slots


def slot_of_face(slots, face):
    rc = face_render_class(face)
    key = (face.material, rc['blend'], rc['two_sided'], rc['fullbright'])
    for i, (have, _rc) in enumerate(slots):
        if have == key:
            return i
    return 0


def decode_cel(baf, material_index, cel_index):
    """RGBA eines Cels, Zeile 0 oben - wie DecodeCel() in baf_reader.cpp."""
    mat = baf.materials[material_index]
    if cel_index >= len(mat.cels):
        return None
    cel = mat.cels[cel_index]
    if not cel.pixels or not cel.width or not cel.height or mat.bit_depth != 8:
        return None
    pal = baf.material_palette(material_index)
    keyed = bool(cel.color_key_enable & 1)
    out = bytearray(cel.width * cel.height * 4)
    for row in range(cel.height):
        for column in range(cel.width):
            index = cel.pixels[row * cel.width + column]
            if pal and index * 3 + 2 < len(pal):
                red, green, blue = pal[index * 3], pal[index * 3 + 1], pal[index * 3 + 2]
            else:
                red = green = blue = index
            target = (row * cel.width + column) * 4
            out[target] = red
            out[target + 1] = green
            out[target + 2] = blue
            out[target + 3] = 0 if (keyed and index == 0) else 255
    return {'width': cel.width, 'height': cel.height, 'keyed': keyed, 'rgba': bytes(out)}


def build_mesh(baf, mesh_index, scale, material_cel_size, texel_to_uv, slots):
    """Dieselbe Aufbereitung wie BuildMeshGeometry() in baf_reader.cpp:
    Pivot, Faktor, Faechertriangulierung, UVs je Ecke, entartete Faces
    uebersprungen.

    Die drei Ausschlussgruende sind wortwoertlich die des Blender-Addons.
    Was danach kommt, ist bewusst NICHT dessen from_pydata: das laesst ein
    Polygon fallen, dessen Vertexmenge schon vorkam, und die Retail-Kunst
    baut zweiseitige Geometrie als deckungsgleiche Zwillinge. Max nimmt die
    an, also behalten wir sie - der Vergleich prueft hier also unsere eigene
    Regel, nicht die von Blender."""
    mesh = baf.meshes[mesh_index]
    node = baf.node_of_mesh(mesh_index)
    pivot = node.pivot if node else (0.0, 0.0, 0.0)
    node_index = node.num if node else -1

    positions = [tuple(float32((position[axis] + pivot[axis]) * scale)
                       for axis in range(3))
                 for position in mesh.positions]

    tris, skipped = [], []
    for face_index, face in enumerate(mesh.faces):
        corners = face.vert_idx
        if len(corners) < 3 or len(set(corners)) != len(corners) \
                or any(index >= len(positions) for index in corners):
            skipped.append(face_index)
            continue
        cel = material_cel_size(baf, face.material)

        def corner_uv(corner):
            if corner >= len(face.tex_idx):
                return (0.0, 0.0)
            texel = face.tex_idx[corner]
            if texel >= len(mesh.tex_verts):
                return (0.0, 0.0)
            u, v = texel_to_uv(mesh.tex_verts[texel], cel)
            return (float32(u), float32(v))

        slot = slot_of_face(slots, face) if slots else 0
        for corner in range(1, len(corners) - 1):
            pick = (0, corner, corner + 1)
            tris.append((tuple(corners[k] for k in pick),
                         tuple(corner_uv(k) for k in pick),
                         face.material, slot, face_index))
    return {'name': mesh.name, 'index': mesh_index, 'node': node_index,
            'drawn': 1 if mesh.geometry_mode != 0 else 0,
            'positions': positions, 'tris': tris, 'skipped': skipped}


def write_build(baf, out, scale, material_cel_size, texel_to_uv):
    slots = material_slots(baf)
    for i, (key, rc) in enumerate(slots):
        out.write('slot %d mat=%d blend=%d twosided=%d fullbright=%d envmap=%d alpha=%s\n'
                  % (i, key[0], key[1], int(key[2]), int(key[3]),
                     int(rc['env_map']), f(rc['alpha'])))
    for mi in range(len(baf.materials)):
        for ci in range(len(baf.materials[mi].cels)):
            decoded = decode_cel(baf, mi, ci)
            if decoded is None:
                continue
            out.write('cel %d %d %dx%d keyed=%d rgba=%s\n'
                      % (mi, ci, decoded['width'], decoded['height'],
                         int(decoded['keyed']), fnv1a(decoded['rgba'])))
    for mesh_index in range(len(baf.meshes)):
        built = build_mesh(baf, mesh_index, scale, material_cel_size, texel_to_uv, slots)
        out.write('build %d name=%s node=%d drawn=%d verts=%d tris=%d skipped=%d\n'
                  % (built['index'], built['name'], built['node'], built['drawn'],
                     len(built['positions']), len(built['tris']),
                     len(built['skipped'])))
        for i, position in enumerate(built['positions']):
            out.write('  bv %d %s %s %s\n'
                      % (i, f(position[0]), f(position[1]), f(position[2])))
        for i, (verts_, uvs, material, slot, source) in enumerate(built['tris']):
            out.write('  bt %d %d %d %d mat=%d slot=%d src=%d uv=%s\n'
                      % (i, verts_[0], verts_[1], verts_[2], material, slot, source,
                         ' '.join('%s %s' % (f(uv[0]), f(uv[1])) for uv in uvs)))
        for face_index in built['skipped']:
            out.write('  bs %d\n' % face_index)


def sample_local(track, frame, scale, euler_basis, cursor_at):
    """Wie SampleLocalMatrix() in baf_reader.cpp: der LETZTE Key mit
    frame <= t, und ohne sein Interpolationsbit haelt er seinen Wert."""
    if not track.keys:
        return [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0], [0.0, 0.0, 0.0]]
    key = track.keys[cursor_at(track, frame)]
    elapsed = frame - key.frame
    position, orientation = [], []
    for axis in range(3):
        position.append(key.pos[axis] + key.dpos[axis] * elapsed
                        if (key.flags & 0x01) else key.pos[axis])
        orientation.append(key.orient[axis] + key.dorient[axis] * elapsed
                           if (key.flags & 0x02) else key.orient[axis])
    right, forward, up = euler_basis(*orientation)
    return [list(right), list(forward), list(up),
            [component * scale for component in position]]


def write_anim(baf, out, scale, euler_basis, cursor_at):
    for clip_index, clip in enumerate(baf.clips):
        out.write('anim %d name=%s fps=%s frames=%d tracks=%d\n'
                  % (clip_index, clip.name, f(clip.fps), clip.num_frames,
                     len(clip.tracks)))
        for track_index, track in enumerate(clip.tracks):
            joint = track.joint_index \
                if 0 <= track.joint_index < len(baf.nodes) else track_index
            out.write('  at %d joint=%d name=%s keys=%d\n'
                      % (track_index, joint, track.name, len(track.keys)))
            for key_index, key in enumerate(track.keys):
                tm = sample_local(track, key.frame, scale, euler_basis, cursor_at)
                out.write('    ak %d f=%s flags=%d %s\n'
                          % (key_index, f(key.frame), key.flags,
                             ' '.join(f(tm[row][col])
                                      for row in range(4) for col in range(3))))


def join_list(values):
    return ','.join(values)


def v3(vec_like):
    return '%s %s %s' % (f(vec_like[0]), f(vec_like[1]), f(vec_like[2]))


def write_props(baf, out, scale, material_cel_size, texel_to_uv, slots):
    """Spiegelt ModelPropText/NodePropText/MeshPropText/ClipPropText und
    CheckLimits aus baf_reader.cpp."""
    out.write('props model\n')
    out.write('bafModelName = %s\n' % baf.name)
    out.write('bafHeaderScale = %s\n' % f(baf.scale))
    out.write('bafRadiusU = %s\n' % f(baf.radius_u))
    out.write('bafHeightU = %s\n' % f(baf.height_u))
    out.write('bafNodeRefs = %d,%d,%d,%d\n' % tuple(baf.node_refs))
    out.write('bafClipCount = %d\n' % len(baf.clips))

    for i, node in enumerate(baf.nodes):
        out.write('props node %d\n' % i)
        out.write('bafNodeIndex = %d\n' % i)
        out.write('bafNodeName = %s\n' % node.name)
        out.write('bafNodeType = %d\n' % node.type)
        out.write('bafNodeFlags = %d\n' % node.flags)
        out.write('bafMeshIndex = %d\n' % node.mesh_index)
        out.write('bafParent = %d\n' % node.parent)
        out.write('bafPivot = %s\n' % v3(node.pivot))
        out.write('bafBindPos = %s\n' % v3(node.bind_pos))
        out.write('bafBindEuler = %s\n' % v3(node.bind_euler))

    for mesh_index, mesh in enumerate(baf.meshes):
        built = build_mesh(baf, mesh_index, scale, material_cel_size, texel_to_uv, slots)
        out.write('props mesh %d\n' % mesh_index)
        out.write('bafMeshIndex = %d\n' % mesh_index)
        out.write('bafMeshName = %s\n' % mesh.name)
        out.write('bafGeometryMode = %d\n' % mesh.geometry_mode)
        out.write('bafLightingMode = %d\n' % mesh.lighting_mode)
        out.write('bafTextureMode = %d\n' % mesh.texture_mode)
        out.write('bafBoundsRadius = %s\n' % f(mesh.radius))
        out.write('bafBoundsCenter = %s\n' % v3(mesh.center))
        out.write('bafFaceMap = %s\n'
                  % join_list([str(tri[4]) for tri in built['tris']]))
        out.write('bafSkippedFaces = %s\n'
                  % join_list([str(x) for x in built['skipped']]))
        out.write('bafFaceFlags = %s\n'
                  % join_list([str(face.flags) for face in mesh.faces]))
        out.write('bafFaceLightMode = %s\n'
                  % join_list([str(face.light_mode) for face in mesh.faces]))
        out.write('bafFaceGeoMode = %s\n'
                  % join_list([str(face.geometry_mode) for face in mesh.faces]))
        out.write('bafFaceTexMode = %s\n'
                  % join_list([str(face.texture_mode) for face in mesh.faces]))
        out.write('bafFaceExtraLight = %s\n'
                  % join_list([f(face.extra_light) for face in mesh.faces]))
        out.write('bafFaceMaterial = %s\n'
                  % join_list([str(face.material) for face in mesh.faces]))
        out.write('bafIntensity = %s\n'
                  % join_list([f(value) for value in mesh.intensities]))

    for clip_index, clip in enumerate(baf.clips):
        prefix = 'bafClip%d' % clip_index
        out.write('props clip %d\n' % clip_index)
        out.write('%sName = %s\n' % (prefix, clip.name))
        out.write('%sFps = %s\n' % (prefix, f(clip.fps)))
        out.write('%sFrames = %d\n' % (prefix, clip.num_frames))
        out.write('%sFade = %s\n' % (prefix, f(clip.fade_s)))
        out.write('%sLoop = %d\n' % (prefix, clip.var1_loop))
        out.write('%sVar2 = %d\n' % (prefix, clip.var2_frame))
        out.write('%sTrackFlags = %d\n' % (prefix, clip.track_flags))
        out.write('%sUsage = %d\n' % (prefix, clip.special_usage))
        out.write('%sTypeMask = %d\n' % (prefix, clip.type_mask))
        out.write('%sEvents = %s\n' % (prefix, join_list([
            '%d:%s:%d:%s:%d:%d' % (event.frame, event.wav, event.type_bits,
                                   f(event.volume, 3), event.priority, event.sound_id)
            for event in clip.events])))

    # --- Grenzen ---
    lines, exceeded = [], 0

    def check(bad, text):
        nonlocal exceeded
        lines.append(('UEBER  ' if bad else 'ok     ') + text)
        if bad:
            exceeded += 1

    check(len(baf.nodes) > 48, 'Knoten %d / 48' % len(baf.nodes))
    worst = [(0, ''), (0, ''), (0, ''), (0, '')]
    for mesh in baf.meshes:
        if len(mesh.positions) > worst[0][0]:
            worst[0] = (len(mesh.positions), mesh.name)
        if len(mesh.faces) > worst[1][0]:
            worst[1] = (len(mesh.faces), mesh.name)
        if len(mesh.tex_verts) > worst[2][0]:
            worst[2] = (len(mesh.tex_verts), mesh.name)
        for face in mesh.faces:
            if len(face.vert_idx) > worst[3][0]:
                worst[3] = (len(face.vert_idx), mesh.name)
    check(worst[0][0] > 0x200, 'Vertices je Mesh %d / 512  (%s)' % worst[0])
    check(worst[1][0] > 0x200, 'Faces je Mesh %d / 512  (%s)' % worst[1])
    check(worst[2][0] > 0x300, 'Texturvertices je Mesh %d / 768  (%s)' % worst[2])
    check(worst[3][0] > 0x18, 'Ecken je Face %d / 24  (%s)' % worst[3])
    out.write('limits exceeded=%d\n' % exceeded)
    for line in lines:
        out.write(line + '\n')


def write_dump(baf, label, out, verts=False, keys=False, rest=False,
               build=False, anim=False, props=False, scale=1.0, euler_basis=None,
               material_cel_size=None, texel_to_uv=None, cursor_at=None):
    w = out.write
    w('BAF %s\n' % label)
    w('name %s\n' % baf.name)
    w('scale %s\n' % f(baf.scale))
    w('radius %s\n' % f(baf.radius_u))
    w('height %s\n' % f(baf.height_u))
    w('noderefs %d %d %d %d\n' % tuple(baf.node_refs))
    palette = baf.palette or b''
    w('palette %d %s\n' % (len(palette), fnv1a(palette)))
    w('counts materials=%d meshes=%d nodes=%d clips=%d\n'
      % (len(baf.materials), len(baf.meshes), len(baf.nodes), len(baf.clips)))

    for i, material in enumerate(baf.materials):
        w('material %d name=%s type=%d bits=%d cels=%d ownpal=%d\n'
          % (i, material.name, material.mat_type, material.bit_depth,
             len(material.cels), len(material.own_palette or b'')))
        for c, cel in enumerate(material.cels):
            w('  cel %d %dx%d key=%d mips=%d bytes=%d hash=%s\n'
              % (c, cel.width, cel.height, cel.color_key_enable, cel.num_mips,
                 len(cel.pixels), fnv1a(cel.pixels)))

    for i, node in enumerate(baf.nodes):
        w('node %d name=%s flags=%d type=%d mesh=%d parent=%d children=%d child=%d '
          'sibling=%d pivot=%s pos=%s eul=%s\n'
          % (i, node.name, node.flags, node.type, node.mesh_index, node.parent,
             node.num_children, node.child, node.sibling,
             v3(node.pivot), v3(node.bind_pos), v3(node.bind_euler)))

    if rest:
        write_rest(baf, out, scale, euler_basis)

    for i, mesh in enumerate(baf.meshes):
        w('mesh %d name=%s geo=%d light=%d tex=%d verts=%d texverts=%d faces=%d '
          'radius=%s center=%s\n'
          % (i, mesh.name, mesh.geometry_mode, mesh.lighting_mode, mesh.texture_mode,
             len(mesh.positions), len(mesh.tex_verts), len(mesh.faces),
             f(mesh.radius), v3(mesh.center)))
        if not verts:
            continue
        for vi in range(len(mesh.positions)):
            w('  v %d %s n %s i %s\n'
              % (vi, v3(mesh.positions[vi]), v3(mesh.normals[vi]),
                 f(mesh.intensities[vi])))
        for ti, texel in enumerate(mesh.tex_verts):
            w('  t %d %s %s\n' % (ti, f(texel[0]), f(texel[1])))
        for fi, face in enumerate(mesh.faces):
            w('  f %d mat=%d flags=%d geo=%d light=%d tex=%d xl=%s n=%s '
              'loaded=%d/%d/%s vi=%s ti=%s\n'
              % (fi, face.material, face.flags, face.geometry_mode, face.light_mode,
                 face.texture_mode, f(face.extra_light), v3(face.normal),
                 face.flags_loaded, face.light_mode_loaded, f(face.v0_loaded),
                 ','.join(str(x) for x in face.vert_idx),
                 ','.join(str(x) for x in face.tex_idx)))

    if build:
        write_build(baf, out, scale, material_cel_size, texel_to_uv)

    if props:
        write_props(baf, out, scale, material_cel_size, texel_to_uv,
                    material_slots(baf))

    if anim:
        write_anim(baf, out, scale, euler_basis, cursor_at)

    for i, clip in enumerate(baf.clips):
        key_total = sum(len(track.keys) for track in clip.tracks)
        w('clip %d name=%s fps=%s frames=%d fade=%s loop=%d var2=%d trackflags=%d '
          'usage=%d mask=%d tracks=%d keys=%d events=%d\n'
          % (i, clip.name, f(clip.fps), clip.num_frames, f(clip.fade_s),
             clip.var1_loop, clip.var2_frame, clip.track_flags, clip.special_usage,
             clip.type_mask, len(clip.tracks), key_total, len(clip.events)))
        for ei, event in enumerate(clip.events):
            w('  e %d frame=%d type=%d wav=%s call=%d vol=%s range=%s min=%s max=%s '
              'prio=%d id=%d\n'
              % (ei, event.frame, event.type_bits, event.wav, event.call_flags,
                 f(event.volume), f(event.range_u), f(event.min_dist_u),
                 f(event.max_dist_u), event.priority, event.sound_id))
        if not keys:
            continue
        for ti, track in enumerate(clip.tracks):
            w('  track %d name=%s joint=%d keys=%d\n'
              % (ti, track.name, track.joint_index, len(track.keys)))
            for ki, key in enumerate(track.keys):
                w('    k %d f=%s flags=%d p=%s o=%s dp=%s do=%s\n'
                  % (ki, f(key.frame), key.flags, v3(key.pos), v3(key.orient),
                     v3(key.dpos), v3(key.dorient)))


def main(argv):
    if len(argv) < 4:
        print(__doc__ or 'siehe Kopf der Datei')
        return 2
    addon_dir = argv[1]
    sys.path.insert(0, addon_dir)
    from baf.codec import (parse, euler_basis, material_cel_size,   # noqa: E402
                           texel_to_uv, cursor_at)
    from baf.lab import LabArchive             # noqa: E402

    verts = '--all' in argv or '--verts' in argv
    keys = '--all' in argv or '--keys' in argv
    rest = '--all' in argv or '--rest' in argv
    build = '--all' in argv or '--build' in argv
    anim = '--all' in argv or '--anim' in argv
    props = '--all' in argv or '--props' in argv
    scale = 1.0
    for arg in argv:
        if arg.startswith('--scale='):
            scale = float(arg.split('=', 1)[1])
    # Nur die Detailschalter wegfiltern - "--alle" ist ein Modus,
    # kein Schalter, und muss stehen bleiben.
    detail_flags = ('--all', '--verts', '--keys', '--rest', '--build', '--anim',
                    '--props')
    argv = [a for a in argv if not a.startswith('--scale=')]
    args = [a for a in argv[2:] if a not in detail_flags]

    archive_path, second = args[0], args[1]

    if second == '--alle':
        folder = args[2]
        os.makedirs(folder, exist_ok=True)
        lab = LabArchive(archive_path)
        names = sorted(e.name for e in lab.entries if e.name.lower().endswith('.baf'))
        for name in names:
            entry = lab.find(name)
            baf = parse(lab.read_bytes(entry), name)
            with open(os.path.join(folder, name + '.txt'), 'w', newline='\n') as out:
                write_dump(baf, name, out, verts, keys, rest, build, anim, props, scale,
                           euler_basis, material_cel_size, texel_to_uv, cursor_at)
        print('%d gedumpt' % len(names))
        return 0

    lab = LabArchive(archive_path)
    entry = lab.find(second)
    if entry is None:
        print('%s nicht im Archiv' % second)
        return 1
    baf = parse(lab.read_bytes(entry), entry.name)
    target = args[2] if len(args) > 2 else None
    if target:
        with open(target, 'w', newline='\n') as out:
            write_dump(baf, entry.name, out, verts, keys, rest, build, anim, props, scale,
                       euler_basis, material_cel_size, texel_to_uv, cursor_at)
        print('Dump geschrieben: %s' % target)
    else:
        write_dump(baf, entry.name, sys.stdout, verts, keys, rest, build, anim, props, scale,
                   euler_basis, material_cel_size, texel_to_uv, cursor_at)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
