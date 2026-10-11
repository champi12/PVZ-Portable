# Plants vs. Zombies (J2ME) para PSP

Port nativo para **PSP** del juego para móviles *Plants vs. Zombies 4.6.0* (versión J2ME
táctil, `Plants_vs._Zombies_4.6.0_LockedRes_320x480_Touch_En.jar`), en pantalla ancha
**480×272** y **sin pantallas de carga** al entrar en una partida ni entre niveles.

> Este repositorio **no contiene el juego**: ni el código ni los gráficos de EA/PopCap.
> Contiene las herramientas que convierten *tu* copia del `.jar` en un `EBOOT.PBP` nativo.

## Cómo funciona

El código Java del juego (bytecode ofuscado de 99 clases) se **traduce automáticamente a
C++** con `tools/jvm2cpp.py`: cada método Java se convierte en una función C++ normal, que
`psp-g++` compila a código MIPS nativo. No hay emulador ni máquina virtual Java en la PSP.

```
 .jar ──► tools/jvm2cpp.py ──► C++ (build/gen) ─┐
 runtime/java (CLDC/MIDP mínimo, Java) ──► C++ ─┼─► psp-g++ ──► EBOOT.PBP
 runtime/*.cpp + platform/platform_psp.cpp ─────┘
 datos del .jar ──► tools/pack_resources.py ──► incrustados en el EBOOT
```

| Carpeta | Contenido |
|---|---|
| `tools/jvm2cpp.py` | Traductor de bytecode JVM a C++ (vtables, interfaces, excepciones, inicialización de clases). |
| `runtime/java/` | Implementación propia y mínima de CLDC 1.0 / MIDP 2.0 (String, Canvas, Graphics, Image, RecordStore, Player...). También se traduce a C++. |
| `runtime/*.cpp` | Modelo de objetos, recolector de basura, renderizador por software, decodificador PNG/JPEG, entrada, sintetizador MIDI. |
| `game/` | Cambios específicos del juego: pantalla ancha, eliminación de cargas, presentación. |
| `platform/platform_psp.cpp` | Backend nativo de PSP (framebuffer, `sceCtrl`, `sceAudio`, Memory Stick). |
| `platform/platform_sdl.cpp` | Backend SDL2 para PC (desarrollo y pruebas automáticas). |

### Cambios respecto al original

* **Pantalla ancha 16:9.** El juego del móvil dibuja en un lienzo de 480×320. En la PSP el
  lienzo se amplía a **564×320** (proporción 16:9) y se escala de forma uniforme a 480×272:
  la imagen no se deforma y se ve más jardín (incluida la calle por la que llegan los
  zombis). Las pantallas cuyo arte sólo mide 480 px (menú principal) se muestran a pantalla
  completa a partir de esa zona.
* **Sin pantallas de carga.** Al empezar un nivel, reintentar o pasar al siguiente, todo se
  carga en un único fotograma y la pantalla de carga no llega a mostrarse. La carga inicial
  al arrancar el juego ya no tiene la duración mínima artificial de 5 s.
* **Controles de PSP** en lugar de la pantalla táctil (ver abajo).
* **Música**: los MIDI del juego se reproducen con un sintetizador por software.
* **Partidas guardadas** en `saves/` junto al EBOOT.

## Controles

| PSP | Acción |
|---|---|
| Stick analógico / cruceta | Mover el cursor (dedo) |
| ✕ | Tocar la pantalla (mantener y mover = arrastrar) |
| ◯ / START | Atrás / pausa |
| △ | Tecla de selección del móvil |
| R (mantener) | Cursor rápido |
| L (mantener) | Acelerar el juego ×2 |
| HOME | Salir |

## Compilar

Necesitas tu copia del `.jar`, Python 3, Java (JDK 8 o superior, sólo para `javac`) y el
toolchain [pspdev](https://github.com/pspdev/pspdev) con `psp-config` en el `PATH`.

```sh
# PSP
make -f Makefile.psp JAR=/ruta/Plants_vs._Zombies_4.6.0_LockedRes_320x480_Touch_En.jar
# -> build/psp/EBOOT.PBP

# PC (SDL2), para probar
make JAR=/ruta/al/juego.jar
./build/pc/pvz        # ratón = dedo; Z = ✕, X = ◯, flechas, Q = L, W = R
```

### Instalar en la PSP

Copia `build/psp/EBOOT.PBP` a `ms0:/PSP/GAME/PVZ/EBOOT.PBP` en la Memory Stick y ejecútalo
desde *Juego → Memory Stick*. Necesita un firmware que permita homebrew (CFW / LME / ARK).

## Rendimiento

En PSP el lienzo del juego se dibuja con la GPU (GE) como textura escalada con filtro
bilineal; la CPU sólo ejecuta la lógica del juego y dibuja los sprites en el lienzo. Medido
en PPSSPP (CPU a 333 MHz) durante una partida: ~10–15 % de CPU para el juego y ~8 % para la
música. La lógica original va a ~6 fotogramas por segundo (cada 166 ms, como en el móvil);
el cursor se sigue redibujando a 60 Hz entre fotogramas.

`make -f Makefile.psp PROFILE=1 BUILD=build/psp-prof` genera un EBOOT de pruebas que
imprime el uso de CPU cada 5 s y lee entradas guionizadas de `autoplay.txt` (mismo formato
que abajo; `shot` guarda capturas `.raw` ARGB 480×272).

## Pruebas automáticas en PC

El ejecutable de PC puede jugar sin ventana con un reloj virtual y entradas guionizadas, y
guardar capturas:

```sh
PVZ_HEADLESS=1 PVZ_SCRIPT=guion.txt PVZ_SHOTDIR=capturas ./build/pc/pvz
# guion.txt:  "<ms> down X Y" | "<ms> up X Y" | "<ms> move X Y" | "<ms> press CROSS" |
#             "<ms> release CROSS" | "<ms> shot nombre" | "<ms> quit"
```

`tools/run_script.sh guion.txt carpeta` además monta una hoja de contacto con las capturas.
Variables de depuración: `PVZ_TRACE_EXC` (excepciones Java), `PVZ_DEBUG`, `PVZ_GC_DEBUG`,
`PVZ_INPUT_DEBUG`, `PVZ_STATE`.
