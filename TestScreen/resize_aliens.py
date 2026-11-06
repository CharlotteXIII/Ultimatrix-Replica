"""
Resize alien PNGs to 240x240 for the M5Dial screen.

- Reads every .png in SRC_DIR (other files are ignored)
- Pads to a square with black (no stretching), flattens transparency onto black
- Resizes to 240x240 and saves a PNG with the same name into DST_DIR
- Prints each file size and the total, so you can compare with the LittleFS space

Usage (run from the sketch folder):
    python resize_aliens.py alien_original data
"""
import os
import sys
from PIL import Image

SIZE = 240
BG = (0, 0, 0, 255)   # black, same as COLOR_BG_BLACK


def convert(src_path, dst_path):
    with Image.open(src_path) as img:
        img = img.convert("RGBA")
        side = max(img.size)
        canvas = Image.new("RGBA", (side, side), BG)
        canvas.paste(img, ((side - img.width) // 2, (side - img.height) // 2), img)
        canvas.convert("RGB").resize((SIZE, SIZE), Image.Resampling.LANCZOS).save(dst_path, optimize=True)


def main(src_dir, dst_dir):
    os.makedirs(dst_dir, exist_ok=True)
    total = 0
    for entry in sorted(os.scandir(src_dir), key=lambda e: e.name):
        if not (entry.is_file() and entry.name.lower().endswith(".png")):
            continue
        dst_path = os.path.join(dst_dir, entry.name)
        convert(entry.path, dst_path)
        size = os.path.getsize(dst_path)
        total += size
        print(f"{entry.name:20s} {size / 1024:7.1f} KB")
    print(f"TOTAL (PNG only): {total / 1024:.1f} KB")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])