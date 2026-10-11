#!/bin/bash
# Prueba automatica en PPSSPP sin pantalla (el juego se juega solo, modo AUTOTEST).
#   tools/ppsspp_test.sh <nivel 0-49> <segundos> <carpeta_salida> [flags extra]
# Necesita PPSSPPHeadless (variable PPSSPP_HEADLESS) y el toolchain pspdev en el PATH.
# Guarda una captura cada 600 fotogramas (shotNNNNNNN.raw, 480x272 RGBA) y una hoja
# de contacto sheet.png (requiere Python con Pillow).
set -e
cd "$(dirname "$0")/.."
LEVEL=${1:-0}; SECS=${2:-120}; OUT=$(realpath -m ${3:-autotest_out})
PPSSPP_HEADLESS=${PPSSPP_HEADLESS:-PPSSPPHeadless}
make clean >/dev/null
make -j4 AUTOTEST=3 EXTRA="-DAT_LEVEL=$LEVEL -DAT_SHOT_EVERY=${SHOT:-600} $4" 2>&1 | grep -E "error" || true
rm -rf "$OUT"; mkdir -p "$OUT/data"
cp EBOOT.PBP "$OUT/"; cp data/*.pak "$OUT/data/"; cp -r data/music "$OUT/data/"
(cd "$OUT" && timeout $((SECS * 10)) "$PPSSPP_HEADLESS" "$OUT/EBOOT.PBP" --graphics=software --timeout=$SECS -l 2>&1 \
    | grep -o "stdout: .*" | awk 'NR%2==1' | tail -20) || true
python3 - "$OUT" <<'PY' || true
import sys, glob, os
from PIL import Image, ImageDraw
d = sys.argv[1]
fs = sorted(glob.glob(d + '/shot*.raw'))
if not fs: sys.exit()
sel = fs[::max(1, len(fs) // 12)][:12]
sheet = Image.new('RGB', (480 * 3, 286 * ((len(sel) + 2) // 3)))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(sel):
    im = Image.frombuffer('RGBA', (480, 272), open(f, 'rb').read(), 'raw', 'RGBA', 0, 1).convert('RGB')
    x, y = (i % 3) * 480, (i // 3) * 286
    sheet.paste(im, (x, y + 14)); dr.text((x + 3, y + 1), os.path.basename(f), fill=(255, 255, 0))
sheet.save(d + '/sheet.png'); print(len(fs), 'capturas ->', d + '/sheet.png')
PY
