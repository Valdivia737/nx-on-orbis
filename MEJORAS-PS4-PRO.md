# NX on Orbis — mejoras para PS4 Pro (rama `mejoras-ps4-pro`)

Fork de [alechurri/nx-on-orbis](https://github.com/alechurri/nx-on-orbis) (port experimental del emulador
de Switch **Eden** a PS4 Pro) mantenido por **Valdivia737**. Esta rama recoge el trabajo de una semana
de pruebas en una **PS4 Pro con firmware 10.50 y GoldHEN**: de un Eden que con Super Mario Bros. Wonder
bajaba a 1 FPS y se congelaba a los 2–3 minutos, a uno que **juega Wonder 30 minutos seguidos a 26–30 FPS**
y, con **fastmem** (jitcache16–18), **Super Mario 3D World a 30 FPS fijos** en el mundo 1-1.
jitcache20–21 añaden **ajustes por juego desde el menú** (Triángulo, como en Crema) y arreglan el **cierre
al arrancar**. Zelda: Breath of the Wild arranca, pero se queda sin memoria (ver Resultados).

> Escrito por Claude (asistente de IA de Anthropic) a partir de las sesiones de trabajo con Valdivia737.
> Todo lo que aparece como resultado está **medido** en la consola con los registros que hay en
> [`registros/`](registros/). Lo que no está comprobado se marca como **HIPÓTESIS**.

**Sigue siendo experimental.** No incluye juegos, claves ni firmware de Switch: cada uno pone los suyos.
Licencia GPL-3.0, igual que el proyecto original y Eden.

---

## Resultados (PS4 Pro, fw 10.50)

| Juego | Formato | Estado | FPS jugando | Versión | De dónde sale el dato |
|---|---|---|---|---|---|
| Super Mario Bros. Wonder | XCI | **Estable** sin fastmem: 32 min seguidos sin fallos y guarda partida. **Con fastmem (jitcache14–18) se queda sin memoria a los ~1,5 min** | Niveles **26–30**, mapa del mundo **44–56** | jitcache10–12 | Medido en vivo con jitcache10 (ese registro no se guardó). Con jitcache8: [`jitcache8_20261031_mundo_ok`](registros/eden-propio/jitcache8_20261031_mundo_ok/). El fallo con fastmem: [`jitcache18_20261101_wonder_sinmemoria`](registros/eden-propio/jitcache18_20261101_wonder_sinmemoria/) |
| Super Mario 3D World + Bowser's Fury | XCI | **Estable**: 7 min sin fallos con fastmem; 15,8 min sin él | Mundo 1-1 **30 fijos** con `fps=30` (baja a 26–28 en la zona más pesada); sin fastmem, niveles 23–30 (mediana 27) | **jitcache18** (antes jitcache11) | [`jitcache18_20261101_3dworld_fps30`](registros/eden-propio/jitcache18_20261101_3dworld_fps30/), [`jitcache11_20261101_3dworld`](registros/eden-propio/jitcache11_20261101_3dworld/) |
| Paper Mario: La Puerta Milenaria | NSP | **Jugable pero lento**, en español: 16,9 min sin fallos. Con fastmem no mejora (mediana 7,6). Con jitcache20 se congeló al empezar partida, a los ~3 min (pendiente) | **7–9** (mediana 8,9), menús hasta 26. Una cinemática bajó a 0,2–0,5 | jitcache11 | [`jitcache11_papermario_boot.log`](registros/eden-propio/jitcache11_papermario_boot.log), [`jitcache18_20261101_papermario_fastmem`](registros/eden-propio/jitcache18_20261101_papermario_fastmem/), [`jitcache20_20261101_papermario_congelado`](registros/eden-propio/jitcache20_20261101_papermario_congelado/) |
| Zelda: Breath of the Wild | NSP (v0) | **Arranca, pero se queda sin memoria**: con los ajustes de abajo se cierra a los **15 min** (memoria directa a 1 MiB). Con `ORBIS_ARENA_MIB=1152` se cierra antes, a los 6,5 min, por memoria de la GPU (`VK_ERROR_OUT_OF_DEVICE_MEMORY`). Eden4 1.0 se cierra a los ~3 min | **28–30** en la intro; **8–12** en el juego, cuando la GPU se llena de texturas | jitcache21 | [`jitcache21_20261101_zelda_15min`](registros/eden-propio/jitcache21_20261101_zelda_15min/), [`jitcache21_20261101_zelda_arena1152`](registros/eden-propio/jitcache21_20261101_zelda_arena1152/), Eden4: [`20261101_zelda`](registros/eden4-xatyy/20261101_zelda/) |
| Metroid Dread | NSP | **No carga**: al cargar pide una reserva de 1,8 GiB y falla | — | — | Pendiente |

### Registro de FPS por sesión (sacado automáticamente de `boot.log`)

| Sesión (`registros/eden-propio/`) | Versión | Juego | Minutos | FPS mediana | Peor 10 % | Máx. | Qué pasó |
|---|---|---|---|---|---|---|---|
| `jitcache1_20261031_sesion1` | jitcache1 | Wonder | 6,0 | 4,3 | 1,0 | 44,5 | Bajones a 1 FPS: la caché del JIT se vació 25/28/10 veces |
| `jitcache1_20261031_sesion2_garlic0` | jitcache1 | Wonder | 7,3 | 6,2 | 1,0 | 41,5 | Lo mismo: 31/34/13 vaciados |
| `jitcache1_20261031_sesion3_garlic0_cache128` | jitcache1 | Wonder | 5,9 | 30,0 | 23,5 | 38,1 | Sin bajones (caché de 128 MiB), pero se quedó **sin memoria** a los 4 min |
| `jitcache2_diag_boot.log` | jitcache2 | Wonder | 3,4 | 30,0 | 10,9 | 33,5 | Sin memoria (`bad_alloc`): así se descubrió el problema del heap |
| `jitcache4_20261031_cache64` | jitcache4 | Wonder | 4,5 | 18,9 | 10,8 | 54,1 | Ningún fallo: heap arreglado |
| `jitcache6_20261031_arena1792_vma` | jitcache6 | Wonder | 6,5 | 21,2 | 6,7 | 50,9 | Congelado a los 213 s con arena 1792 (configuración descartada) |
| `jitcache8_20261031_mundo_ok` | jitcache8 | Wonder | 12,0 | 22,5 | 8,9 | 57,4 | Ningún fallo: el mapa del mundo carga |
| `jitcache11_papermario_boot.log` | jitcache11 | Paper Mario | 16,9 | 8,9 | 7,0 | 26,5 | Ningún fallo; lo limita la CPU |
| `jitcache11_20261101_3dworld` | jitcache11 | 3D World | 15,8 | 27,0 | 23,2 | 60,0 | Ningún fallo; un solo vaciado del JIT |
| `jitcache12_20261101_wonder` | jitcache12 | Wonder | 7,4 | — | — | — | Colores correctos; un nivel nuevo a 11–19 FPS sin ningún núcleo al tope (pendiente de investigar); vaciados del JIT 6/5/3 con 64 MiB |
| `jitcache13_20261101_3dworld_unsafe` | jitcache13 | 3D World | 6,0 | — | — | — | `cpu=unsafe` + JIT 96 MiB: 1-1 a 20–28 FPS, igual que jitcache11 |
| `jitcache14_20261101_3dworld_fastmem` | jitcache14 | 3D World | 14,1 | 59,9 | 9,8 | 60,0 | Fastmem del autor tal cual: **1-1 a ~10 FPS** y la primera imagen a los 104 s (la mediana alta es de los menús) |
| `jitcache15_20261101_3dworld_fastmem` | jitcache15 | 3D World | 7,3 | 45,5 | 21,0 | 60,0 | 1-1 a ~21 FPS: la vista de fastmem no llegó a mapear ninguna página |
| `jitcache16_20261101_3dworld_fastmem` | jitcache16 | 3D World | 6,8 | 31,4 | 27,1 | 60,0 | **1-1 a 27–35 FPS (casi siempre 29–32)**: primera mejora real de fastmem |
| `jitcache17_20261101_wonder_colgado` | jitcache17 | Wonder | 4,6 | — | — | — | **Colgado** al arrancar: un núcleo dando vueltas en un candado que nunca se libera (error de jitcache17, arreglado en jitcache18) |
| `jitcache18_20261101_wonder_sinmemoria` | jitcache18 | Wonder | 3,7 | 20,6 | 1,3 | 29,0 | Ya no se cuelga, pero **se queda sin memoria a los 93 s**: ~240.000 fallos de fastmem y el heap crece 640 MiB de golpe |
| `jitcache18_20261101_3dworld_cierre_arranque` | jitcache18 | 3D World | 0,2 | — | — | — | Cierre intermitente al arrancar (ver Pendientes); al reabrir, bien |
| `jitcache18_20261101_3dworld_fps30` | jitcache18 | 3D World | 7,0 | 29,7 | 26,2 | 30,0 | **Con `fps=30`: 1-1 a 30,0 fijos casi todo el rato**, 26–28 en la zona más pesada |
| `jitcache18_20261101_papermario_fastmem` | jitcache18 | Paper Mario | 5,2 | 7,6 | 3,5 | 25,8 | Con fastmem no mejora: para Paper Mario, `fastmem=off` |
| `jitcache20_20261101_papermario_congelado` | jitcache20 | Paper Mario | 34,3 | 14,2 | 1,7 | 25,1 | Se congela al empezar partida (~3 min); el registro siguió escribiendo sin imagen nueva. HIPÓTESIS: el reparto de registros de Eden4 (jitcache19); falta probar con `regalloc=off` |
| `jitcache21_20261101_zelda_15min` | jitcache21 | Zelda BotW | 14,9 | 9,6 | 6,9 | 29,6 | 28–30 en la intro, 8–12 jugando; **se cierra a los 15 min sin memoria** (1 MiB libre) |
| `jitcache21_20261101_zelda_arena1152` | jitcache21 | Zelda BotW | 7,9 | 9,3 | 4,1 | 30,0 | Con `ORBIS_ARENA_MIB=1152` (para ahorrar memoria): 22–30 más tiempo, pero **se cierra a los 6,5 min sin memoria de GPU** |

- La mediana y el "peor 10 %" se calculan con todas las líneas `status:` de 1 FPS o más, así que incluyen cargas y menús.
- El "sin imagen durante 15 s" que sale en todas las sesiones hacia los 16 s es la carga inicial, y es normal.

**El límite hoy es la CPU, no la GPU.** En los tres juegos que funcionan, el núcleo que ejecuta el código
principal del juego va al 93–97 % y la GPU espera el 0 % del tiempo. Bajar la resolución no ayuda; lo que
ayuda es abaratar el código que genera el JIT: eso es lo que hace **fastmem** (jitcache14–18, ver abajo).
**Zelda: Breath of the Wild es otra historia: lo limita la memoria.** El juego usa casi 2 GiB propios y la
app tiene unos 4,5 GiB en total; los FPS caen a 8–12 cuando la GPU se llena de texturas.

---

## Qué estaba mal y cómo se arregló (en orden)

Cada paso se encontró midiendo con el registro de la consola (`boot.log`), no a ojo.

| # | Problema medido | Arreglo | Versión |
|---|---|---|---|
| 1 | Bajones periódicos a ~1 FPS: la caché de código del JIT (32 MiB por núcleo) se llenaba y se vaciaba entera, y el juego se recompilaba durante un minuto | Caché ajustable desde `settings.txt` (`NX_JIT_CACHE_MIB`), contador de vaciados en el registro (parche 0028) | jitcache1 |
| 2 | Al subir la caché, congelamientos por falta de memoria: el heap de musl crecía a saltos exponenciales (128, 256, 512 MiB) aunque se pidieran 96 bytes | `__expand_heap` propio con un paso máximo de 32 MiB (`frontend/libc_heap_fixes.cpp`) | jitcache4 |
| 3 | La tabla de páginas de la GPU reservaba 128 MiB de golpe | Tabla perezosa (parche 0029) | jitcache3 |
| 4 | Al entrar a un nivel, la ventana de memoria de la GPU se llenaba: las texturas ASTC se descomprimían a RGBA8 (4 bytes por píxel) | Opción `astc_recompress=bc3` (1 byte por píxel) y estadísticas de memoria de la GPU en el registro (parches 0030 y 0031) | jitcache5–7 |
| 5 | El mapa del mundo de Wonder no cargaba por el reparto de memoria | Búfer de vídeo y cola de descriptores ajustables (parche 0032) | jitcache8 |
| 6 | Robustez ante falta de memoria de la GPU | Liberar memoria y reintentar cuando falla una reserva, **adaptado de [Eden4](https://github.com/xatyy/Eden4) de xatyy** (parches 0033 y 0034) | jitcache9–10 |
| 7 | Los juegos salían en inglés | `language=` y `region=` en `settings.txt` | jitcache11 |
| 8 | **Rojo y azul cambiados en toda la imagen** (problema que el autor original dejó abierto) | La imagen final se dibujaba en orden BGRA sobre una pantalla RGBA; ahora se usa el orden de la pantalla, como ya se hacía en Android (parche 0035) | jitcache12 (**confirmado en la consola el 2026-11-01: Mario sale rojo**) |
| 9 | ¿Más FPS con un JIT menos exacto? | `cpu=unsafe` en settings.txt (NaN y coma flotante menos exactos). **Probado en 3D World 1-1: 25,1 FPS frente a 25,8 = sin mejora**: el núcleo no gasta el tiempo en la coma flotante sino en los accesos a memoria | jitcache13 |
| 10 | Cada acceso a memoria del juego recorre la tabla de páginas (8–10 instrucciones) | **fastmem** de la rama experimental del autor (parches 0036–0039, de alechurri): una "vista" de la memoria del juego en la que un acceso es una sola instrucción. Necesita `cpu=unsafe`. Tal cual, **1-1 a ~10 FPS**: todo lo que caía fuera de la vista (8 GiB, la PS4 no da más) iba por la vía más lenta (una llamada por byte) | jitcache14 |
| 11 | Lo de fuera de la vista, por la vía más lenta | Esas direcciones recorren la tabla de páginas en código aparte y solo si eso falla se llama a la vía lenta (parche 0040). 1-1 a ~21 FPS: la vista mapeó **0 páginas** | jitcache15 |
| 12 | El heap y las pilas del juego caían por encima de los 8 GiB (colocación al azar en 512 GiB), y la base del código en pasos de 4 KiB no cuadraba con las páginas de 16 KiB de la PS4 | En ORBIS, pila y heap justo después del código y base del código alineada a 16 KiB (parche 0041). **1-1 a 27–35 FPS (casi siempre 29–32)** frente a ~25 sin fastmem | jitcache16 |
| 13 | Páginas de 16 KiB con algún trozo de 4 KiB sin mapear (bordes de las pilas) quedaban fuera de la vista. El primer intento (jitcache17) **colgaba Wonder**: también enseñaba trozos que el juego tiene mapeados a otra memoria | Se mapean solo si los trozos que faltan no están mapeados en absoluto (parches 0042 y 0043); `fastmem_partial=off` vuelve a la regla de jitcache16 | jitcache17–18 |
| 14 | Entre 30 y 35 FPS la imagen alterna fotogramas de 16 y 33 ms (tirones) | `fps=30` en settings.txt: limitador de Eden que muestra un fotograma cada 33,3 ms. En una tele de 60 Hz solo 60, 30 y 20 se ven parejos | jitcache17–18 |
| 15 | Eden4 va más rápido en 3D World (35–57 FPS) | Su reparto de registros en dynarmic: apartar un valor a RBX/R12/R13 libres en vez de guardarlo en memoria, y R13 asignable sin fastmem, **adaptado de [Eden4](https://github.com/xatyy/Eden4)** (parche 0044); `regalloc=on|off`. Además, el registro dice al arrancar el modo Pro y el reloj de la CPU (2100 MHz, igual que Eden4: el reloj no explica la diferencia) | jitcache19 |
| 16 | `fastmem` y el resto de ajustes eran globales: Wonder y Paper Mario lo necesitan apagado, 3D World encendido | **Ajustes por juego** en el menú (Triángulo sobre el juego): fastmem, FPS, reparto de registros e idioma. Se guardan en `/data/edenps4/juegos/<juego>.txt` y se aplican después de `settings.txt`; las líneas que el menú no conoce (p. ej. `env=…`) se conservan. El juego sale marcado `[ajustes propios]` | jitcache20 |
| 17 | **Cierre al arrancar** (1 de cada 2–3 veces; Wonder 3 de 3 con jitcache20), con trozos de texto del registro en el heap | El registro de Eden escribía en el mismo archivo desde varios hilos a la vez, y el `stdio` de la PS4 no lo protege para los hilos del sistema: un `fwrite` pisaba el heap. Ahora escribe un hilo cada vez (parche 0045). Desde entonces, ningún cierre al arrancar en las aperturas probadas | jitcache21 |

Los cambios de Eden están en [`patches/eden/`](patches/eden/) (0028 a 0045) y los del frontend en
[`frontend/`](frontend/). Todo va dentro de `#ifdef __ORBIS__`: no afecta a otras plataformas.

---

## `settings.txt` recomendado

Va en `/data/edenps4/settings.txt` de la PS4:

```
env=ORBIS_VRAM_GARLIC=0
env=ORBIS_ARENA_MIB=1408
env=NX_JIT_CACHE_MIB=96
env=NX_JIT_CACHE_MIB_CORE3=8
env=NX_TEX_CACHE_PCT=40
env=NX_BUF_CACHE_PCT=20
astc_recompress=bc3
env=NX_STREAM_BUFFER_MIB=128
env=NX_DESC_PAYLOAD=0x20000
language=es-419
region=usa
cpu=unsafe
fastmem=on
fastmem_partial=on
fps=30
regalloc=on
```

- **Ajustes por juego (jitcache20+)**: en el menú, Triángulo sobre el juego. Los que usamos: Wonder `fastmem=off`; Paper Mario `fastmem=off`, `fps=30`; Zelda BotW `env=NX_JIT_CACHE_MIB=64` y `astc_recompress=bc1` (sin probar todavía; `ORBIS_ARENA_MIB=1152` empeoró).
- `fps=30` es para juegos que no llegan a 60 (3D World); `fps=auto` lo quita.
- `fastmem_partial=off` si algún juego se cuelga con jitcache18 (vuelve a la regla de jitcache16).
- `language=es-419` es español latino; para español de España: `language=es` y `region=europe`.
- Combinaciones que **fallaron**: caché del JIT a 64/128 sin el arreglo del heap (se congela); `ORBIS_ARENA_MIB=1792` (deja sin memoria al resto); `NX_RECLAIM_LOW_MIB` (el presupuesto que da el driver siempre es 0).
- Desde jitcache12: `env=NX_PRESENT_ORDER=auto|swap|off` (`auto` por defecto = colores correctos; `off` = como antes, rojo y azul cambiados).

---

## Instalar

1. Descarga el `.pkg` de la versión que quieras en [Releases](../../releases). Recomendado: **jitcache21** (ajustes por juego y sin el cierre al arrancar). Antes: **jitcache18** (fastmem; 3D World a 30 FPS) o **jitcache12** (colores arreglados, sin fastmem).
2. Instálalo con tu instalador de PKG habitual. Se instala como `EDPS00001` y reemplaza al Eden anterior.
3. Pon en la PS4 tus claves (`/data/edenps4/keys`), tu firmware (`/data/edenps4/firmware`, archivos `.nca`) y tus juegos (`/data/edenps4/roms`), igual que con el original.
4. Copia el `settings.txt` de arriba.

---

## Leer los registros

En la PS4 (por FTP): `/data/edenps4/boot.log`, `mesa.log` y `log/eden_log.txt`.
- La primera línea de `boot.log` dice qué versión corre.
- Cada 10 s: `status: game X fps … direct free … jit cache flushes a/b/c/d` (FPS, memoria libre y vaciados de la caché del JIT).
- Cada 30 s: `profile [CPUCore_0]: busy X% (jit Y%)`: si un núcleo va al ~95 % con jit alto, el límite es la CPU.
- `mesa.log` → `BUDGET … waited N ms for the GPU`: si es ~0 %, la GPU no es el cuello de botella.
- Con fastmem (jitcache14+): `guest layout: … heap 0x… ; fastmem covers below 0x200000000` (el heap tiene que quedar por debajo) y, cada 10 s, `fastmem: view 8 GiB … N pages mapped … M slow-path redirects` con `redirects by cause:`. Muchas páginas mapeadas y redirecciones que dejan de subir = fastmem trabajando.

[`registros/`](registros/) guarda las sesiones reales de cada versión:
- `eden-propio/`: nuestro Eden (Wonder, Paper Mario y 3D World).
- `eden4-xatyy/`: la prueba de Eden4 1.0. Arranca en fw 10.50, pero Wonder se aborta dentro del juego (`nn::oe::EnableGamePlayRecording`). Zelda BotW se cierra a los ~3 min sin memoria (`20261101_zelda`).

---

## Pendientes

1. **Wonder con fastmem**: los fallos de fastmem no dejan de subir (~240.000 en 90 s; en 3D World se quedan en ~91.000) y cada uno recompila un bloque; a los 93 s el heap del emulador crece 640 MiB de golpe (la mayor petición, 107 MiB, es una tabla hash enorme). HIPÓTESIS: estructuras de dynarmic que crecen con cada recompilación. Mientras tanto, `fastmem=off` para Wonder (ajuste por juego).
2. **Zelda BotW sin memoria**: probar `astc_recompress=bc1`; modo portátil o resolución menor por juego (menos memoria de GPU); la marca de modo Pro en `param.sfo` (hoy `ATTRIBUTE=0`; HIPÓTESIS: daría la GPU completa y quizá más memoria, pero el autor de Eden4 dice que en modo Pro se le colgaba la consola).
3. **Paper Mario se congela al empezar partida con jitcache20+**: probar `regalloc=off` (Triángulo → Registros de Eden4: No) para saber si es el parche 0044.
4. **Más FPS en la CPU**: portar lo que falta del JIT de Eden4 (tabla de páginas directa de dos niveles y reutilizar la búsqueda de página dentro de un bloque; solo sirve sin fastmem).
5. Paper Mario: cinemática a 0,2–0,5 FPS con un núcleo en espera activa.
6. Metroid Dread: averiguar quién pide 1,8 GiB al cargar (sigue igual con jitcache18).
7. Menú dentro del juego con L3+R3 (como Crema) para cambiar ajustes sin salir.

## Créditos

- **alechurri**: NX on Orbis, el port original y todas sus herramientas, incluida la base de fastmem (parches 0036–0039).
- **Eden Emulator Project** / yuzu: el emulador.
- **xatyy**: [Eden4](https://github.com/xatyy/Eden4), del que se adaptaron el reintento de memoria de la GPU y el reparto de registros de dynarmic.
- **Valdivia737**: pruebas en la consola, decisiones y este fork.
- **Claude** (Anthropic): análisis de registros, parches y esta documentación.
