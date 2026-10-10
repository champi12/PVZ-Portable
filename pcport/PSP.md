# PvZ de PC en PSP (basado en PvZ-Portable)

Este directorio es una copia de [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (LGPL-3.0) con los
cambios para la PSP. No contiene ningún recurso del juego.

## Compilar
    cd pcport/psp && make          # necesita pspdev (GCC 15, SDL2, libpng, libjpeg, zlib)

## Instalar
Copia `EBOOT.PBP` a `ms0:/PSP/GAME/PVZPC/` y, en la misma carpeta, los datos de tu copia del juego:
- GOTY: `main.pak` y la carpeta `properties/` (recomendado; incluye logros y Zombatar).
- PvZ 1.0: las carpetas `data images particles properties reanim sounds compiled`, después de pasar
  `tools/pcport/reanim_decompile.py` y `particle_decompile.py`.

## Cambios para la PSP
- `psp/gl_gu.cpp`: las ~40 funciones de OpenGL ES 2 que usa `GLInterface.cpp`, hechas con sceGu (sin shaders).
  Texturas a mitad de resolución y en 16 bits; lectura de texturas para lo que dibuja por software.
- `platform/psp/`: ventana (sceGu) y controles: cursor con stick o cruceta (R = rápido), X = clic,
  O = clic derecho, START = menú, SELECT = pausa. La pantalla de 800x600 se ve a 0,6 (480x360) y la vista
  sube y baja con el cursor.
- `LOW_MEMORY`, sin atlas de animaciones, imágenes sin copia en memoria normal tras crear su textura,
  sonido a 22 kHz mono, `int32_t` como `int` (newlib de la PSP usa `long`).
- `ResourceManager.cpp`: los recursos que solo tiene la GOTY no detienen el juego con los datos 1.0.

## Estado
Arranca: logo de PopCap, pantalla de título y carga de recursos (probado en PPSSPP con los datos 1.0).
Pendiente: memoria durante la partida, música (.mo3), velocidad, controles más cómodos.
