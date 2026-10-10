# Herramientas del port del PvZ de PC a PSP

Base: [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (reimplementación libre del PvZ GOTY, LGPL-3.0,
SDL2 + OpenGL ES 2). No incluye ningún recurso del juego: hacen falta los archivos de una copia propia.

## Datos del PvZ 1.0 (no GOTY)
La versión 1.0 trae las animaciones y partículas solo compiladas (volcados de memoria de 32 bits), y PvZ-Portable
no las acepta. Estos scripts las pasan al XML original:

    python3 reanim_decompile.py   "<carpeta del juego>"   # compiled/reanim/*.reanim.compiled -> reanim/*.reanim
    python3 particle_decompile.py "<carpeta del juego>"   # compiled/particles/*.compiled -> particles/*.xml / *.trail

Además, PvZ-Portable necesita un cambio en `ResourceManager.cpp` para que las imágenes, sonidos y fuentes que
solo existen en la GOTY (logros, Zombatar, Quick Play...) no detengan el juego (se usa IMAGE_BLANK).
Con eso, el juego de PC arranca hasta la pantalla de título en Linux con los datos 1.0.

## Música para la PSP
El `.mo3` decodificado no cabe en la memoria de la PSP. `render_music.py` graba cada melodía en pistas IMA ADPCM
(necesita libopenmpt en el PC):

    python3 render_music.py "<carpeta del juego>/sounds/mainmusic.mo3" music

## Empaquetar datos sueltos
    python3 make_pak.py main.pak "<carpeta del juego>" compiled data images particles reanim sounds
