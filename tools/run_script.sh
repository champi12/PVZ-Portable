#!/bin/sh
# Runs the PC build headless with a scripted input file and builds a contact sheet of the
# screenshots it took:  tools/run_script.sh script.txt out_dir [columns]
set -e
SCRIPT=$1
OUT=$2
COLS=${3:-3}
mkdir -p "$OUT"
rm -f "$OUT"/*.png
PVZ_HEADLESS=1 PVZ_SCRIPT="$SCRIPT" PVZ_SHOTDIR="$OUT" PVZ_SAVEDIR="${PVZ_SAVEDIR:-$OUT/saves}" \
    timeout 300 ./build/pc/pvz 2>&1 | grep -v screenshot | sort | uniq -c | sort -rn | head -20
python3 - "$OUT" "$COLS" <<'PY'
import sys, os
from PIL import Image, ImageDraw
d, cols = sys.argv[1], int(sys.argv[2])
files = sorted(f for f in os.listdir(d) if f.endswith('.png') and not f.startswith('sheet'))
if not files:
    sys.exit(0)
W, H = 480, 272
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (W * cols, (H + 14) * rows), (40, 40, 40))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    x, y = (i % cols) * W, (i // cols) * (H + 14)
    sheet.paste(Image.open(os.path.join(d, f)), (x, y + 14))
    dr.text((x + 4, y + 1), f[:-4], fill=(255, 255, 0))
sheet.save(os.path.join(d, 'sheet.png'))
PY
