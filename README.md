# Plantas contra Zombis para PSP

Port nativo para **PSP** de *Plants vs. Zombies*: gráficos, fuentes y animaciones de la
versión J2ME 4.6.0 (EA), con la **jugabilidad, las hordas, la música y los efectos de la
versión de PC**. Motor propio en C (sceGu + sceMp3, sin SDL) a **60 fps**.

* Vista del jardín igual que el J2ME (480×320 a 1:1): la casa a la izquierda, el césped
  desde x=171 y la acera a la derecha, por donde entran los zombis.
* Sin pantallas de carga al entrar en un nivel ni entre niveles.
* 50 niveles de la aventura del J2ME, 31 plantas y 21 zombis con valores del PC.
* Objetivo: PSP-1000 (32 MB, 333 MHz).

El historial detallado del desarrollo está en [PROGRESO.md](PROGRESO.md).

> El repositorio no contiene el contenido del juego (EA / PopCap). Para compilar hace
> falta la carpeta `data/` (`gfx.pak`, `anim.pak`, `sfx.pak` y `music/*.mp3`), que se genera
> con las herramientas de `tools/` a partir del `.jar` del J2ME y de los sonidos del PvZ de
> PC (ver PROGRESO.md).

## Controles

| PSP | Acción |
|---|---|
| Cruceta / stick | Mover el cursor por las casillas (arriba/abajo del todo: caja de semillas) |
| ✕ | Abrir la caja de semillas / elegir semilla / plantar |
| ◯ | Cancelar |
| △ | Pala |
| L / R | Cambiar de sobre |
| START | Pausa |

Los soles se recogen solos al pasar el cursor cerca.

## Compilar

Toolchain [pspdev](https://github.com/pspdev/pspdev) con `psp-config` en el `PATH`:

```sh
make                # -> EBOOT.PBP
./build_dist.sh     # -> dist/PSP/GAME/PVZ/ (EBOOT.PBP + data/)
```

Copia `dist/PSP/GAME/PVZ/` a `ms0:/PSP/GAME/PVZ/` en la Memory Stick.

## Pruebas automáticas

`tools/ppsspp_test.sh <nivel> <segundos> <carpeta>` compila en modo `AUTOTEST` (el juego se
juega solo), lo ejecuta en `PPSSPPHeadless` y guarda capturas y una hoja de contacto.

## Otras carpetas

* `j2me_traducido/`: la primera versión del port, que traduce automáticamente el bytecode
  del juego J2ME original a C++ (`jvm2cpp.py`). Es fiel al original pero va a ~6 fps, como
  en el móvil. Se mantiene como referencia.
