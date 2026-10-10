#!/bin/bash
# Arma dist/PSP/GAME/PVZ lista para copiar a la Memory Stick o abrir en PPSSPP
set -e
cd "$(dirname "$0")"
D=dist/PSP/GAME/PVZ
rm -rf dist; mkdir -p $D/data/music
cp EBOOT.PBP $D/
cp data/gfx.pak data/sfx.pak data/anim.pak $D/data/
cp data/music/*.mp3 $D/data/music/
du -sh $D
rm -f PvZ_PSP_J2ME.zip; (cd dist && zip -qr ../PvZ_PSP_J2ME.zip PSP)   # zip listo para mandar
