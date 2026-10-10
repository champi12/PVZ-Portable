# Plantas contra Zombis — port nativo para PSP

Base: **PvZ J2ME 4.6.0 (EA, 480x320 horizontal)** → gráficos, fuentes y animaciones.
Audio: **PvZ de PC** (carpeta `O.G PVZ 1/sounds`) → música y efectos originales.
Jugabilidad objetivo: **igual que la versión de PC** (tiempos, costes y recargas del PC).
Consola objetivo: **PSP-1000 "fat"** (32 MB de RAM, CPU a 333 MHz). Si va bien en la 1000, va en todas.

---

## Cómo compilar

1. Toolchain: [pspdev](https://github.com/pspdev/pspdev) (release `pspdev-ubuntu-latest-x86_64.tar.gz`, o la de Windows/WSL).
2. `export PSPDEV=<ruta>/pspdev; export PATH=$PSPDEV/bin:$PATH`
3. `make` → genera `EBOOT.PBP`.

## Cómo probar

- `make AUTOTEST=1` compila una versión que entra sola al jardín y planta de todo (para probar sin mandos,
  p. ej. con `PPSSPPHeadless --graphics=software --memstick=dist --timeout-emulated=4 --screenshot-save=x.png dist/PSP/GAME/PVZ/EBOOT.PBP`).
  Se verificó así en la sesión 1: título y jardín se ven bien; RAM usada en el jardín ≈ 600 KB.

Copia la carpeta `dist/PSP/GAME/PVZ/` a la Memory Stick (`ms0:/PSP/GAME/PVZ/`) o ábrela en PPSSPP.
Debe contener: `EBOOT.PBP` + `data/gfx.pak` + `data/sfx.pak` + `data/music/*.mp3`.

Controles (desde la sesión 2, copiados del PvZ J2ME de teclado **PvZBV.jar**, clases `be`, `w`, `cc`):
- Cruceta / stick: mover el cursor por las casillas. Arriba en la primera fila o abajo en la última → caja de semillas.
- X (= tecla 5): en casilla vacía abre la caja; sobre una planta abre la caja con ese sobre marcado.
  En la caja: ←/→ eligen, X toma la semilla y el cursor salta solo a una casilla útil. Con semilla en mano: X planta.
- Los **soles se recogen solos** al pasar el cursor cerca (3x3 casillas alrededor, como `w.a()` del J2ME).
- △ = pala (tecla blanda izquierda del J2ME; disponible tras el nivel 1-4). O = cancelar.
- START = pausa (tecla blanda derecha). L / R = cambiar de sobre directo (extra de PSP). SELECT = info de memoria.

---

## Estado

### ✅ Hito 1 (sesión 1) — base del motor
- Decompilado el .jar (CFR) → `ref/java_decompilado/` (99 clases, ~24.500 líneas, ofuscado).
- **Formato de gráficos descifrado** (`i0..i45` = paquete de 32 KB troceado):
  grupos `[u64 máscara][u32 len][PNG/JPEG]` + imágenes base (12 bytes c/u: u64, flags, u16 paleta, u8)
  + imágenes derivadas (ops: 0 recorte, 1 rotación con seno/coseno 16.16, 2 escala %). 605 imágenes.
  Paletas alternativas en `/p`. Extractor: `tools/extract_imgs.py` → `ref/imgs/NNN.png` + `meta.json`.
- **Fuentes** (`/f`) descifradas: 12 fuentes bitmap, incluyen Á É Í Ó Ú Ñ ¿ ¡ → `tools/parse_fonts.py`.
- **Textos** (`/t`): solo inglés en este .jar (hay que traducir a español a mano).
- Música de PC: `mainmusic.mo3` renderizado por posición de orden (como `Music.cpp` del PC):

  | orden | archivo | uso |
  |---|---|---|
  | 0x00 | day_grasswalk.mp3 | día |
  | 0x30 | night_moongrains.mp3 | noche |
  | 0x5E | pool_waterygraves.mp3 | piscina |
  | 0x7A | choose_seeds.mp3 | elegir semillas |
  | 0x7D | fog_rigormormist.mp3 | niebla |
  | 0x98 | title_crazydave.mp3 | título / menú |
  | 0x9E | boss_brainiacmaniac.mp3 | jefe final |
  | 0xA6 | minigame_loonboon.mp3 | minijuegos |
  | 0xB1 | puzzle_cerebrawl.mp3 | puzles |
  | 0xB8 | roof_grazetheroof.mp3 | tejado |
  | 0xD4 | conveyer.mp3 | niveles de cinta |
  | 0xDD | zen_garden.mp3 | jardín zen |
  | — | credits_zombiesonyourlawn.mp3 | créditos |

  MP3 44.1 kHz 96 kbps → **decodificado por el Media Engine (sceMp3)**, streaming desde la MS, ~20 KB de RAM.
- 167 efectos de PC → **IMA-ADPCM 4 bits, 22050 Hz mono** (`data/sfx.pak`, 2,8 MB en total; se cargan
  solo los que usa cada nivel). Mezclador propio de 8 voces en un hilo.
- Texturas: `data/gfx.pak` — T8+CLUT (1 byte/píxel) para 560 de 605 imágenes, 5650 para fondos,
  8888 para el resto; todas *swizzled*. Fondos de 610 px partidos en tiles ≤512.
- Motor C sin SDL: `gfx.c` (sceGu), `audio.c`, `input.c`, `game.c`.
- Jugable: título (logo en español) → jardín de día, banco de 6 semillas con costes/recargas del PC,
  sol del cielo con los tiempos de `Board::UpdateSunSpawning`, girasoles que producen sol
  (`Plant::UpdateProductionPlant`), pala, pausa.
- **Animaciones "re" descifradas y funcionando** (adelanto del hito 2): las plantas ya se ven animadas.

### ✅ Hito 2 — animaciones + zombis + niveles 1-1…1-9 (sesiones 1 y 2)
**Hecho:**
- Formato del archivo `re` (263 KB, 52 animaciones; offsets en `ay.d[]`) — `tools/parse_reanim.py`:
  ```
  u8 n_pistas; i32 fps
  por pista: i32 n_frames; por frame:
     i8 g (-1 oculto / 0 visible)
     6 campos en orden x, y, sx, sy, kx, ky: u8 flag; si flag != 0 -> i32 valor (si no, repite el anterior)
     imagen: i8 flag; si != -1 -> i32 id de imagen (si no, repite)
  x,y en px 20.12 · sx,sy 4096 = 1.0 (0 = 1.0) · kx,ky en grados 20.12 (igual que el Reanim del PC)
  ```
  Matriz: `a=cos(-kx)·sx, b=-sin(-kx)·sx, c=sin(-ky)·sy, d=cos(-ky)·sy`; píxel (u,v) → (x+a·u+c·v, y+b·u+d·v).
- Las pistas **sin imagen** son pistas de control: su rango visible define cada animación
  (reposo, disparo, explosión…). Reposo = la pista de control visible en el frame 0.
- `data/anim.pak` + `src/reanim.c`: reproduce con **interpolación entre frames** (como el PC; el J2ME no lo hacía)
  y rotación/escala por GU.
- Mapa de animaciones (índice → personaje) en `src/reanim.h`:
  0 zombi base (con TODOS los accesorios: cono, cubo, puerta, bandera… hay que ocultar pistas por tipo),
  1 saltador con pértiga, 2 bailarín, 3 bailarín de apoyo, 4 futbolista, 6 caja sorpresa, 7 Gargantúa,
  8 diablillo, 9 escalera, 10 catapulta, 12 periódico, 13 globo, 14 zombi carbonizado, 16 Zombistein/jefe,
  22 girasol, 23 carnívora, 24 patapum, 25 calabaza, 26 pinchohierba, 27 lanzacoles, 28 nuez, 29 nuez alta,
  30 petacereza, 31 lanzamaíz, 32 lanzamelones, 33 tripitidor, 34 tronco ardiente, 35 lanzaguisantes,
  36 frutal estrella, 37 jalapeño, 38 cactus, 39–46 setas, 47 nenúfar, 48 Dave el Loco, 49 maceta.
  (5, 11, 15, 17–21, 50, 51 sin identificar del todo: efectos / otros zombis).

**Hecho en la sesión 2:**
- Controles del PvZBV (ver arriba). Sobre del lanzaguisantes corregido: la animación 35 tiene 3 cabezas:
  pista 4 = lanzaguisantes (img 453), 5 = hielaguisantes (283), 6 = repetidora (345).
- Textos oficiales en español sacados de `t-spa` del PvZBV → `ref/textos.json` (281 cadenas, con inglés al lado).
- Zombis con la animación 0: rangos 0-7 quieto, 8-19 andar, 20-29 comer, 30-39 morir (40-55 otros).
  Pistas: 11-13 bandera, 16-17 flotador, 19 cabeza, 20 brazo, 21-23 cono, 24-26 cubo, 27-29 puerta.
  Tipos: normal, abanderado, caracono, caracubo (vida/casco del PC: 270 / 370 / 1100).
- Combate: guisante 20 de daño (PC), hielaguisantes ralentiza 10 s, comer 100/s, nuez 4000 con 3 estados,
  petacereza 1800 en 3x3, patatapum se arma a los 15 s, cortacésped por fila, zombi carbonizado.
- Oleadas como `Board::SpawnZombieWave` (puntos = ola·4/5+1, ×2,5 en bandera; 2500+0..599 cs entre olas;
  se adelanta al bajar la vida de la ola; aviso de gran horda 725 cs). Mensajes "PREPARADOS / LISTOS / ¡A PLANTAR!",
  "SE APROXIMA UNA GRAN HORDA DE ZOMBIS", "ÚLTIMA OLEADA", "¡TE HAN COMIDO LOS SESOS!".
- Niveles 1-1 (1 fila de césped), 1-2/1-3 (3 filas), 1-4…1-9 (5 filas). Recompensas del PC:
  girasol, petacereza, nuez, pala, patatapum, hielaguisantes. Progreso guardado en `data/save.dat`.
- Prueba automática: `make AUTOTEST=2 EXTRA="-DAT_LEVEL=0"` juega el 1-1 solo (se gana en ~110 s).

**Pendiente / aproximado:**
- 1-5 en el PC es "bolos con nueces" y 1-10 la cinta: aquí son niveles normales por ahora.
- Faltan zombis saltador (anim 1), periódico (12), portero (puerta, pistas 27-29) y el resto de plantas.
- La cabeza no sale volando al morir (solo se oculta); el PC también suelta el brazo con animación.
- Colocación automática del cursor al elegir semilla: misma idea que el PvZBV pero con heurística propia.


### ✅ Sesión 3 — correcciones visuales (pedidas tras probar el hito 2)
- **Emulador J2ME sin pantalla** para comparar con el original: FreeJ2ME + `tools/fj_headless/Headless.java`
  (ejecuta el .jar, pulsa teclas por guion y guarda capturas). Con PvZBV a 320x240 se jugó el tutorial 1-1/1-2.
- Animaciones: el J2ME **no interpola** entre frames; ahora tampoco (`reanim_interp = 0`). Era lo que hacía raros
  el brazo del zombi y el lanzaguisantes.
- Máscaras de pistas confirmadas en el código (`bc.c()`): lanzaguisantes oculta pistas 5 y 6, repetidora 4 y 6,
  hielaguisantes 4 y 5 (ids 450-452); girasol oculta 5, 6 y 7 (caras brillantes).
- Girasol: brilla solo cuando va a dar sol (caras 171 → 168 → 166) y se apaga después.
- Lanzaguisantes: reproduce su animación de disparo (pista de control 0) y el guisante nace en la boca
  (posición real de la pista de la cabeza en ese frame).
- Soles con "recuadro": la CLUT tenía en el índice 0 un color opaco y el filtro bilineal lo leía en los bordes.
  `make_pak.py` ahora reserva el índice 0 como transparente.
- Sonido de comer: un mordisco audible por ciclo (chomp / chomp2 / chompsoft).
- Cámara con zoom 1,25: el césped llena la pantalla como en el PC; la calle queda tapada por un seto
  de matorrales (img 105) a la derecha, estilo Xbox 360. Los zombis salen de detrás del seto.
- Cursor de esquinas naranjas como el J2ME; número de soles recentrado.


### ✅ Sesión 4 — aventura completa del J2ME (versión de prueba)
- **50 niveles** como el J2ME (`cs.a` / `cj.a`): 1-x día (1-1 una fila, 1-2/1-3 tres filas), 2-x noche con tumbas,
  3-x piscina (6 filas, 2 de agua), 4-x piscina de noche, 5-x tejado. Olas por nivel, coste/peso/ola mínima
  y **zombis permitidos por nivel sacados del código J2ME** (`src/defs.h`).
- **31 plantas** (todas las del J2ME) con reglas del PC: disparo, bombeo (col/maíz con mantequilla/melón con
  salpicadura), frutaestrella 5 direcciones, humoseta atraviesa puertas, setas que duermen de día,
  nenúfar/maceta como base, carroñívora, apisonaflor, alga, pinchohierba, tumbatumbas, hipnoseta, seta
  hielo/destructora (cráter), jalapeño, plantorcha (guisante de fuego), nuez cáscara-rabias (frena saltos).
- **21 zombis**: normal, abanderado, cono, cubo, portero (puerta), deportista, saltador, bailón + extras,
  cajita (explota 3x3), lector (se enfada), Zombistein (aplasta, lanza Zombidito), escalador, zombipulta,
  saltarín, picado (excava y sale por detrás), globo (solo cactus/estrella), playero/buzo en agua, Dr. Zombi.
- 1-5 **bolos con nueces**, x-10 **cinta transportadora**, 5-10 **jefe** (simplificado: invoca zombis y pisa).
- Pantalla de **carga como el J2ME** (EA, PopCap, césped que se desenrolla + podadora). Menú sin zoom.
  Menú: Aventura / **Elegir nivel (los 50, para probar)** / Salir. Pantalla **¡ELIGE TUS PLANTAS!**,
  **diálogos de Dave** (textos oficiales), sobre de recompensa que se recoge con X, pausa con menú.
- Podadoras correctas (img 248); limpiapiscinas en filas de agua. Guisante sale más abajo (boca).
- Pruebas: `./at.sh <nivel> <segundos> captura.png` juega solo cualquier nivel en PPSSPP sin pantalla;
  `./at.sh 5 5 x.png -DAT_ZOO` muestra todos los zombis.

**Aproximado / pendiente:** rangos de animación de algunos zombis (comer/morir) puestos a ojo;
recompensas de los niveles x-4/x-9 (notas) sin pantalla de nota; jefe simplificado (sin bolas de fuego/hielo);
espacios de semilla (6→8) aproximados; buzo y delfín sin animación propia (el J2ME no los usa en niveles).


### ✅ Sesión 5 — lista de errores (errores.txt) corregida
1. Alga enredadora: ahora agarra al zombi (animación 5-16), lo hunde y desaparecen los dos.
2/7. Proyectiles: guisante = img 544 (como antes), espora de seta marina = 444, seta desesporada = 324
   y sale más abajo (las setas son bajitas).
3. Hipnoseta: el zombi hipnotizado se da la vuelta (dibujo espejado, `reanim_draw_flip`) y suena a mordisco.
4. Piscina con podadoras normales; el tejado usa los limpiatejados (img 413).
5. Elegir plantas: O sin nada elegido (o SELECT) vuelve al menú.
6. Tumbatumbas y humoseta: su sonido se corta al acabar la animación (`sfx_stop`).
8. Seta miedosa (y demás setas): el proyectil sale al final del gesto.
9. Apisonaflor: mira ("hmm"), salta en arco hasta el zombi, cae y se queda aplastada un momento.
10/15. Proyectiles sacados de las propias animaciones: espina del cactus 426, col 544, maíz 30, melón 473.
11. Pausa: corta todos los efectos que estaban sonando (sirena, horda...).
12. Tejado: empieza con macetas en las 3 primeras columnas, como el original.
13. Jefe: máquina de estados con sus animaciones (entrada, reposo, invocar con la mano, pisotón,
    bajar cabeza + bola de fuego/hielo, cabeza abajo = recibe doble daño, subir cabeza, muerte).
    La seta hielo apaga la bola de fuego; el jalapeño derrite la de hielo en su fila.
14. Sin zoom: vista 1:1 como el juego (se ve el jardín entero; seto tapando la calle, menos en el tejado).
16. Plantas sobre maceta/nenúfar: se apoyan según la altura real de la base.
17. Pausa con REANUDAR / REINICIAR NIVEL / VOLVER AL MENÚ.
18. El sobre de recompensa se recoge con X en cualquier parte (o solo a los 5 s).

### Hitos siguientes
- H3: Aventura mundo 1 completo + pantalla de elegir semillas + Dave + almanaque + guardado.
- H4: Noche (setas, tumbas), piscina, niebla (no está en el J2ME: dibujar niebla procedural), tejado, jefe.
- H5: Lo que no trae el J2ME: minijuegos, puzles, supervivencia, tienda de Dave, jardín zen
  (plantas/zombis sin gráficos J2ME → sprites hechos/reducidos aparte).
- H6: Traducción completa al español, optimización final (perfilado en PSP-1000 real).

## Notas técnicas / pendientes
- Recarga inicial de semillas lentas (3500/2000 cs) — aproximada, verificar con el código del PC.
- Música: se renderizan todos los canales del módulo. El PC sube/baja canales de batería ("burst")
  según la cantidad de zombis; pendiente generar versión "calmada" y "con batería" si se nota.
- El bucle MP3 tiene un microcorte (relleno del codificador); se puede quitar recortando ~1105 muestras.
- Memoria (SELECT en el juego): hito 1 usa ~1 MB de texturas en el jardín.

## Estructura
```
src/        código C del port
tools/      herramientas Python/C para regenerar los datos desde el .jar y el PvZ de PC
data/       datos generados (gfx.pak, sfx.pak, music/)
ref/        referencia: java decompilado, jar extraído, PNGs + meta.json + fonts.json
dist/       carpeta lista para copiar a la PSP / PPSSPP
```

### ✅ Sesión 6 — integración en el repositorio PVZ-Portable
- Este motor pasa a ser el juego principal del repositorio; el port traducido del bytecode
  J2ME queda en `j2me_traducido/`.
- **Vista del jardín como el J2ME**: cámara en x=0 (casa a la izquierda, césped desde x=171,
  acera a la derecha). Ya no se dibuja el seto: los zombis entran por el borde derecho.
- AUTOTEST: `-DAT_SHOT_EVERY=n` guarda capturas cada n fotogramas; `tools/ppsspp_test.sh`
  sustituye a `at.sh`. Probado en PPSSPP 1.18.1: niveles 1-1 y 2-4 → 2-5 completos a 60 fps.
- Los datos (`data/`, `ref/`) no se suben a git (contenido de EA/PopCap).
- **Música en la consola real**: el hilo de música abría `data/music/...` con ruta relativa,
  pero en PSP los hilos nuevos no tienen directorio de trabajo (PPSSPP lo avisa: "no current
  working directory"), así que la música no sonaba. `music_play` ahora pasa la ruta completa.

### ✅ Sesión 7 — lo que faltaba del J2ME (comparando con su código y con el port traducido)
Medido en el J2ME con el registro de dibujo del port traducido (`PVZ_DRAWLOG`) y su código:
- **Menú de la lápida** (fondo 290, logo, pieza 527 con la caja del nivel en la fuente 253,
  losas 270/271 con la fuente 566, icono de salir 525) y la **mano de zombi** (443) al pulsar
  Aventura. Pantallas nuevas: **Opciones** (sonido, música, borrar datos), **Almanaque**
  (plantas y zombis con su animación y los textos oficiales 130-238) y **Acerca de**.
  `gfx_set_vscale(272/320)` dibuja estas pantallas con las coordenadas 480x320 del J2ME.
- **Intro del nivel**: la cámara va a la calle con la curva exacta del J2ME (0→130 en 30
  frames de 1/6 s, vuelta en 25) y se ven los zombis del nivel de pie en la calle (posiciones
  de sus sombras 550). Si hay más plantas que huecos, sale **"¡ELIGE TUS PLANTAS!"** como en el
  J2ME (clase `c`): columna de la izquierda (430/481) con las elegidas, panel 386/214/157/203/375
  con la cuadrícula de 3 columnas en el orden del J2ME y el título en marquesina.
- **Cortacésped**: en x=140, 6 px por encima de la casilla, y entran rodando desde x=115 con
  los pasos +5,+4,+4,+4,+3,+3,+2, primero el de abajo y cada fila 3 frames después. La barra de
  semillas y el contador de soles bajan a la vez.
- **Patatapum**: estados del J2ME (`cp` tipo 8): al explotar queda el puré (frame 8, imagen 564)
  2 s, sale "SPUDOW!!" (104) y 10-15 trozos de patata (hojas 85/142), como `bk.d`.
- **Partículas** (hojas de sprites de la clase `y`): salpicadura de guisante/hielo/fuego,
  trozos de cono (70) y cubo (234), humo de la humoseta (232), llamas del jalapeño (477, por
  frames), POWIE/humo de la petacereza, DOOM y nubes de la petaseta, copos de la seta congelada.
- **Proyectiles**: el guisante es la imagen 73 (la 544 es la col que sostiene la coltapulta); la
  col en vuelo es la 597. Las catapultas lanzan en el frame 6, cuando desaparece lo que sostienen.
- Nombres oficiales del J2ME para las plantas (COMEPIEDRAS, SETA MIEDICA, PETASETA, ZAMPALGA...).
- Arreglado `gfx_clip` (pasaba x2,y2 a `sceGuScissor`, que espera ancho y alto).

### ✅ Sesión 8 — lista de errores del usuario, comparando con el J2ME
- **Zombis con la animación equivocada**: los archivos `re` 5, 11, 12 y 15 son el lector, el saltarín,
  el minero y el buzo (estaban cruzados: el lector usaba la del minero y por eso "explotaba"). Rangos
  nuevos: lector rompe el periódico (26-29) y corre (30-33); pértiga salta con 13-23 (la animación lleva
  ~58 px a la izquierda) y luego anda sin pértiga (24-34), también al terminar de comer; saltarín con el
  palo (39-41); minero bajo tierra (32-36) y sale (37-41); globo vuela (4-7); buzo bajo el agua (20-21).
- **Piscina**: los zombis andan con pies hasta entrar al agua (chapoteo) y nadan con la animación 40-49
  de la animación 0, que ya oculta las piernas; se hunden con 50-55. El cortacésped de una fila de agua
  cae a la piscina (el J2ME no tiene limpiapiscinas: solo carga 248 o 413).
- **Piezas que caen**: brazo (350), cabeza (78), cono (33), cubo (316) y puerta (515) caen girando y
  rebotan, tomados de la posición de su pista en el frame actual.
- **Hipnosis**: los zombis muerden al hipnotizado que tienen delante; los mordiscos entre zombis ya no
  suenan como golpes en el cono/cubo. Mordiscos sincronizados con la animación de comer.
- **Plantas**: el guisante sale de la boca (cabeza a escala 1.12) en el frame 9, el del J2ME; frames de
  disparo medidos para cada planta; la planta carnívora suena al cerrar la boca; la humoseta suelta 20
  bolitas moradas (hoja 197) y una nube (89) en cada zombi, como `bk.i` del J2ME; la patatapum no
  chamusca; espina del cactus = imagen 109 (la 426 es su boca); grano de maíz = 188 (la 30 es la cesta);
  col = 544; mantequilla (281) dibujada en la cabeza; plantas asentadas en la tierra de la maceta;
  4 columnas de macetas en el tejado (como el J2ME).
- **Interfaz del J2ME** (`src/ui.c`, a escala 1:1 para que los mosaicos no dejen rayas): lápida morada
  con calavera para la pausa (también al elegir plantas: START) y los avisos; almanaque con su fondo
  naranja y marco, índice con los tres botones (plantas, zombis, ayuda), retratos 45x45 de cada zombi,
  fichas con panel y desplazamiento que se detiene al final del texto; ficha de "¡ENCONTRASTE UNA
  SEMILLA NUEVA!" y destellos en el sobre ganado; diálogo de Dave con bocadillo sobre el fondo del
  nivel; pantalla de carga de la clase `by` (franja de césped fija y el cortacésped recorriéndola).

### ✅ Sesión 9 — segunda lista de errores y el PvZBV (versión de teclado)
- El `PvZBV.jar` (4.1.61, 320x240) se tradujo con `jvm2cpp.py` para verlo funcionar (hacía falta
  `ByteArrayOutputStream`/`DataOutputStream` y tolerar `ch.b()V`, que falta en ese jar modificado).
  Sus imágenes se extraen con `extract_imgs.py` (37 paquetes). `tools/make_gfx_dir.py` añade a
  `gfx.pak`: esquinas del cursor (612/613), marco del sobre (614), flechas, candado, sobres 38x28
  (620+PL_*) y los logos francés/alemán/italiano (651-653).
- **Cursor del PvZBV**: 4 esquinas de 13x12 que se deslizan a la casilla; rojas donde no se puede
  plantar; la planta elegida se ve en la casilla. **Caja de semillas** en columna a la izquierda con el
  marco 41x31 y los sobres pequeños; izquierda en la primera columna entra en la caja.
- **Idiomas**: `tools/gen_texts.py` genera `texts.h` con las 6 tablas (mismos índices); se elige por el
  idioma de la consola (`sceUtilityGetSystemParamInt`) o en Opciones; textos propios del port en `XS()`.
- **Elegir plantas**: 4 columnas; el cursor entra en la columna de las elegidas para quitarlas.
- **Elegir nivel**: marco del almanaque, miniatura del fondo de cada zona y niveles en marcos de sobre.
- **Pantalla de carga**: las piezas l3/l4 estaban cambiadas (el extremo de 24 px se usaba de tramo
  central); ahora se dibuja a 1:1 y la franja es continua.
- **Tejado como el J2ME**: casillas de 29 px desde x=168 y pendiente de 8.3 px por columna en las 5
  primeras (medido con el registro de dibujo); zombis, plantas, cursor y limpiatejados la siguen.
- Plantas en maceta asentadas en la tierra (fila 13 de la imagen de la maceta).
- **Pértiga**: cada animación de zombi tiene los pies en otro sitio de su caja (la pértiga 20 px a la
  derecha); ahora se alinean los pies con la posición lógica. No salta la apisonaflor y su "hup" se
  corta si muere en el salto.
- Guisantes: salen de dentro de la boca, el tramo recorrido cuenta desde la planta (un zombi pegado
  recibe el primero) y la repetidora suelta el segundo desde la boca 0,22 s después.
- Zampalga: hunde al zombi sin moverlo; carnívora: alcanza al que come la planta de delante; el brazo
  caído rebota y se desliza un poco hacia atrás antes de desaparecer.

### ✅ Sesión 10 — tercera lista
- Tejado: el J2ME juega el tejado con la cámara en x=130 (las macetas salen en pantalla desde x=38 y
  los limpiatejados detrás de la caja de semillas); ahora igual.
- Pértiga: empieza el salto 40 px antes de la planta para caer justo detrás de ella (saltaba dos).
- Planta carnívora: alcanza al zombi que se come la nuez de delante (2 casillas).
- Cursor: parpadea y se queda en su casilla al elegir un sobre.
- Tejado: filas alineadas con las tejas del fondo (y0=44, alto 37.6); antes iban media teja por encima y
  las macetas parecían flotar. Los zombis siguen la fila y la pendiente.

### ✅ Sesión 11 — versión Tencent (640x360) y cuarta lista
- El jar Tencent usa el mismo formato que el 4.6.0 pero con gráficos a 640x360 (x1.33). `tools/add_tencent.py`
  añade sus 564 imágenes (ids 700+, reescaladas x0.75 al tamaño del J2ME), sus `l*.png` (1280+n), sobres de las
  plantas nuevas (1360+ y 1380+ a 38x28) y sus 56 animaciones (archivos 52.. de `anim.pak`, posiciones x0.75).
  `extract_imgs.py` y `parse_reanim.py` aceptan el jar/tabla del Tencent (`parse_reanim.py re out.json tc`).
- Plantas nuevas: guisantrallador, melonpulta invernal, mazorcañón (X sobre él y X en el blanco), espadaña
  (pinchos teledirigidos, solo en agua), trébol (globos y niebla), planterna, ajo (cambia de fila) y calabaza
  (tercera capa de la casilla). Recompensas en 4-1, 4-3, 4-4, 4-7, 5-3, 5-4, 5-5 y 5-6. Niebla en la zona 4.
- Zombi bailarín y delfín con las animaciones del Tencent.
- Minijuegos (menú "Minijuegos"): bolos con nueces (y nuez explosiva), combate de portales, última
  resistencia (5000 soles, SELECT lanza cada asalto), zombis invisibles y zombis veloces. La tragaperras de la
  Gran Muralla no se incluye.
- errores4: brazo de la pértiga, panorámica del tejado como el J2ME (cámara 0-130-0), escalera que se apoya y
  se trepa, pala de verdad en la casilla, disparo del lanzaguisantes como el J2ME (clase `cp`: animación en
  bucle y guisante 7 ticks después), rodillo de césped en 1-1/1-2/1-4, L/R al siguiente sobre listo,
  partículas al plantar, bailarín solo en filas interiores, hongo nuclear de la petaseta, caja sorpresa y
  lector tenían las animaciones cruzadas (5 = caja, 6 = lector), casco del deportista, todos se chamuscan,
  nunca dos abanderados a la vez.

### ✅ Sesión 12 — errores5
- Lanzaguisantes (y hielaguisantes/repetidora): en la animación del J2ME los frames 0-5 son el disparo (la
  cabeza se encoge y se estira) y 6-11 el reposo; estaban al revés. El guisante sale al final del estiramiento.
- Marseta: 8-11 es su reposo con boca y 1-4 dormida. Bailarín (Tencent): 14-22 anda, 23-33 brazos arriba al
  invocar, uno por oleada y nunca dos a la vez. Deportista: el casco entero es la pista 9 (452), no la 11.
- Brazo caído en todos los zombis: tabla `arm_track()` con la pista del brazo de delante de cada animación;
  la pieza que cae es la imagen de esa pista en ese frame.
- Plantas del Tencent: caja calculada con su frame de reposo (`reanim_fix_bbox`): la planterna y el mazorcañón
  la tomaban del frame de la semilla. El mazorcañón ocupa dos casillas (mitad derecha `part`).
- Texturas del Tencent sin reducir a 256 colores (perdían mucha calidad). Sobres nuevos con el coste montado con
  los dígitos de píxel de los sobres del J2ME/PvZBV. Niebla con una mancha suave generada (1390), sin cuadros.
- Tejado: las partículas de tierra de las macetas iniciales se quedaban congeladas durante la intro.
- Zombis de cada nivel según la lista del PC (`orden_zombis_pvz1.txt`): playero, buzo, delfín, cajita, globo,
  excavador, saltarín, escalerilla, catapulta, zombistein y diablillo en sus niveles (sin zomboni, bobsled ni bungee).

### ✅ Sesión 13 — errores6 y seta melancólica
- Seta melancólica (PL_GLOOM): piezas de la hoja de la DS (`ref/melancoseta_ds.png`, `tools/gloom_parts.py`,
  ids 1460-1473) dibujadas por código (`draw_gloom`): cuerpo, bocas de tubo, cabeza que respira y parpadea,
  duerme de día e infla los mofletes; 4 bocanadas de humo en 3x3. Recompensa en 4-4.
- Púa del cactus y de la espadaña: la del PC (`ref/pua_cactus.png`, id 1474).
- Mazorcañón: la mazorca sube recta y cae recta en el blanco (3,2 s); animaciones del Tencent de 12 fps a la mitad;
  se dibuja desde su mitad derecha para quedar encima de las dos macetas.
- Orden de recompensas y de "elige tus plantas"/almanaque como el PC (`pc_order`). Zombis de muestra con los de la
  piscina y un solo abanderado. Playero pierde también el brazo de nadar (pista 18). Casco del deportista con
  sonido de plástico. Última resistencia: SELECT empieza de verdad el siguiente asalto. El saltador salta la
  primera nuez de los bolos; la nuez explosiva es la nuez del J2ME tenida de rojo. Niebla más espesa.

### ✅ Sesión 14 — errores7
- No se tocan las imágenes originales del juego base (los sobres del J2ME quedan como estaban).
- Números de 2 cifras de los sobres nuevos centrados en el globo. Púa del cactus/espadaña reducida al 60 %.
- Seta melancólica sin las bocas laterales y más baja. Niebla más espesa (4 manchas por casilla).
- Apisonaflor: alcanza al zombi que se come la planta de delante (1,6 casillas) y también hacia atrás.
- Delfín: entra en la piscina montado (24-34), va montado (35) y rápido, salta la primera planta (36-41) y sigue
  nadando sin delfín (42-51); los desplazamientos de la animación se pasan a la posición (sin saltos atrás).
- Última resistencia: entre asaltos los sobres están siempre cargados. Diana del mazorcañón: imagen 398 del Tencent.
  Pinchos de la espadaña a velocidad de guisante. Dr. Zombi en x=280 (la cámara del tejado ahora está en 0).
- Melonpultas 4 px más atrás; planterna centrada y más alta. Buzo nadando en su fila.
- Bolos como el PC: cada golpe quita la puerta, el casco o la vida; la nuez rebota en diagonal y no vuelve a golpear
  hasta cambiar de fila; cinta cada 3,3 s.

### Sesión 15 (errores8)
- Delfín: montado va 0.2 filas más bajo y nadando sin delfín 0.9 (antes 0.55 en ambos casos, demasiado bajo al ir montado).
- Planterna 4 px más adelante.
- Seta melancólica armada con las piezas de la DS como la del PC (`gloom_parts.assemble`): cuerpo verde y 8 tubos en la imagen 1475, con la cabeza encima.
- Minijuegos: el título va en la losa; la imagen va detrás del marco l61 ajustada a su ventana, y el nombre en la placa gris (texto a escala 0.62, nuevo `text_scale`).
- add_tencent: un `fill()` metido antes de otro `put()` duplicaba ids en el meta y desplazaba las imágenes del pak.

### Sesión 16
- El zip de la sesión 15 no se había regenerado (build_dist no hacía el zip): ahora `build_dist.sh` crea `PvZ_PSP_J2ME.zip`.
- Niebla: la mancha 1390 nunca tiene alfa 0 (el bilineal de la PSP mezclaba con el negro transparente y salían líneas oscuras).
- Trofeo de minijuego en la esquina de arriba a la izquierda del marco.
- Almanaque: fondo de pergamino de la DS con el girasol (plantas, 1476) y el zombi (zombis, 1477), sacados de `ref/ds_menus.webp`. La lista de plantas muestra 5 filas y se desplaza con el cursor (49 plantas ya no caben).
- Planta nueva: pantalla como la de la DS (sobre, nombre, primera frase de la descripción sobre el pergamino del girasol); la ficha completa solo en el almanaque.

### Sesión 17 (errores9)
- Guisantralla (PL_SPLITPEA, PK_SPLIT): armada con `tools/splitpea_parts.py` (1478 cabeza doble, 1479 tallo y hojas); 1 guisante delante y 2 detrás; sobres 1400/1440; premio del 4-4 (la melancólica pasa al 4-9). Los guisantes rectos ya chocan en los dos sentidos.
- Ajo: cara de asco del PC (1481, `ref/zombi_partes.png`) en la cabeza 19 del zombi normal mientras muerde y un rato después, y SFX_YUCK.
- Playero: sin chapoteo extra al caerle el brazo en el agua y sin sombra en la piscina (ningún nadador).
- Pergaminos de la DS: trazo separado del papel y pintado sobre papel liso (1476/1477 almanaque más pequeños, 1480 planta nueva). Nombre de la planta nueva en su barra; los nombres largos se encogen para caber.
- Paneles: los laterales girados se dibujan sin bilineal (GFX_NEAREST) para evitar líneas oscuras; niebla con borde suave sobre la valla.

### Sesión 18 (errores10)
- Ajo: mordisco con sonido, "puaj" y cambio de fila en diagonal hacia delante; la cara de asco se dibuja con la misma posición y giro que la cabeza (`reanim_swap_track`).
- Guisantralla rearmada como la referencia del PC (cabezas juntas y más altas, tallo largo, hojas grandes).
- Grano de café (PL_COFFEE): solo sobre una seta dormida; el grano (1482, tira de 22 frames de la hoja del PC con la hoja arriba) se queda y se deshace, y la seta despierta (instantáneas: explotan). Premio del 5-3 (la guisantralladora pasa al 5-5); sobres 1402/1442.
- Cinta de los niveles x-10 y bolos: marco y banda del PC (1483/1484) que corre, 10 sobres como el PC.
- 2-5 "Golpea al zombi": solo el mazo (1485); X da un mazazo en la casilla (normal 1, cono 2, cubo 3); los zombis salen de las tumbas más rápido y salen tumbas nuevas.
