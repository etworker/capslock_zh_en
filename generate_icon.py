"""
Generate a custom tray icon for CapsLock Zh<->En.
Design: a keyboard key split into two halves —
  left:  blue  with "En"
  right:  red   with "中"
Output: bin/capslock.ico  (multi-resolution)
"""
import math
import os
from PIL import Image, ImageDraw, ImageFont

OUT_DIR = os.path.join(os.path.dirname(__file__), "bin")
OUT_ICO = os.path.join(OUT_DIR, "capslock.ico")

# Colors
BG_LEFT   = (41, 128, 235)   # blue
BG_RIGHT  = (235, 60, 60)    # red
KEY_BORDER= (230, 230, 230)
TEXT_LIGHT= (255, 255, 255)
SHADOW    = (0, 0, 0, 40)

# Font paths (try common Windows CJK fonts)
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\msyh.ttc",    # Microsoft YaHei
    r"C:\Windows\Fonts\simhei.ttf",   # SimHei
    r"C:\Windows\Fonts\simsun.ttc",   # SimSun
]

def find_font(size):
    for p in FONT_CANDIDATES:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

def rounded_gradient_key(size):
    """Draw a keyboard key: rounded rect, left-blue / right-red."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    r = max(2, size // 6)           # corner radius
    pad = max(1, size // 16)        # padding
    x0, y0 = pad, pad
    x1, y1 = size - pad, size - pad

    # Drop shadow
    sh = Image.new("RGBA", (size, size), (0,0,0,0))
    sd = ImageDraw.Draw(sh)
    sd.rounded_rectangle([x0+1, y0+2, x1+1, y1+2], radius=r, fill=(0,0,0,60))
    img = Image.alpha_composite(img, sh)
    draw = ImageDraw.Draw(img)

    # Left half (blue)
    draw.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=BG_LEFT)
    # Right half (red) — clip to rounded shape
    overlay = Image.new("RGBA", (size, size), (0,0,0,0))
    od = ImageDraw.Draw(overlay)
    od.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=BG_RIGHT)
    # Erase left half of overlay
    mask = Image.new("L", (size, size), 0)
    md = ImageDraw.Draw(mask)
    md.rectangle([size//2, 0, size, size], fill=255)
    overlay.putalpha(mask)
    img = Image.alpha_composite(img, overlay)
    draw = ImageDraw.Draw(img)

    # Border
    draw.rounded_rectangle([x0, y0, x1, y1], radius=r, outline=KEY_BORDER,
                           width=max(1, size//64))

    # Center divider line
    cx = size // 2
    draw.line([cx, y0+r//2, cx, y1-r//2], fill=TEXT_LIGHT, width=max(1, size//64))

    # Text "En" (left) and "中" (right)
    if size >= 32:
        font_size = max(10, size // 4)
        font = find_font(font_size)

        # "En" on left half
        text_en = "En"
        bbox = draw.textbbox((0,0), text_en, font=font)
        tw = bbox[2]-bbox[0]; th = bbox[3]-bbox[1]
        tx = (size//4) - tw//2 - bbox[0]
        ty = (size - th)//2 - bbox[1] - size//32
        draw.text((tx, ty), text_en, fill=TEXT_LIGHT, font=font)

        # "中" on right half
        text_zh = "中"
        bbox = draw.textbbox((0,0), text_zh, font=font)
        tw = bbox[2]-bbox[0]; th = bbox[3]-bbox[1]
        tx = (3*size//4) - tw//2 - bbox[0]
        ty = (size - th)//2 - bbox[1] - size//32
        draw.text((tx, ty), text_zh, fill=TEXT_LIGHT, font=font)
    elif size >= 16:
        # Tiny: just draw letters without font
        s = max(5, size//3)
        draw.text((size//4 - s//3, size//2 - s//2), "E", fill=TEXT_LIGHT)
        # Simplified "中" as a cross for tiny sizes
        cx2 = 3*size//4
        cy = size//2
        w = max(1, size//16)
        draw.line([cx2-s//2, cy, cx2+s//2, cy], fill=TEXT_LIGHT, width=w)
        draw.line([cx2, cy-s//2, cx2, cy+s//2], fill=TEXT_LIGHT, width=w)
        draw.rectangle([cx2-s//4, cy-s//4, cx2+s//4, cy+s//4], outline=TEXT_LIGHT, width=w)

    return img

def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    sizes = [16, 20, 24, 32, 40, 48, 64, 128, 256]
    images = [rounded_gradient_key(s) for s in sizes]

    # Save as multi-resolution ICO (pass all images, no sizes= to let
    # Pillow use each image at its native resolution)
    images[-1].save(OUT_ICO, format="ICO", append_images=images[:-1])
    print(f"[OK] {OUT_ICO}")

    # Also save a large PNG preview
    preview = rounded_gradient_key(256)
    preview_path = os.path.join(OUT_DIR, "capslock_preview.png")
    preview.save(preview_path)
    print(f"[OK] {preview_path}")

if __name__ == "__main__":
    main()
