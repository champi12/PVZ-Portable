#!/bin/bash
# Renderiza mainmusic.mo3 (PvZ PC) por posiciones de orden y lo codifica a MP3 44.1k 96 kbps
# Requiere: libopenmpt (.so), ffmpeg con libmp3lame.  Uso: build_music.sh <ruta/mainmusic.mo3> <salida>
set -e
MO3="$1"; OUT="$2"; mkdir -p "$OUT" /tmp/mo3
gcc "$(dirname "$0")/mo3_render.c" -o /tmp/mo3/render /usr/lib/x86_64-linux-gnu/libopenmpt.so.0
/tmp/mo3/render "$MO3" /tmp/mo3/m 00 30 5E 7A 7D 98 9E A6 B1 B8 D4 DD
declare -A N=([00]=day_grasswalk [30]=night_moongrains [5E]=pool_waterygraves [7A]=choose_seeds [7D]=fog_rigormormist [98]=title_crazydave [9E]=boss_brainiacmaniac [A6]=minigame_loonboon [B1]=puzzle_cerebrawl [B8]=roof_grazetheroof [D4]=conveyer [DD]=zen_garden)
for k in "${!N[@]}"; do
  ffmpeg -hide_banner -loglevel error -y -f s16le -ar 44100 -ac 2 -i /tmp/mo3/m_$k.raw \
    -af "volume=5dB,alimiter=limit=0.89:level=false" -c:a libmp3lame -b:a 96k -write_xing 0 -id3v2_version 0 "$OUT/${N[$k]}.mp3"
done
