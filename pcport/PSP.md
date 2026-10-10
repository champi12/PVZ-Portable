# PvZ de PC en PSP (basado en PvZ-Portable)

Este directorio es una copia de [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (LGPL-3.0) con los
cambios para la PSP. No contiene ningún recurso del juego.

## Compilar
    cd pcport/psp && make          # necesita pspdev (GCC 15, SDL2, libpng, libjpeg, zlib)

## Instalar
Copia `EBOOT.PBP` a `ms0:/PSP/GAME/PVZPC/` y, en la misma carpeta, los datos de tu copia del juego:
- GOTY: `main.pak` y la carpeta `properties/` (recomendado; incluye logros y Zombatar).
- PvZ 1.0: las carpetas `data images particles properties reanim sounds compiled`, después de pasar
  `tools/pcport/reanim_decompile.py` y `particle_decompile.py` (o empaquetadas con `tools/pcport/make_pak.py`).
- Música: la carpeta `music/` que crea `tools/pcport/render_music.py sounds/mainmusic.mo3 music`
  (necesita libopenmpt en el PC). Sin ella el juego va sin música.

## Cambios para la PSP
- `psp/gl_gu.cpp`: las ~40 funciones de OpenGL ES 2 que usa `GLInterface.cpp`, hechas con sceGu (sin shaders).
  Texturas a mitad de resolución y en 16 bits; lectura de texturas para lo que dibuja por software.
- `platform/psp/`: ventana (sceGu) y controles: cursor con el stick (R = rápido), X = clic, O = clic derecho,
  START = menú, CUADRADO = pausa, SELECT = zoom. La pantalla de 800x600 se ve entera (escala 272/600) o, con
  zoom, a 0,6 con la vista siguiendo al cursor. En un nivel, como en consola: la cruceta salta por casillas,
  L/R eligen sobre, TRIÁNGULO = pala y los soles se recogen al pasar por encima.
- Velocidad: CPU a 333 MHz, tope de 30 fps sin esperar al refresco si el fotograma va tarde, constantes en
  coma flotante simple (`-fsingle-precision-constant`).
- Caché en la carpeta de datos (`cache32/`): animaciones compiladas, texturas ya reducidas (`tex/`) y sonidos
  decodificados (`snd22050_1/`). La primera carga las crea; las siguientes tardan menos de la mitad.
- `LOW_MEMORY`, sin atlas de animaciones, imágenes sin copia en memoria normal tras crear su textura,
  sonido a 22 kHz mono, `int32_t` como `int` (newlib de la PSP usa `long`).
- `ResourceManager.cpp`: los recursos que solo tiene la GOTY no detienen el juego con los datos 1.0.
- Música: el `.mo3` decodificado ocupa ~26 MB, así que cada melodía va pre-renderizada en pistas IMA ADPCM
  (melodía, tambores, platillos) y el códec `SDL-Mixer-X/src/codecs/music_pvzm.c` las mezcla imitando el
  salto de orden y el volumen por canal que pide `Music.cpp`.
- Las animaciones se cargan al usarse (no todas al empezar) y sin la película de introducción.

## Estado
Probado en PPSSPP con los datos 1.0 (sueltos y en `main.pak`): logo, título, carga, nombre de usuario con el
teclado de la PSP, menú principal y nivel 1-1 de Aventura jugable (memoria: menú ~18 MB, nivel ~23 MB).
Pendiente: probar en una PSP real (velocidad), controles más cómodos, resto de niveles y minijuegos.
