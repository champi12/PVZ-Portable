# Port del PvZ de PC a PSP (en preparación)

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

## Pendiente para la PSP
- Compilar PvZ-Portable con la toolchain de PSP (GCC 15, SDL2, libpng, libjpeg, zlib, vorbis ya disponibles).
- Sustituir el renderizador OpenGL ES 2 (`GLInterface.cpp`) por uno con sceGu.
- Memoria: texturas reducidas (x0.6) y de 16 bits o con paleta, cargadas y descargadas por grupos.
- Pantalla 800x600 (4:3) en 480x272 (16:9).
- Música: los .mo3 pasados a OGG/MP3.
