from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

BASE = Path(__file__).resolve().parent
ROOT = BASE / 'assets' / 'ford_ipc'
SRC = ROOT / 'host_vehicle.png'
im = Image.open(SRC).convert('RGBA')
W,H = im.size

# Polygon masks are intentionally based on the genuine top-down Escape sprite.
# They isolate the movable body panels while retaining the factory shading.
panels = {
    'vehicle_door_df': [(5,67),(25,64),(28,112),(7,117)],
    'vehicle_door_pf': [(65,64),(85,67),(83,117),(62,112)],
    'vehicle_door_rl': [(7,116),(28,111),(30,158),(10,164)],
    'vehicle_door_rr': [(62,111),(83,116),(80,164),(60,158)],
    'vehicle_hood': [(13,4),(77,4),(79,57),(11,57)],
    'vehicle_liftgate': [(14,145),(76,145),(78,188),(12,188)],
}

# Build a base image with the movable panels removed and dark recessed cavities
# underneath. Closed panel sprites are composited over these cavities at runtime,
# so the closed state reconstructs the original appearance closely.
base = im.copy()
base_draw = ImageDraw.Draw(base, 'RGBA')

# Interior/recess colors chosen from the existing dark greenhouse/trim in the sprite.
for name, poly in panels.items():
    mask = Image.new('L', (W,H), 0)
    md = ImageDraw.Draw(mask)
    md.polygon(poly, fill=255)
    # Feather one pixel to keep panel edges smooth when supersampled.
    soft = mask.filter(ImageFilter.GaussianBlur(0.55))

    # Panel texture: preserve the exact original pixels under the polygon.
    panel = Image.new('RGBA', (W,H), (0,0,0,0))
    panel.paste(im, (0,0), soft)
    panel.save(ROOT / f'{name}.png')

    # Shadow texture derived from the panel alpha. Runtime offsets/rotates this
    # slightly behind the moving panel.
    shadow_alpha = soft.filter(ImageFilter.GaussianBlur(1.7)).point(lambda p: int(p*0.65))
    shadow = Image.new('RGBA',(W,H),(0,0,0,0))
    shadow.putalpha(shadow_alpha)
    shadow.save(ROOT / f'{name}_shadow.png')

    # Paint the opening underneath. Doors get a deep cabin opening, hood an engine
    # bay, and liftgate a cargo opening. Slight outline preserves body separation.
    if 'hood' in name:
        fill=(8,11,13,255); outline=(32,38,43,255)
    elif 'liftgate' in name:
        fill=(7,9,11,255); outline=(28,34,39,255)
    else:
        fill=(5,8,10,255); outline=(25,31,35,255)
    base_draw.polygon(poly, fill=fill)
    base_draw.line(poly+[poly[0]], fill=outline, width=1)

base.save(ROOT/'vehicle_base.png')
print('generated vehicle panel assets from', SRC)
