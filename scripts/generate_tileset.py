from PIL import Image, ImageDraw

img = Image.new('RGBA', (256, 256))
draw = ImageDraw.Draw(img)

colors = [
    (34, 139, 34),    # Gras
    (139, 69, 19),    # Erde
    (128, 128, 128),  # Stein
    (0, 105, 148),    # Wasser
    (194, 178, 128),  # Sand
    (160, 82, 45),    # Holz
    (255, 215, 0),    # Gold
    (75, 0, 130),     # Dunkel
]

tile = 32
for y in range(8):
    for x in range(8):
        c = colors[(y * 8 + x) % len(colors)]
        draw.rectangle([x*tile, y*tile, (x+1)*tile-1, (y+1)*tile-1], fill=c)
        draw.rectangle([x*tile, y*tile, (x+1)*tile-1, (y+1)*tile-1], outline=(0,0,0))

img.save('/home/user/rpgmaker3d/assets/textures/tileset_demo.png')
print('Created tileset_demo.png')
