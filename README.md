# Plantas contra Zombis para PSP

Port nativo para **PSP** de *Plants vs. Zombies*: gráficos, fuentes y animaciones de la
versión J2ME 4.6.0 (EA), con la **jugabilidad, las hordas, la música y los efectos de la
versión de PC**. Motor propio en C (sceGu + sceMp3, sin SDL) a **60 fps**.

* Vista del jardín igual que el J2ME (480×320 a 1:1): la casa a la izquierda, el césped
  desde x=171 y la acera a la derecha, por donde entran los zombis.
* Sin pantallas de carga al entrar en un nivel ni entre niveles.
* Como el J2ME: menú de la lápida (aventura, opciones, almanaque, elegir nivel, acerca de),
  intro de cada nivel con la cámara yendo a la calle para ver los zombis, elección de
  plantas en el panel "¡ELIGE TUS PLANTAS!", cortacésped que entran rodando y la barra de
  semillas que baja, y efectos de partículas del J2ME (¡SPUDOW!, ¡POWIE!, ¡DOOM!, llamas,
  humo, salpicaduras de guisante, trozos de cono y cubo...).
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
| Cruceta / stick | Mover el cursor por las casillas (izquierda en la primera columna: caja de semillas, como el PvZBV) |
| ✕ | Abrir la caja de semillas / elegir semilla / plantar |
| ◯ | Cancelar |
| △ | Pala |
| L / R | Siguiente sobre que se puede plantar |
| ✕ sobre el mazorcañón | Apuntar (luego ✕ en la casilla de destino) |
| SELECT | Última resistencia: empezar el asalto |
| START | Pausa |

Los soles se recogen solos al pasar el cursor cerca.

**¡Elige tus plantas!** (cuando tienes más plantas que huecos): cuadrícula de 4 columnas; ✕ elige o
quita; izquierda desde la primera columna lleva el cursor a las elegidas para quitarlas con ✕;
△ empieza la partida; START abre la pausa (reanudar, reiniciar, menú, sonido).

**Idiomas**: los 6 del J2ME (inglés, francés, alemán, italiano, portugués y español). Se usa el idioma
de la consola y se puede cambiar en Opciones.

**Minijuegos** (menú principal): bolos con nueces, combate de portales, última resistencia, zombis
invisibles y zombis veloces (de la versión Tencent del J2ME).

**Menú**: cruceta y ✕; ◯ va a "salir". En el almanaque, ◀ ▶ pasan de ficha y ▲ ▼ desplazan el texto.

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
