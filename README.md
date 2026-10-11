# Plants vs. Zombies para PSP

Este repositorio tiene dos versiones del juego para la **PSP**:

* **Versión J2ME** (raíz: `src/`, `Makefile`): juego nativo a 60 fps rehecho a partir del PvZ de móviles (J2ME), con
  plantas y zombis añadidos de la versión Tencent y del PvZ de PC. Las plantas y zombis que se veían mal ya usan las
  **animaciones del PC** (guisantralla, seta melancólica, grano de café, zombi globo, buzo y delfín), dibujadas con
  la misma lógica que el PC (`Zombie::GetDrawPos`, `AttachToAnotherReanimation`...). Ver [PROGRESO.md](PROGRESO.md).
* **Versión PC** (`pcport/`): port del *Plants vs. Zombies* de PC, basado en
  [PvZ-Portable](https://github.com/wszqkzqk/PvZ-Portable) (reimplementación libre del PvZ GOTY, LGPL-3.0).

> El repositorio no contiene ningún recurso del juego (EA / PopCap): las dos versiones necesitan los datos de una
> copia propia.

## Versión J2ME

    make                    # necesita pspdev
    ./build_dist.sh         # arma dist/PSP/GAME/PVZ y PvZ_PSP_J2ME.zip

Los datos van en `data/` (`gfx.pak`, `anim.pak`, `sfx.pak`, `music/`). Las animaciones del PC se añaden a
`gfx.pak` y `anim.pak` con `tools/add_pc.py`, a partir del `main.pak` de la GOTY extraído (carpeta con `reanim/`):

    python3 tools/add_pc.py gfx_j2me.pak anim_j2me.pak goty_extraida data/gfx.pak data/anim.pak src/pc_anims.h \
        SplitPea GloomShroom Coffeebean Zombie_balloon Zombie_snorkle Zombie_dolphinrider \
        +Zombie_balloon_outerarm_upper2 +Zombie_snorkle_outerarm_upper2 +Zombie_dolphinrider_outerarm_upper2

(siempre desde los `.pak` originales del J2ME y con todos los nombres a la vez: escribe también `src/pc_anims.h`,
que hay que volver a compilar).

## Versión PC (`pcport/`)

Funciona con los datos de una copia propia del juego: `main.pak` y la carpeta `properties/` de la GOTY
(con Zombatar y logros) o los archivos de la versión 1.0.

* `pcport/` — el código del juego con los cambios para la PSP. Detalles en [pcport/PSP.md](pcport/PSP.md).
* `tools/pcport/` — herramientas para los datos: música pre-renderizada para la PSP, empaquetar `main.pak` y pasar
  a XML las animaciones y partículas de la versión 1.0. Ver [tools/pcport/README.md](tools/pcport/README.md).

### Compilar

    cd pcport/psp && make          # necesita pspdev (GCC 15, SDL2, libpng, libjpeg, zlib)

### Instalar

En `ms0:/PSP/GAME/PVZPC/`: `EBOOT.PBP`, `main.pak`, la carpeta `properties/` y la carpeta `music/` que genera
`tools/pcport/render_music.py` a partir de `sounds/mainmusic.mo3`. Las partidas y la caché van en `savedata/`.

### Controles (como en las versiones de consola)

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
