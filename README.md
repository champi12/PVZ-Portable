# Plants vs. Zombies (PC) para PSP

Port del *Plants vs. Zombies* de PC a la **PSP**, basado en
[PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (reimplementación libre del PvZ GOTY, LGPL-3.0).
Funciona con los datos de una copia propia del juego: `main.pak` y la carpeta `properties/` de la GOTY
(con Zombatar y logros) o los archivos de la versión 1.0.

> El repositorio no contiene ningún recurso del juego (EA / PopCap).

* `pcport/` — el código del juego con los cambios para la PSP. Detalles en [pcport/PSP.md](pcport/PSP.md).
* `tools/pcport/` — herramientas para los datos: música pre-renderizada para la PSP, empaquetar `main.pak` y pasar
  a XML las animaciones y partículas de la versión 1.0. Ver [tools/pcport/README.md](tools/pcport/README.md).

## Compilar

    cd pcport/psp && make          # necesita pspdev (GCC 15, SDL2, libpng, libjpeg, zlib)

## Instalar

En `ms0:/PSP/GAME/PVZPC/`: `EBOOT.PBP`, `main.pak`, la carpeta `properties/` y la carpeta `music/` que genera
`tools/pcport/render_music.py` a partir de `sounds/mainmusic.mo3`. Las partidas y la caché van en `savedata/`.

## Controles (como en las versiones de consola)

| PSP | Acción |
|---|---|
| Cruceta / stick | Mover la selección (en un nivel, de casilla en casilla) |
| ✕ | Aceptar / plantar |
| ◯ | Volver (en un nivel: soltar la planta o la pala) |
| L / R | Planta anterior / siguiente (en un nivel) |
| △ | Pala |
| □ | Pausa |
| START | Menú |
| SELECT | Vista: 16:9, 4:3 con bandas o zoom |

Los soles y monedas se recogen al pasar la selección por encima. Arriba a la izquierda hay un contador de
fotogramas (FPS) y de actualizaciones del juego por segundo (L, lo normal es 100).
