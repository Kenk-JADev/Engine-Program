#!/usr/bin/env python3
"""Erzeugt die Demo-Karten des SampleProjects (PAKET 25).

Schreibt das binaere .map-Format der Engine:
    int32 width, int32 height, int32 layerCount
    pro Layer: int32 nameLen, name (utf-8), float32 elevation,
               width*height int32 Tile-IDs (-1 = leer)

Aufruf (vom Repo-Root):
    python3 scripts/make_sample_maps.py

Tile-Palette des Demo-Tilesets (assets/textures/tileset_demo.png,
8 Spalten x 6 Zeilen, ID = zeile*8 + spalte):
    8  = Gras (hell)            24 = hohes Gras (Durchwiese/bush)
    9  = Erde (Weg)             12 = Sand (Ufer)
    2  = Steinwand (blockiert)  18 = Fels (blockiert)
    11 = Wasser (blockiert)
"""

import os
import struct
import sys

K_GRASS = 8
K_TALL_GRASS = 24
K_DIRT = 9
K_SAND = 12
K_STONE = 2
K_ROCK = 18
K_WATER = 11


def new_layer(name, w, h, elevation=0.0, fill=-1):
    return {
        "name": name,
        "elevation": elevation,
        "tiles": [fill] * (w * h),
    }


def set_tile(layer, w, h, x, z, tid, only_if=None):
    if not (0 <= x < w and 0 <= z < h):
        return
    idx = z * w + x
    if only_if is not None and layer["tiles"][idx] != only_if:
        return
    layer["tiles"][idx] = tid


def ellipse(ground, w, h, cx, cz, rx, rz, inner, outer):
    """Fuellt eine Ellipse: d<=1 -> inner; d<=outer -> Sand-Uferring."""
    for z in range(cz - rz - 2, cz + rz + 3):
        for x in range(cx - rx - 2, cx + rx + 3):
            dx = (x - cx) / float(max(1, rx))
            dz = (z - cz) / float(max(1, rz))
            d = dx * dx + dz * dz
            if d <= 1.0:
                set_tile(ground, w, h, x, z, inner)
            elif d <= outer:
                set_tile(ground, w, h, x, z, K_SAND)


def border_ring(objects, w, h, tid):
    for x in range(w):
        set_tile(objects, w, h, x, 0, tid)
        set_tile(objects, w, h, x, h - 1, tid)
    for z in range(1, h - 1):
        set_tile(objects, w, h, 0, z, tid)
        set_tile(objects, w, h, w - 1, z, tid)


def rect(layer, w, h, x0, z0, x1, z1, tid, only_if=None):
    for z in range(z0, z1 + 1):
        for x in range(x0, x1 + 1):
            set_tile(layer, w, h, x, z, tid, only_if=only_if)


def write_map(path, w, h, layers):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(struct.pack("<i", w))
        f.write(struct.pack("<i", h))
        f.write(struct.pack("<i", len(layers)))
        for layer in layers:
            name = layer["name"].encode("utf-8")
            f.write(struct.pack("<i", len(name)))
            f.write(name)
            f.write(struct.pack("<f", layer["elevation"]))
            f.write(struct.pack("<%di" % (w * h), *layer["tiles"]))
    print("geschrieben: %s (%dx%d, %d Layer)" % (path, w, h, len(layers)))


def build_map001():
    """Dorfrand: Dorfplatz mit Wegekreuz, Teich, Haus + Truhen-Ecke."""
    w, h = 25, 20
    ground = new_layer("Ground", w, h, 0.0, K_GRASS)
    objects = new_layer("Objects", w, h, 0.02, -1)

    # Wegekreuz (Spawn liegt auf der Kreuzung 12/10)
    for x in range(1, w - 1):
        set_tile(ground, w, h, x, 10, K_DIRT)
    for z in range(10, h - 1):
        set_tile(ground, w, h, 12, z, K_DIRT)

    # Teich links oben mit Sand-Ufer
    ellipse(ground, w, h, 5, 5, 3, 3, K_WATER, 1.45)

    # Hohes Gras (Durchwiese, hoehere Begegnungsrate)
    rect(ground, w, h, 16, 2, 18, 4, K_TALL_GRASS)
    rect(ground, w, h, 2, 14, 5, 16, K_TALL_GRASS)

    # Hausvorplatz
    set_tile(ground, w, h, 20, 5, K_DIRT)

    border_ring(objects, w, h, K_STONE)
    # Haus (3x2 Steinblock)
    rect(objects, w, h, 19, 3, 21, 4, K_STONE)
    # Felsen-Deko (nur auf freiem Gras)
    for rx, rz in ((8, 13), (9, 14), (16, 13), (4, 12)):
        if ground["tiles"][rz * w + rx] == K_GRASS:
            set_tile(objects, w, h, rx, rz, K_ROCK)
    return w, h, [ground, objects]


def build_map002():
    """Waldweg: schmaler Pfad von Sued nach Nord, Seen, viel hohes Gras."""
    w, h = 19, 15
    ground = new_layer("Ground", w, h, 0.0, K_GRASS)
    objects = new_layer("Objects", w, h, 0.02, -1)

    # Weg: Suedeingang (9,13) bis Nord (9,1) + Querweg in der Mitte
    for z in range(1, h - 1):
        set_tile(ground, w, h, 9, z, K_DIRT)
    for x in range(1, w - 1):
        set_tile(ground, w, h, x, 6, K_DIRT)

    # See rechts unten + Tümpel links oben
    ellipse(ground, w, h, 14, 10, 2, 2, K_WATER, 1.5)
    ellipse(ground, w, h, 4, 3, 2, 1, K_WATER, 1.6)

    # Hohes Gras
    rect(ground, w, h, 2, 8, 5, 11, K_TALL_GRASS)
    rect(ground, w, h, 12, 2, 16, 4, K_TALL_GRASS)

    border_ring(objects, w, h, K_STONE)
    # Felswand links (Kanalisierung des Weges)
    rect(objects, w, h, 6, 2, 6, 4, K_STONE)
    # Felsen-Deko
    for rx, rz in ((3, 10), (15, 13), (12, 8)):
        if ground["tiles"][rz * w + rx] == K_GRASS:
            set_tile(objects, w, h, rx, rz, K_ROCK)
    return w, h, [ground, objects]


def main():
    root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    maps_dir = os.path.join(root, "SampleProject", "maps")
    for builder, filename in ((build_map001, "map1.map"), (build_map002, "map2.map")):
        w, h, layers = builder()
        write_map(os.path.join(maps_dir, filename), w, h, layers)
    return 0


if __name__ == "__main__":
    sys.exit(main())
